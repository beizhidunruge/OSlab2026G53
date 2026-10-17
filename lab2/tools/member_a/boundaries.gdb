hbreak pmm_init
continue
python
import os
case = os.environ['LAB2_CASE']
cases = {
 'zero-size': (0x80000000, 0, True),
 'wrong-base': (0x80001000, 0x8000000, True),
 'overflow': (0x80000000, 0xffffffffffffffff, True),
 'no-kernel': (0x80000000, 0x100000, True),
 'no-descriptors': (0x80000000, 0x208000, True),
 'no-free-page': (0x80000000, 0x20d000, False),
 'partial-tail': (0x80000000, 0x400007b, False),
 'clipped-upper': (0x80000000, 0x10000000, False),
}
base, size, expected_panic = cases[case]
gdb.execute("set var 'dtb.c'::memory_base = %d" % base)
gdb.execute("set var 'dtb.c'::memory_size = %d" % size)
calls = []
class MapCall(gdb.Breakpoint):
    def stop(self):
        calls.append(int(gdb.parse_and_eval('$a1')))
        return False
MapCall('*default_init_memmap')
panic_bp = gdb.Breakpoint('__panic')
done_bp = gdb.Breakpoint('default_check')
gdb.execute('continue')
panicked = gdb.newest_frame().name() == '__panic'
assert panicked == expected_panic, 'Unexpected panic outcome'
if panicked:
    assert not calls, 'Invalid input registered memory'
    gdb.execute('x/s $a2')
else:
    actual_upper = int(gdb.parse_and_eval('npage')) * 4096
    assert actual_upper == min(base + size, 0x88000000) // 4096 * 4096
    page_size = int(gdb.parse_and_eval('sizeof(struct Page)'))
    count = int(gdb.parse_and_eval('npage - nbase'))
    desc_pa = (int(gdb.parse_and_eval('(unsigned long)pages')) - 0xffffffff40000000) & ((1<<64)-1)
    free_start = (desc_pa + count * page_size + 4095) // 4096 * 4096
    expected_free = max(0, (actual_upper - free_start) // 4096)
    free_count = int(gdb.parse_and_eval("'default_pmm.c'::free_area.nr_free"))
    assert free_count == expected_free, 'free count mismatch: %d != %d' % (free_count, expected_free)
    assert calls == ([expected_free] if expected_free else []), 'init_memmap calls: %s' % calls
    if case == 'no-free-page':
        assert expected_free == 0
    print('upper=%#x free_start=%#x free_count=%d init_memmap_calls=%s' % (actual_upper, free_start, free_count, calls))
print('BOUNDARY_PASS ' + case)
end
detach
