#!/usr/bin/env python3
"""Exercise actual strict regulator and board telemetry methods with fake bus replies."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
def method(source,signature):
    start=source.index(signature);brace=source.index('{',start);depth,end=1,brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]+'\n'
with tempfile.TemporaryDirectory(prefix='5tfw-tuning-sensors-') as temporary:
    path=Path(temporary)
    base=(ROOT/'main/boards/drivers/TPS53647.cpp').read_text()
    rev=(ROOT/'main/boards/drivers/rev7/TPS546.cpp').read_text()
    board=(ROOT/'main/boards/board.cpp').read_text()
    nerd=(ROOT/'main/boards/nerdqaxeplus.cpp').read_text()
    qa=(ROOT/'main/boards/nerdqaxeplus2.cpp').read_text()
    octaxe=(ROOT/'main/boards/nerdoctaxegamma.cpp').read_text()
    base_bodies='\n'.join(method(base,s) for s in ('float TPS53647::slinear11_to_float(', 'bool TPS53647::readTuningTelemetry(', 'bool TPS53647::setTuningVoltage('))
    rev_bodies='\n'.join(method(rev,s) for s in ('static float slinear11_2_float(', 'static bool tuning_mode(', 'bool TPS546::readTuningTelemetry(', 'bool TPS546::setTuningVoltage('))
    board_bodies='\n'.join(method(board,s) for s in ('void Board::setChipTemp(', 'float Board::getChipTemp(', 'bool Board::readTuningChipTemperatures('))
    board_bodies+='\n'.join(method(nerd,s) for s in ('bool NerdQaxePlus::tuningAsicTemperature(', 'bool NerdQaxePlus::readTuningSensors(', 'bool NerdQaxePlus::setTuningVoltage('))
    board_bodies+=method(qa,'float NerdQaxePlus2::getTemperature(')+method(octaxe,'bool NerdOctaxeGamma::tuningAsicTemperature(')+method(octaxe,'bool NerdOctaxeGamma::refreshTuningTemperatures(')
    fan=(ROOT/'main/boards/drivers/EMC2302.cpp').read_text()
    fan_defines='\n'.join(re.findall(r'^#define EMC2302_\w+\s+0x[0-9a-fA-F]+', (ROOT/'main/boards/drivers/EMC2302.h').read_text(),re.M))
    fan_bodies=fan_defines+'\n'+method(fan,'esp_err_t EMC2302_get_fan_speed(')+method(fan,'bool EMC2302_read_tuning_fan(')
    fan_bodies+=method((ROOT/'main/boards/drivers/tmp451_mux.cpp').read_text(),'esp_err_t Tmp451Mux::select_channel(')
    text=(ROOT/'test/host/autotune_sensor_harness.cpp').read_text()
    text=text.replace('/* BASE_METHODS */',base_bodies).replace('/* REV_METHODS */',rev_bodies).replace('/* BOARD_METHODS */',board_bodies)
    text=text.replace('/* FAN_METHODS */',fan_bodies)
    (path/'sensor.cpp').write_text(text)
    binary=path/'sensor'
    subprocess.run([os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++'),'-std=c++17','-Wall','-Wextra','-Werror','-pthread',
        '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(ROOT/'main'),'-I',str(ROOT/'main/boards'),'-I',str(ROOT/'main/boards/drivers'),
        str(path/'sensor.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
