#!/usr/bin/env python3
"""Runs build/Arashi.prc on a test matrix device in Frond.

Boots the device's firmware through its setup recipe once and keeps the
launcher session in build/frond/DEVICE/ (it holds the firmware, so it stays
out of the repo). Frond has no setup recipe for the Zodiac, so zodiac_setup
here goes through its Welcome wizard, and its PRC goes in on an SD card.
Each run starts from that session, installs the PRC, launches the app,
plays the replay (timed in guest ms from the launch), waits --seconds and
saves the screen as OUTDIR/end.png. --rotate sends Frond's rotate command,
which on the ARM devices only turns the host's view; build with
EXTRA=-DTEST_ROTATE to turn the guest.

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
    'tx': dict(device='palm-tx', nor=IMG / 'Palm/TX/Palm-TX-5.4-nor.bin', nand=IMG / 'Palm/TX/Palm-TX-5.4-nand.bin'),
    # Frond's install and launch don't answer on the Zodiac yet, so the PRC goes on
    # an SD card; Home opens the card's view with it as the first icon.
    'zodiac': dict(device='tapwave-zodiac-2', nor=IMG / 'Tapwave/Zodiac-2/Tapwave-Zodiac-2-v1.1.rom',
                   engine='fast', card_tap=(140, 75)),
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


def dark_near(rows, x, y, r=24):
    """Dark pixels in a box around x, y (a calibration target shows as 40 or more)."""
    if rows is None:
        return 0
    h, w = len(rows), len(rows[0]) // 4
    return sum(1 for yy in range(max(y - r, 0), min(y + r + 1, h)) for xx in range(max(x - r, 0), min(x + r + 1, w))
               if sum(rows[yy][4 * xx:4 * xx + 3]) < 200)


def zodiac_setup(core, root):
    """The Zodiac's Welcome wizard to the Home screen, after Frond's Zodiac
    notes (doc/step-zodiac-finish-plan.md): touch calibration (tapping each
    target where it shows), stick calibration, then Next through the rest.
    Landscape pen coordinates, a frame after each step in root/zodiac-N.png."""
    step = [0]

    def shot():
        step[0] += 1
        return core.frame(root / f'zodiac-{step[0]}.png')

    def stick(x, y, ms):
        core.replay(root, [f'0 stick {x} {y}', f'{ms} stick 0 0'])
        time.sleep(ms / 1000 + 0.5)

    time.sleep(60)          # cold boot to the Welcome screen
    shot()
    core.tap(root, 240, 160)
    # The target on the Step 1 page, then the three calibration targets.
    for n, (x, y) in enumerate(((355, 240), (20, 20), (459, 298), (240, 120))):
        core.wait_frame(root / f'target-{n}.png', lambda rows: dark_near(rows, x, y) >= 40, 90)
        time.sleep(1)
        core.tap(root, x, y)
    time.sleep(5)
    shot()
    core.tap(root, 440, 305)
    time.sleep(3)
    shot()
    core.key(root, '62', 3000)      # Function, held
    time.sleep(2)
    for x, y in ((0, -100), (0, 100), (-100, 0), (100, 0)):
        stick(x, y, 1500)
    time.sleep(1)
    for x, y in ((0, -100), (70, -70), (100, 0), (70, 70), (0, 100), (-70, 70), (-100, 0), (-70, -70)):
        stick(x, y, 600)
    time.sleep(2)
    shot()
    core.tap(root, 440, 305)
    time.sleep(3)
    shot()
    for _ in range(3):
        core.tap(root, 410, 305)
        time.sleep(3)
        shot()
    time.sleep(5)


SETUPS = {'zodiac': zodiac_setup}


def card_with(prc, card):
    """A 32 MB FAT card with the PRC in /PALM/Launcher (needs mtools)."""
    card.unlink(missing_ok=True)
    with card.open('wb') as f:
        f.truncate(32 << 20)
    subprocess.run(['mformat', '-i', str(card), '-v', 'ARASHI', '::'], check=True, capture_output=True)
    subprocess.run(['mmd', '-i', str(card), '::/PALM', '::/PALM/Launcher'], check=True)
    subprocess.run(['mcopy', '-i', str(card), str(prc), '::/PALM/Launcher/' + prc.name], check=True)


def image_args(d):
    if 'rom' in d:
        return ['--rom', d['rom'], '--device', d['device']]
    args = ['--device', d['device'], d['nor']]
    if 'nand' in d:
        args += ['--nand', d['nand']]
    if 'engine' in d:
        args += ['--engine', d['engine']]
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
        if key in SETUPS:
            SETUPS[key](core, folder)
            core.frame(folder / 'launcher.png')
        else:
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
    args = [session, '--device', d['device']] + (['--speed', a.speed] if is68k else []) \
        + (['--engine', d['engine']] if 'engine' in d else [])
    card = Path(session).parent / 'card.img'
    if 'card_tap' in d:
        card_with(a.prc.resolve(), card)
        args += ['--sd', card]
    core = palm_boot.Core(args, out / 'core.log')
    try:
        wait_started(core)
        if is68k:
            core.until('speed-state', 'speed:', 'speed', 30)
        else:
            time.sleep(5)
        if 'card_tap' not in d:
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
        if 'card_tap' in d:
            time.sleep(15)
            core.tap(out, *d['card_tap'])
            time.sleep(5)
        else:
            core.launch(a.app)
        if a.replay:
            # A bad line (a key name the core doesn't know, a pen point off its
            # surface) gives Frond's error instead of the loaded reply.
            reply = core.send(f'input-replay "{out / "input.replay"}"',
                              ('input replay loaded', 'invalid replay event', 'cannot open', 'refused'))
            if 'loaded' not in reply:
                raise SystemExit(f'replay: {reply.strip()}')
        time.sleep(max(a.seconds, 3))
        if is68k:
            core.send('speed-state', 'speed:')
        if a.rotate:
            core.frame(out / 'portrait.png')
            core.send('rotate', None if not is68k else 'done')
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
