hbreak *0x80200008
continue
printf "=== Bare: real kernel store before paging ===\n"
x/i $pc
info registers pc t0 a0 satp
si
hbreak *memset
continue
printf "=== Sv39: BSS clear ===\n"
info registers pc a0 a1 a2 satp
disassemble /r memset
si 4
printf "=== Actual byte-store instruction and effective VA ===\n"
x/i $pc
info registers a5 a1
p/x $a5 - 1
p/x ($a5 - 1) - 0xffffffff40000000
printf "=== Root PTE and one-level leaf ===\n"
p/x &boot_page_table_sv39
x/gx (unsigned long)&boot_page_table_sv39 + 511*8
python
va = int(gdb.parse_and_eval('$a5 - 1')) & ((1<<64)-1)
satp = int(gdb.parse_and_eval('$satp')) & ((1<<64)-1)
root = (satp & ((1<<44)-1)) << 12
idx = (va >> 30) & 511
root_va = int(gdb.parse_and_eval('(unsigned long)&boot_page_table_sv39'))
pte = int(gdb.parse_and_eval('*(unsigned long *)(%d + %d)' % (root_va, idx*8)))
pa = ((pte >> 10) << 12) | (va & ((1<<30)-1))
assert idx == 511 and pte == 0x200000cf
assert pa == ((va - 0xffffffff40000000) & ((1<<64)-1))
print('GUEST_VA_PA_PASS VA=%#x root_PA=%#x PTE_PA=%#x PTE=%#x PA=%#x' % (va,root,root+idx*8,pte,pa))
end
detach
