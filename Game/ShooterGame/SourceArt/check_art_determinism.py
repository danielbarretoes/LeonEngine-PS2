"""Checks that the Blender source art scripts export the same bytes every time (Docs/ART_PIPELINE.md, ps2-shipping
N26's gate; plan decision D6: the scripts are the source of truth).

    python Game/ShooterGame/SourceArt/check_art_determinism.py [make_script.py ...]

Runs each script twice in Blender, headless (`--background --factory-startup`, `-- --out <temp folder>`), and compares
every .glb of the two runs byte for byte, then with the .glb of that name next to the script (the committed one).
Without arguments it checks every make_*.py under this folder that uses leon_art, and fails when one of the scripts
the art needs is missing from that list (EXPECTED_SCRIPTS: the skeleton and its clips, the CS 1.6 bodies, the arms,
the weapons, de_leon, de_puerto and the samples). The generators that need no Blender (PYTHON_SCRIPTS: the skies'
HDRs, ps2-polish P8) run twice with this Python (`<script> --out <temp folder>`) and their .hdr files are compared the same way. The .blend files are not
compared:
Blender writes different bytes on every save. Blender is LEON_BLENDER, else Blender 5.2's default install; the glTF
exporter writes its version in the file, so another Blender version is expected to differ from the committed files.
Exits 1 on any difference. Only the Python standard library is used.
"""

import glob
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
# The scripts that make ShooterGame's art (ps2-shipping N26, N27): every one must be found and checked.
EXPECTED_SCRIPTS = [
    os.path.join("Characters", "make_arms.py"),
    os.path.join("Characters", "make_characters.py"),
    os.path.join("Characters", "make_cs16_characters.py"),
    os.path.join("Maps", "make_de_leon.py"),
    os.path.join("Maps", "make_de_puerto.py"),
    os.path.join("Samples", "make_art_samples.py"),
    os.path.join("Weapons", "make_weapons.py"),
]
# The Python standard library's generators, run without Blender: the skies (Sky/make_sky.py: Sky_Desert.hdr and
# Sky_Coast.hdr).
PYTHON_SCRIPTS = [
    os.path.join("Sky", "make_sky.py"),
]
OUTPUTS = ("*.glb", "*.hdr")


def find_scripts():
    scripts = []
    for path in sorted(glob.glob(os.path.join(HERE, "**", "make_*.py"), recursive=True)):
        with open(path, encoding="utf-8") as source:
            if "import leon_art" in source.read():
                scripts.append(path)
    return scripts


def run(blender, script, out):
    if os.path.relpath(script, HERE) in PYTHON_SCRIPTS:
        command = [sys.executable, script, "--out", out]
    else:
        command = [blender, "--background", "--factory-startup", "--python", script, "--", "--out", out]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    if result.returncode != 0 or "Traceback" in result.stdout:
        sys.stdout.write(result.stdout)
        raise RuntimeError("%s failed in Blender" % os.path.relpath(script, HERE))


def read(path):
    with open(path, "rb") as file:
        return file.read()


def check(blender, script):
    """The failures of one script: its outputs between two runs, and against the committed files."""
    failures = []
    name = os.path.relpath(script, HERE)
    folders = [tempfile.mkdtemp(prefix="leon_art_"), tempfile.mkdtemp(prefix="leon_art_")]
    try:
        for folder in folders:
            run(blender, script, folder)
        outputs = sorted(os.path.basename(p) for pattern in OUTPUTS
                         for p in glob.glob(os.path.join(folders[0], pattern)))
        if not outputs:
            failures.append("%s: wrote no .glb or .hdr" % name)
        for output in outputs:
            first = read(os.path.join(folders[0], output))
            second_path = os.path.join(folders[1], output)
            if not os.path.exists(second_path) or read(second_path) != first:
                failures.append("%s: %s differs between two runs" % (name, output))
                continue
            committed = os.path.join(os.path.dirname(script), output)
            if not os.path.exists(committed):
                failures.append("%s: %s is not next to the script" % (name, output))
            elif read(committed) != first:
                failures.append("%s: %s differs from the committed file" % (name, output))
            else:
                print("%s: %s identical (%d bytes, twice and committed)" % (name, output, len(first)))
    finally:
        for folder in folders:
            shutil.rmtree(folder, ignore_errors=True)
    return failures


def main(argv):
    blender = os.environ.get("LEON_BLENDER", DEFAULT_BLENDER)
    if not os.path.exists(blender):
        print("check_art_determinism: Blender not found at %s (set LEON_BLENDER)" % blender)
        return 1
    scripts = [os.path.abspath(path) for path in argv] or (find_scripts() +
                                                           [os.path.join(HERE, name) for name in PYTHON_SCRIPTS])
    failures = []
    if not argv:
        found = {os.path.relpath(script, HERE) for script in scripts}
        failures.extend("%s: not found, or it does not use leon_art" % name for name in EXPECTED_SCRIPTS
                        if name not in found)
    for script in scripts:
        failures.extend(check(blender, script))
    for failure in failures:
        print("FAILED " + failure)
    print("check_art_determinism: %s (%d script(s))" % ("FAILED" if failures else "PASSED", len(scripts)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
