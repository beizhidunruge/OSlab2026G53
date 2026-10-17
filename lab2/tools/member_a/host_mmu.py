"""Source from host GDB; follows real guest accesses, never calls MMU artificially."""
import gdb
import os
from pathlib import Path

SOURCE = Path(os.environ['LAB2_QEMU_SOURCE'])
BARE = int(os.environ.get('LAB2_BARE_ADDR', '0x80206000'), 0)
VIRTUAL = int(os.environ.get('LAB2_VIRTUAL_ADDR', '0xffffffffc0206018'), 0)
PAGE_MASK = ((1 << 64) - 1) ^ 4095


def n(expr):
    return int(gdb.parse_and_eval(expr)) & ((1 << 64) - 1)


def dump(*names):
    for name in names:
        try:
            print('%s=%#x' % (name, n(name)))
        except gdb.error:
            print('%s=<not available>' % name)


def source_line():
    sal = gdb.selected_frame().find_sal()
    if not sal.symtab or not sal.line:
        return ''
    return Path(sal.symtab.fullname()).read_text().splitlines()[sal.line - 1].strip()


def walk(label, expected):
    print('\n=== %s: real get_physical_address call ===' % label)
    gdb.execute('bt 7')
    dump('addr', 'access_type', 'mmu_idx', 'env->satp', 'env->priv')
    pa_pointer = n('physical')
    pte_seen = False
    for step in range(200):
        if gdb.newest_frame().name() != 'get_physical_address':
            break
        line = source_line()
        if 'if (!(pte & PTE_V))' in line:
            dump('levels', 'ptidxbits', 'ptesize', 'base', 'i', 'ptshift',
                 'idx', 'pte_addr', 'pte', 'ppn')
            assert n('i') == 0 and n('idx') == 511
            assert n('pte') == 0x200000cf
            pte_seen = True
        gdb.execute('next')
    else:
        raise RuntimeError('MMU walk did not return')
    physical_page = n('*(unsigned long long *)%d' % pa_pointer)
    assert physical_page == expected
    assert pte_seen == (label == 'SV39')
    print('HOST_WALK_PASS %s returned_PA=%#x' % (label, physical_page))
    if label == 'SV39':
        print('FULL_PA=%#x' % (physical_page | (VIRTUAL & 4095)))


class TargetWalk(gdb.Breakpoint):
    def __init__(self, address):
        super().__init__('get_physical_address', internal=False)
        self.condition = 'addr == %d && access_type == 1 && mmu_idx == 1' % address


def fill(address):
    bp = gdb.Breakpoint('tlb_set_page_with_attrs')
    bp.condition = 'vaddr == %d && mmu_idx == 1' % (address & PAGE_MASK)
    gdb.execute('continue')
    print('=== Software TLB fill (also used in Bare) ===')
    dump('vaddr', 'paddr', 'prot', 'mmu_idx', 'size')
    expected_pa = address if address == BARE else address - 0xffffffff40000000
    assert n('paddr') == (expected_pa & PAGE_MASK) and n('size') == 4096
    gdb.execute('bt 5')
    # Step until the new cache entry has been copied, then inspect it.
    for step in range(150):
        if 'tlb_n_used_entries_inc(env, mmu_idx)' in source_line():
            dump('index', 'te->addr_read', 'te->addr_write', 'te->addr_code', 'te->addend')
            break
        gdb.execute('next')
    else:
        raise RuntimeError('TLB fill milestone not reached')
    bp.delete()


gdb.execute('set pagination off')
gdb.execute('set confirm off')
gdb.execute('set print pretty on')
gdb.execute('handle SIGPIPE nostop noprint pass')
gdb.execute('handle SIGUSR1 nostop noprint pass')
first = TargetWalk(BARE)
gdb.execute('run')
first.delete()
walk('BARE', BARE)
fill(BARE)
second = TargetWalk(VIRTUAL)
gdb.execute('continue')
second.delete()
walk('SV39', (VIRTUAL - 0xffffffff40000000) & PAGE_MASK)
fill(VIRTUAL)

lines = (SOURCE / 'accel/tcg/cputlb.c').read_text().splitlines()
lookup_line = next(i+1 for i,line in enumerate(lines) if 'if (unlikely(!tlb_hit(entry->addr_code, addr)))' in line)
lookup = gdb.Breakpoint('accel/tcg/cputlb.c:%d' % lookup_line)
lookup.condition = 'addr >= 0xffffffffc0200000 && addr < 0xffffffffc0203000'
for _ in range(3):
    gdb.execute('continue')
    print('=== C instruction-fetch software-TLB lookup ===')
    dump('addr', 'mmu_idx', 'index', 'entry->addr_code', 'entry->addend')
    assert (n('entry->addr_code') & PAGE_MASK) == (n('addr') & PAGE_MASK)
    gdb.execute('next')
    assert 'TLB_RECHECK | TLB_MMIO' in source_line()
    print('TLB_HIT_PASS: skipped refill branch')
lookup.delete()
print('HOST_MMU_PASS')
gdb.execute('kill')
