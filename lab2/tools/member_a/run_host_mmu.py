#!/usr/bin/env python3
import os
from pathlib import Path
import signal
import subprocess

LAB = Path(__file__).resolve().parents[2]
source = LAB / '_local/qemu-4.1.1'
qemu = LAB / '_local/qemu-build/riscv64-softmmu/qemu-system-riscv64'
script = Path(__file__).with_name('host_mmu.py')
log = LAB / 'report/member-a/logs/host-mmu.txt'
log.parent.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, LAB2_QEMU_SOURCE=str(source))
with log.open('w') as output:
    proc = subprocess.Popen(['gdb', '-q', '-nx', '-batch', '-x', str(script), '--args',
                    str(qemu), '-machine', 'virt', '-m', '128M', '-nographic',
                    '-L', str(source / 'pc-bios'), '-bios', 'default',
                    '-kernel', 'bin/ucore.img'], cwd=LAB / '_local/first-fit',
                   stdout=output, stderr=subprocess.STDOUT, env=env,
                   start_new_session=True)
    try:
        if proc.wait(timeout=240) != 0:
            raise RuntimeError('Host GDB failed; inspect host-mmu.txt')
    finally:
        # Include the inferior QEMU if a failing/timed-out GDB leaves it alive.
        try:
            os.killpg(proc.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        proc.wait(timeout=5)
text = log.read_text()
assert 'HOST_MMU_PASS' in text
print('PASS real Bare/Sv39 QEMU source stepping and software TLB evidence')
