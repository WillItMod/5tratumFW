#!/usr/bin/env python3
"""Build and render production OLED layout with actual LVGL9, without ESP/hardware."""
from pathlib import Path
import json
import os
import subprocess
import shutil
import re
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[2]
BUILD=Path(os.environ.get("FIVETRATUM_OLED_BUILD", str(ROOT/".cache/oled-native"))).resolve()
LVGL_SOURCE=Path(os.environ.get("FIVETRATUM_OLED_LVGL_SOURCE", str(ROOT/"managed_components/lvgl__lvgl"))).resolve()
LVGL_VERSION_HEADER=(LVGL_SOURCE/"lv_version.h").read_text()
LVGL_VERSION=".".join(re.search(rf"^#define LVGL_VERSION_{part}\s+(\d+)", LVGL_VERSION_HEADER, re.M).group(1)
                      for part in ("MAJOR", "MINOR", "PATCH"))
OUT=BUILD/"output"
OUT.mkdir(parents=True,exist_ok=True)
cmake = shutil.which("cmake")
if not cmake:
    raise SystemExit("CMake is required on PATH")
env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",UBSAN_OPTIONS="halt_on_error=1")
subprocess.run([cmake,"-S",str(ROOT/"test/host/oled_native"),"-B",str(BUILD),
                "-DCMAKE_BUILD_TYPE=Debug",f"-DFIVETRATUM_OLED_LVGL_SOURCE={LVGL_SOURCE}"],check=True,env=env)
subprocess.run([cmake,"--build",str(BUILD),"-j","8"],check=True,env=env)
result=subprocess.run([str(BUILD/"oled_native"),str(OUT)],check=True,capture_output=True,text=True,env=env)
print(result.stdout)
runtime=subprocess.run([str(BUILD/"oled_screen_runtime")],check=True,capture_output=True,text=True,env=env)
print(runtime.stdout)
images=[]
for pgm in sorted(OUT.glob("*.pgm")):
    image=Image.open(pgm)
    assert image.size==(128,32)
    png=pgm.with_suffix(".png")
    image.save(png)
    images.append({"name":pgm.stem,"path":str(png.relative_to(OUT)),"width":128,"height":32})
selected=["boot","rate","health","shares","mux-connected","operating","wifi","paused",
          "candidate-01","candidate-02","candidate-03","candidate-04","candidate-05","candidate-06",
          "candidate-motion-2","candidate-motion-3","identify","overheat","setup","unavailable"]
montage=Image.new("RGB",(540,len(selected)*164+44),"#101820")
draw=ImageDraw.Draw(montage)
draw.text((14,10),"SIMULATED READINGS - actual native LVGL 128 x 32",fill="white")
for index,name in enumerate(selected):
    y=44+index*164
    draw.text((14,y),name.replace("-"," ").upper(),fill="#80d6e8")
    montage.paste(Image.open(OUT/f"{name}.png").convert("RGB").resize((512,128),Image.Resampling.NEAREST),(14,y+22))
montage.save(OUT/"oled-simulated-contact-sheet.png")
donuts=Image.new("RGB",(1080,3*164+44),"#101820")
donut_draw=ImageDraw.Draw(donuts)
donut_draw.text((14,10),"SIMULATED CANDIDATES - actual native LVGL 128 x 32, enlarged 4x",fill="white")
for index in range(6):
    name=f"candidate-{index+1:02d}"
    x=14+(index%2)*540
    y=44+(index//2)*164
    donut_draw.text((x,y),f"CANDIDATE {index+1}",fill="#80d6e8")
    donuts.paste(Image.open(OUT/f"{name}.png").convert("RGB").resize((512,128),Image.Resampling.NEAREST),(x,y+22))
donuts.save(OUT/"oled-candidate-contact-sheet.png")
(OUT/"oled-native-report.json").write_text(json.dumps({
    "fixtureOnly":True,"hardwareCalls":False,"runtimeDependencies":"Actual LVGL9 and production portfolio font",
    "lvglVersion":LVGL_VERSION,
    "targetConfiguration":"main/lv_conf.h; host only substitutes OS NONE and libc allocator",
    "sanitizers":["ASan","UBSan"],"views":images,"stdout":result.stdout+runtime.stdout,
},indent=2)+"\n")
print(OUT/"oled-simulated-contact-sheet.png")
