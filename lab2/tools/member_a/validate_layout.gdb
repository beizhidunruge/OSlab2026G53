hbreak pmm_init
continue
printf "=== Before page_init ===\n"
info registers pc satp
hbreak default_check
continue
printf "=== After page_init, before First-Fit self-test ===\n"
python
def number(expr):
    return int(gdb.parse_and_eval(expr))
base = 0x80000000
offset = number('va_pa_offset')
assert offset == 0xffffffff40000000
upper = number('npage') * 4096
count = number('npage - nbase')
pages_va = number('(unsigned long)pages')
pages_pa = (pages_va - offset) & ((1 << 64) - 1)
desc_end = pages_pa + count * number('sizeof(struct Page)')
free_start = (desc_end + 4095) & ~4095
first = (free_start - base) // 4096
reserved = [number('pages[%d].flags' % i) & 1 for i in range(first)]
assert all(reserved), 'Reserved prefix contains allocatable page'
assert all(not(number('pages[%d].flags' % i) & 1) for i in range(first, count))
free_area = gdb.parse_and_eval("'default_pmm.c'::free_area")
assert int(free_area['nr_free']) == (upper - free_start) // 4096
assert number('pages[%d].property' % first) == count - first
assert pages_pa >= ((number('(unsigned long)&end') - offset + 4095) & ~4095)
print('LAYOUT_PASS: all %d reserved pages, %d free pages, descriptor array protected' % (first, count-first))
print('pages_pa=%#x free_start=%#x upper=%#x sizeof(Page)=%d' % (pages_pa, free_start, upper, number('sizeof(struct Page)')))
end
detach
