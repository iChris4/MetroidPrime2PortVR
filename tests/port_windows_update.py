# SPDX-License-Identifier: MIT
"""Exercise the actual Windows updater (launcher/update-windows.ps1) only in disposable fixture
installations under build/updater-fixtures. From PrimedGun v2's launcher tests."""
import ctypes
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

root = Path(__file__).resolve().parents[1]
fixtures = root / 'build' / 'updater-fixtures'
fixtures.mkdir(parents=True, exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='run-', dir=fixtures)).resolve()
assert work.is_relative_to(fixtures.resolve())
shell = Path(os.environ['SystemRoot']) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
script = root / 'launcher/update-windows.ps1'
def run(case, entries, good_hash=True, locked=False):
    folder = work / case
    target = folder / 'install'
    stage = folder / 'stage'
    target.mkdir(parents=True); stage.mkdir()
    (target/'user').mkdir()
    (target/'user/save.raw').write_bytes(b'player save')
    (target/'PrimedGunEchoes.exe').write_bytes(b'old launcher')
    (target/'metroid_prime2_port.exe').write_bytes(b'old game')
    (target/'z.dll').write_bytes(b'old dll')
    archive = stage/'update.zip'
    with zipfile.ZipFile(archive,'w') as z:
        for name, data in entries.items(): z.writestr(name, data)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest() if good_hash else '0'*64
    handle = None
    if locked:
        create = ctypes.windll.kernel32.CreateFileW
        create.argtypes = [ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
        create.restype = ctypes.c_void_p
        handle = create(str(target/'z.dll'),0x80000000,0,None,3,0,None)
        assert handle not in (None,ctypes.c_void_p(-1).value)
    try:
        result = subprocess.run([str(shell),'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(script),
            '-Archive',str(archive),'-Destination',str(target),'-LauncherPid','2147483646',
            '-ExpectedHash',digest,'-NoRestart','-Quiet'],capture_output=True,text=True,timeout=45)
    finally:
        if handle:
            ctypes.windll.kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
            ctypes.windll.kernel32.CloseHandle(handle)
    assert (target/'user/save.raw').read_bytes() == b'player save'
    if case=='success':
        assert result.returncode == 0, result.stderr
        assert (target/'PrimedGunEchoes.exe').read_bytes()==b'new launcher'
        assert (target/'metroid_prime2_port.exe').read_bytes()==b'new game'
        assert (stage/'backup/PrimedGunEchoes.exe').read_bytes()==b'old launcher'
        assert not (target/'user/injected.txt').exists()
    else:
        assert result.returncode != 0, (case,result.stdout,result.stderr)
        assert (target/'PrimedGunEchoes.exe').read_bytes()==b'old launcher', case
        assert (target/'metroid_prime2_port.exe').read_bytes()==b'old game', case
        assert (target/'z.dll').read_bytes()==b'old dll', case
        assert not (target/'a-new.txt').exists(), case
    print(case, 'passed')
entries={'release/PrimedGunEchoes.exe':b'new launcher','release/metroid_prime2_port.exe':b'new game',
         'release/a-new.txt':b'new file','release/z.dll':b'new dll','release/user/injected.txt':b'never install'}
run('success',entries)
run('bad-checksum',entries,good_hash=False)
run('traversal',dict(entries,**{'../outside.txt':b'bad'}))
run('user-alias',dict(entries,**{'release/user./save.raw':b'bad'}))
run('wrong-package',{'release/README.txt':b'no executable'})
run('rollback-locked-file',entries,locked=True)
assert not (work/'traversal/outside.txt').exists()
print('All Windows updater integration checks passed. Fixtures:', work)
