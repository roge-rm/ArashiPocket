#!/usr/bin/env python3
"""Runs build/Arashi.prc on a test matrix device in Frond.

Boots the device's firmware through its setup recipe once and keeps the
launcher session in build/frond/DEVICE/ (it holds the firmware, so it stays
out of the repo). Each run starts from that session, installs the PRC,
launches the app, plays the replay (timed in guest ms from the launch),
waits --seconds and saves the screen as OUTDIR/end.png. A replay line
"rotate" sends Frond's rotate command, at the time given.

Usage: frond-device.py DEVICE OUTDIR [--replay R] [--seconds N]
       [--speed original|uncapped] [--prc PRC] [--app NAME]
       [--export-db NAME] [--rotate] [--frame NAME]
       frond-device.py --list
--speed is for the 68k devices only. Frond's palm_boot takes the heavy
emulator slot itself, so don't wrap this in heavy. Run one device at a time.
Adapted from PalmBolo's tools/frond-iiix.py and frond-tg50.py.
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
FROND = Path.home() / 'Projects/palm/frond'
sys.path.insert(0, str(FROND / 'tools'))
import palm_boot  # noqa: E402

IMG = Path('/srv/downloads/backup/deviceimages/palm')
TXDIR = Path('/srv/downloads/temp/debug/palm/frond')

# name: Frond device, firmware (ROM for 68k, NOR [and NAND] for ARM), session folder.
DEVICES = {
    'iiix': dict(device='palm-iiix', rom=IMG / 'Palm/IIIx/Palm-IIIx-3.1-en.rom', folder='build/iiix'),
    'm100': dict(device='palm-m100', rom=IMG / 'Palm/m100/Palm-m100-3.51-en.rom'),
    'vx': dict(device='palm-vx', rom=IMG / 'Palm/Vx/Palm-Vx-4.1-en.rom'),
    'iiic': dict(device='palm-iiic', rom=IMG / 'Palm/IIIc/Palm-IIIc-4.1-en.rom'),
    'm505': dict(device='palm-m505', rom=IMG / 'Palm/m505/Palm-m505-4.1-en.rom'),
    't415': dict(device='sony-clie-t415', rom=IMG / 'Sony/CLIE-T400/Sony-Clie-T400.rom'),
    'dana': dict(device='alphasmart-dana', rom=IMG / 'AlphaSmart/Dana/AlphaSmart-Dana-1.5.rom'),
    'tg50': dict(device='sony-clie-tg50', nor=IMG / 'Sony/CLIE-TG50/Sony-Clie-TG50-nor.bin'),
    'e2': dict(device='palm-e2', nor=IMG / 'Palm/Tungsten-E2/Palm-Tungsten-E2-nor.bin',
               nand=IMG / 'Palm/Tungsten-E2/Palm-Tungsten-E2-nand.bin'),
    'tx': dict(device='palm-tx', nor=TXDIR / 'Palm-TX-5.4-nor.bin', nand=TXDIR / 'tx-calibrated.bin'),
    'zodiac': dict(device='tapwave-zodiac-2', rom=IMG / 'Tapwave/Zodiac-2/Tapwave-Zodiac-2-v1.1.rom'),
}


def wait_started(core, timeout=900):
    """The core may wait for a free emulator slot first (heavy). It is running once a
    frond core process is among the children of heavy; its output is buffered, so the
    log can't tell."""
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if core.process.poll() is not None:
            raise SystemExit('core exited: ' + ''.join(core.lines[-3:]).strip())
        ps = subprocess.run(['ps', '-eo', 'pid,ppid,args'], capture_output=True, text=True).stdout
        kids = {core.process.pid}
        rows = [l.split(None, 2) for l in ps.splitlines()[1:]]
        for _ in range(4):
            kids |= {int(r[0]) for r in rows if len(r) == 3 and int(r[1]) in kids}
        for r in rows:
            if len(r) == 3 and int(r[0]) in kids and '/out/frond' in r[2].split(None, 1)[0] + ' ' \
                    and not r[2].startswith(('heavy', '/bin/bash', 'flock')):
                return
        time.sleep(1)
    raise SystemExit('no emulator slot after %d s' % timeout)


def image_args(d):
    if 'rom' in d:
        return ['--rom', d['rom'], '--device', d['device']]
    args = ['--device', d['device'], d['nor']]
    if 'nand' in d:
        args += ['--nand', d['nand']]
    return args


def session_for(key, d):
    """The launcher session for a device, booting through setup the first time."""
    folder = ROOT / d.get('folder', f'build/frond/{key}')
    folder.mkdir(parents=True, exist_ok=True)
    stem = Path(d.get('rom') or d['nor']).stem
    session = folder / (stem + '-launcher.frond')
    machine = folder / (stem + '-launcher')
    if machine.is_dir():
        return machine
    if session.is_file():
        return session
    core = palm_boot.Core(image_args(d), folder / 'boot-log.txt')
    try:
        wait_started(core)
        palm_boot.setup_device(d['device'], core, folder)
        core.wait_frame(folder / 'launcher.png', palm_boot.launcher_check(d['device']), 60)
        core.send(f'save-session "{session}"', 'wrote session', 60)
    finally:
        core.close()
        for f in folder.glob('console-*.replay'):
            f.unlink()
    return machine if machine.is_dir() else session


def main():
    p = argparse.ArgumentParser()
    p.add_argument('device', nargs='?')
    p.add_argument('out', nargs='?', type=Path)
    p.add_argument('--list', action='store_true')
    p.add_argument('--replay', type=Path)
    p.add_argument('--seconds', type=float, default=0)
    p.add_argument('--speed', default='original', choices=['original', 'uncapped'])
    p.add_argument('--prc', type=Path, default=ROOT / 'build/Arashi.prc')
    p.add_argument('--app', default='Arashi')
    p.add_argument('--export-db', action='append', default=[], metavar='NAME')
    p.add_argument('--rotate', action='store_true', help='send rotate after the launch and take rotated.png')
    p.add_argument('--frame', default='end.png', help='name of the final screenshot in OUTDIR')
    a = p.parse_args()
    if a.list:
        for key, d in DEVICES.items():
            fw = d.get('rom') or d['nor']
            print(f"{key:8} {d['device']:18} {'ok' if fw.is_file() else 'MISSING'} {fw}")
        return
    if a.device not in DEVICES or not a.out:
        p.error('DEVICE (see --list) and OUTDIR needed')
    d = DEVICES[a.device]
    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    session = session_for(a.device, d)
    is68k = d['device'] in palm_boot.DEVICES_68K
    args = [session, '--device', d['device']] + (['--speed', a.speed] if is68k else [])
    core = palm_boot.Core(args, out / 'core.log')
    try:
        wait_started(core)
        if is68k:
            core.until('speed-state', 'speed:', 'speed', 30)
        else:
            time.sleep(5)
        reply = core.send(f'install "{a.prc.resolve()}"', 'installation', 120)
        if 'successful' not in reply:
            raise SystemExit(f'install: {reply.strip()}')
        if a.replay:
            # Snapshot paths in the replay are relative to OUTDIR.
            lines = []
            for line in a.replay.read_text().splitlines():
                w = line.split()
                if len(w) >= 3 and w[1] == 'snapshot':
                    line = f'{w[0]} snapshot "{out / w[2].strip(chr(34))}"'
                lines.append(line)
            (out / 'input.replay').write_text('\n'.join(lines) + '\n')
        core.launch(a.app)
        if a.replay:
            core.send(f'input-replay "{out / "input.replay"}"', 'input replay loaded')
        time.sleep(max(a.seconds, 3))
        if is68k:
            core.send('speed-state', 'speed:')
        if a.rotate:
            core.frame(out / 'portrait.png')
            core.send('rotate', 'done')
            time.sleep(6)
            core.frame(out / 'rotated.png')
        else:
            core.frame(out / a.frame)
        for name in a.export_db:
            pdb = out / f'{name}.pdb'
            pdb.unlink(missing_ok=True)
            reply = core.send(f'db-export-db "{name}" "{pdb}"', ('export written', 'export failed'), 60)
            if not pdb.is_file():
                print(f'export of {name} failed: {reply.strip()}')
    finally:
        core.close()
        (out / 'input.replay').unlink(missing_ok=True)
        for f in out.glob('console-*.replay'):
            f.unlink()


if __name__ == '__main__':
    main()
