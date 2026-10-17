#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
for case in ('zero-size', 'wrong-base', 'overflow', 'no-kernel', 'no-descriptors',
             'no-free-page', 'partial-tail', 'clipped-upper'):
    env = dict(os.environ, LAB2_CASE=case)
    subprocess.run([sys.executable, str(HERE / 'capture_guest.py'),
                    str(HERE / 'boundaries.gdb'), 'boundary-' + case],
                   env=env, check=True)
    print('PASS ' + case, flush=True)
