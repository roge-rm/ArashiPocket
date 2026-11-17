#!/usr/bin/env python3
"""Runs build/Arashi.prc on a Palm IIIx in Frond: frond-device.py with the
device fixed to iiix, same options as before (OUTDIR first)."""
import runpy
import sys
from pathlib import Path

sys.argv = [sys.argv[0], 'iiix'] + sys.argv[1:]
runpy.run_path(str(Path(__file__).with_name('frond-device.py')), run_name='__main__')
