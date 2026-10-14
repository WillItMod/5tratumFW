#!/usr/bin/env python3
"""Actual LVGL candidate renders and extracted production lifecycle checks."""
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / '.cache/candidate-native'
OUTPUT = BUILD / 'output'

def run():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    flags = '-fsanitize=address,undefined -fno-omit-frame-pointer'
    with (BUILD / 'configure.log').open('w') as log:
        subprocess.run(['cmake', '-S', str(ROOT / 'test/host/candidate_native'), '-B', str(BUILD),
                        '-DCMAKE_BUILD_TYPE=Debug', f'-DCMAKE_C_FLAGS={flags}',
                        f'-DCMAKE_CXX_FLAGS={flags}', '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'],
                       check=True, stdout=log, stderr=subprocess.STDOUT)
    with (BUILD / 'build.log').open('w') as log:
        subprocess.run(['cmake', '--build', str(BUILD), '--target', 'candidate-native', '-j', '6'],
                       check=True, stdout=log, stderr=subprocess.STDOUT)
    reports = []
    for width, height in ((320, 170), (480, 320)):
        result = subprocess.run([str(BUILD / 'candidate-native'), str(OUTPUT), str(width), str(height)],
                                text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(result.stderr or result.stdout)
        reports.append(json.loads(result.stdout))
    try:
        from PIL import Image
        for image in OUTPUT.glob('*.ppm'):
            Image.open(image).save(image.with_suffix('.png'))
    except ImportError:
        pass
    report = {'passed': True, 'nativeLvgl': '8.3.11', 'renders': reports,
              'productionSources': json.loads((BUILD / 'candidate-source.json').read_text()),
              'scope': 'Host rendering/lifecycle only; no physical display, GPIO or thermal fault qualification'}
    (OUTPUT / 'candidate-native-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    run()
