#!/usr/bin/env python3
"""Run a guest GDB command file against an isolated paused QEMU."""
import os
from pathlib import Path
import socket
import subprocess
import sys

LAB = Path(__file__).resolve().parents[2]
BUILD = LAB / '_local/first-fit'
script = Path(sys.argv[1]).resolve()
stem = sys.argv[2]
logs = LAB / 'report/member-a/logs'
logs.mkdir(parents=True, exist_ok=True)
with socket.socket() as sock:
    sock.bind(('127.0.0.1', 0))
    port = sock.getsockname()[1]
with (logs / f'{stem}-serial.txt').open('w') as serial:
    firmware = (['-L', os.environ['LAB2_QEMU_BIOS_DIR']]
                if 'LAB2_QEMU_BIOS_DIR' in os.environ else [])
    proc = subprocess.Popen([os.environ.get('LAB2_QEMU', 'qemu-system-riscv64'),
                             *firmware,
                             '-machine', 'virt', '-nographic', '-bios', 'default',
                             '-kernel', 'bin/ucore.img', '-S', '-gdb',
                             f'tcp:127.0.0.1:{port}'], cwd=BUILD,
                            stdout=serial, stderr=subprocess.STDOUT)
    try:
        with (logs / f'{stem}.txt').open('w') as log:
            subprocess.run(['gdb-multiarch', '-q', '-nx', '-batch', 'bin/kernel',
                            '-ex', 'set pagination off', '-ex', 'set confirm off',
                            '-ex', 'set tcp auto-retry on',
                            '-ex', 'set tcp connect-timeout 10',
                            '-ex', f'target remote 127.0.0.1:{port}',
                            '-x', str(script)], cwd=BUILD, stdout=log,
                           stderr=subprocess.STDOUT, timeout=35, check=True)
    finally:
        proc.terminate()
        proc.wait(timeout=5)
