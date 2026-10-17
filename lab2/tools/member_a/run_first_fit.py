#!/usr/bin/env python3
"""Build an isolated First-Fit baseline without editing B's unfinished module."""
import os
from pathlib import Path
import shutil
import subprocess

LAB = Path(__file__).resolve().parents[2]
BUILD = LAB / '_local/first-fit'
LOGS = LAB / 'report/member-a/logs'
LOGS.mkdir(parents=True, exist_ok=True)
BUILD.mkdir(parents=True, exist_ok=True)
shutil.copytree(LAB / 'code', BUILD, dirs_exist_ok=True,
                ignore=shutil.ignore_patterns('obj', 'bin', 'best_fit_pmm.c'))
with (LOGS / 'first-fit-build.txt').open('w') as log:
    subprocess.run(['make', '-B', '-j4'], cwd=BUILD, stdout=log,
                   stderr=subprocess.STDOUT, check=True)
qemu = os.environ.get('LAB2_QEMU', 'qemu-system-riscv64')
firmware = (['-L', os.environ['LAB2_QEMU_BIOS_DIR']]
            if 'LAB2_QEMU_BIOS_DIR' in os.environ else [])
prefix = os.environ.get('LAB2_LOG_PREFIX', 'first-fit')
for memory in ('64M', '128M', '256M'):
    with (LOGS / f'{prefix}-{memory}.txt').open('w') as log:
        proc = subprocess.Popen([qemu, *firmware, '-machine', 'virt', '-m', memory,
                                 '-nographic', '-bios', 'default',
                                 '-kernel', 'bin/ucore.img'], cwd=BUILD,
                                stdout=log, stderr=subprocess.STDOUT)
        try:
            proc.wait(timeout=8)
        except subprocess.TimeoutExpired:
            proc.terminate()
            proc.wait(timeout=5)
    serial = (LOGS / f'{prefix}-{memory}.txt').read_text()
    if 'check_alloc_page() succeeded!' not in serial:
        raise RuntimeError(f'First-Fit boot/check failed ({memory}); inspect logs')
    print(f'PASS First-Fit {memory}')
