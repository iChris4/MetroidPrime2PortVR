"""GPU smoke test for --options without a disc. Run with the built game executable."""
import os
from pathlib import Path
import subprocess
import sys
import time

exe = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
root.mkdir(parents=True, exist_ok=True)
pages = ["Game", "Controls", "Video", "Enhancements", "Mods", "Save states", "System", "Debug", "VR"]
for page in pages + ["sync"]:
    user = root / page.replace(" ", "-")
    user.mkdir(exist_ok=True)
    settings = user / "settings.cfg"
    settings.write_text("vr_enabled=1\nvr_required=1\ninfinite_beam_ammo=0\ndisc_path=missing-disc.iso\n", newline="\n")
    env = os.environ.copy()
    env.update(MP2_USER_PATH=str(user), MP2_VR="1", MP2_DISC="missing-disc.iso", MP2_NO_DISC_DIALOG="1",
               MP2_DEBUG_TAB="Enhancements" if page == "sync" else page,
               MP2_OPTIONS_FRAMES="120" if page == "sync" else "4", MP2_LOG_FILE="0")
    log = user / "options.log"
    with log.open("w") as output:
        proc = subprocess.Popen([str(exe), "--options"], env=env, stdout=output, stderr=subprocess.STDOUT)
        if page == "sync":
            deadline = time.monotonic() + 45
            while "options-only panel started" not in log.read_text(errors="replace"):
                if proc.poll() is not None or time.monotonic() > deadline:
                    proc.kill()
                    raise AssertionError("options panel did not start")
                time.sleep(.02)
            settings.write_text(settings.read_text().replace("infinite_beam_ammo=0", "infinite_beam_ammo=1"), newline="\n")
        try:
            result = proc.wait(timeout=60)
        except subprocess.TimeoutExpired:
            proc.kill()
            raise AssertionError("options panel did not exit")
    text = log.read_text(errors="replace")
    assert result == 0, (page, result, text[-3000:])
    assert "options-only panel started" in text, page
    assert "OpenXR session started" not in text and "gameplay reached" not in text, page
    assert "vr_enabled=1" in settings.read_text(), "opening flat options changed VR preference"
    if page == "sync":
        assert "infinite_beam_ammo=1 changed in the settings file, applied" in text, text[-3000:]
    print(page + ": passed", flush=True)
