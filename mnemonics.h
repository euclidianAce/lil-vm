#ifndef MNEMONICS_H
#define MNEMONICS_H

// conveniences for constructing programs in memory

#define lib(r, b) vm_op_Load_Immediate_Byte, ((r) << 4), (b)
#define sib(r, b) vm_op_Shift_In_Byte, ((r) << 4), (b)

#define lid(r, d) lib(r, (d) >> 8), sib(r, (d) & 0xff)

#define wab(addr_reg, value_reg) vm_op_Write_Address_Byte, (((addr_reg) << 4) | (value_reg)), 0x00
#define wad(addr_reg, value_reg) vm_op_Write_Address_Two_Byte, (((addr_reg) << 4) | (value_reg)), 0x00

#define rab(value_reg, addr_reg) vm_op_Read_Address_Byte, (((addr_reg) << 4) | (value_reg)), 0x00
#define rad(value_reg, addr_reg) vm_op_Read_Address_Two_Byte, (((addr_reg) << 4) | (value_reg)), 0x00

#define nop() vm_op_Nop, 0x00, 0x00

#define biz(r, d) vm_op_Skip_If_Non_Zero, ((r) << 4), 0x00, vm_op_Branch_Immediate, ((d) >> 8), ((d) & 0xff)
#define binz(r, d) vm_op_Skip_If_Zero, ((r) << 4), 0x00, vm_op_Branch_Immediate, ((d) >> 8), ((d) & 0xff)

#define dec(r, n) vm_op_Decrement, ((r) << 4), (n)

#define add(r1, r2, r3, r4) vm_op_Add, (((r1) << 4) | (r2)), (((r3) << 4) | (r4))
#define sub(r1, r2, r3, r4) vm_op_Subtract, (((r1) << 4) | (r2)), (((r3) << 4) | (r4))

#define pw(r, d) vm_op_Port_Write, ((r) << 4), d
#define pr(r, d) vm_op_Port_Read, ((r) << 4), d

#endif // MNEMONICS_H
