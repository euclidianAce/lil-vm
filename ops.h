#ifndef OPS_H
#define OPS_H

// 16 bit vm
//
// 16 16-bit general purpose registers
//
// instruction format
//
// instructions are 24 bits (fixed size)
//
// aaaaaaaa bbbb bbbb cccc cccc
// ^^^^^^^^ ^^^^^^^^^ ^^^^^^^^^
// opcode   operands and/or modifiers
//          
//          ^^^^ ^^^^ ^^^^ ^^^^
//          R1   R2   R3   R4   interpreted as register ids
//
//          ^^^^^^^^^ ^^^^^^^^^
//          B1        B2        interpreted as bytes
//
//          ^^^^^^^^^^^^^^^^^^^
//          D                   interpreted as a double byte
//          
//                              An S or U before any of these means
//                              interpret as Signed or Unsigned
//
// up to 256 opcodes
// up to 4 registers can be used as operands in one instruction
// 
// TODO:
// 64 opcodes seems like about enough, the upper two bits could be used as
// modifiers
//
// TODO:
// relative branches don't make much sense when absolute branches can fit an
// entire address in them

// TODO:
// it would probably be better to try making instructions 16 or 32 bit fixed or
// variable sized, 24 bit fixed is a bit odd

typedef enum vm_operands {
	vm_operands_none,
	vm_operands_rrrr,
	vm_operands_rrr,
	vm_operands_rrb,
	vm_operands_rr,
	vm_operands_rb,
	vm_operands_bb,
	vm_operands_r,
	vm_operands_d,
} vm_operands;

//      X(Identifier,                 asm mnemonic,  encoding)

#define vm_x_instructions(X) \
	X(Nop,                        "nop",         none    ) \
	/* Immediates */                                       \
	X(Load_Immediate_Byte,        "lib",         rb      ) /* R1 <- B2 */ \
	X(Shift_In_Byte,              "sib",         rb      ) /* R1 <- R1 << 8 | B2 */ \
	/* Register Shuffling */                               \
	X(Copy,                       "copy",        rr      ) /* R1 <- R2 */ \
	X(Copy_2,                     "copy2",       rrr     ) /* R1 <- R3, R2 <- R3 */ \
	X(Copy_3,                     "copy3",       rrrr    ) /* R1 <- R4, R2 <- R4, R3 <- R4 */ \
	X(Rotate_2,                   "rot",         rr      ) /* simultaneously R1 <- R2, R2 <- R1 */ \
	X(Rotate_3,                   "rot3",        rrr     ) /* simultaneously R1 <- R2, R2 <- R3, R3 <- R1 */ \
	X(Rotate_4,                   "rot4",        rrrr    ) /* simultaneously R1 <- R2, R2 <- R3, R3 <- R4, R4 <- R1 */ \
	/* Arithmetic */                                       \
	X(Bit_Or,                     "or",          rrr     ) /* R1 <- R2 bor R3 */ \
	X(Bit_Xor,                    "xor",         rrr     ) /* R1 <- R2 bxor R3 */ \
	X(Bit_And,                    "and",         rrr     ) /* R1 <- R2 band R3 */ \
	X(Shift_Left,                 "shl",         rrrr    ) /* R1:R2 <- R3 << UR4 */ \
	X(Add,                        "add",         rrrr    ) /* R1:R2 <- R3 + R4 */ \
	X(Subtract,                   "sub",         rrrr    ) /* R1:R2 <- R3 - R4 */ \
	X(Unsigned_Multiply,          "umul",        rrrr    ) /* R1:R2 <- UR3 * UR4 */ \
	X(Signed_Multiply,            "smul",        rrrr    ) /* R1:R2 <- SR3 * SR4 */ \
	X(Increment,                  "inc",         rrb     ) /* R1:R2 <- R2 + B2 */ \
	X(Decrement,                  "dec",         rrb     ) /* R1:R2 <- R2 - B2 */ \
	X(Unsigned_Divide,            "udiv",        rrrr    ) /* R1 <- UR3 / UR4, R2 <- UR3 rem UR4 */ \
	X(Signed_Divide,              "sdiv",        rrrr    ) /* R1 <- SR3 / SR4, R2 <- SR3 rem SR4 */ \
	X(Shift_Logical_Right,        "slr",         rrr     ) /* R1 <- UR2 >> UR3 */ \
	X(Shift_Arithmetic_Right,     "sar",         rrr     ) /* R1 <- SR2 >> UR3 */ \
	/* Comparisons */                                      \
	X(Compare_Signed,             "scmp",        rrrr    ) /* R1 <- SR3 <= SR4, R2 <- UR3 > UR4 */ \
	X(Compare_Unsigned,           "ucmp",        rrrr    ) /* R1 <- UR3 <= UR4, R2 <- UR3 > UR4 */ \
	X(Compare_Equal,              "eq",          rrrr    ) /* R1 <- R3 = R4,    R2 <- R3 /= R4 */ \
	/* Control flow */                                     \
	X(Branch_Immediate_Absolute,  "bia",         d       ) /* pc <- D */ \
	X(Branch_Immediate_Relative,  "bir",         d       ) /* pc <- pc + SD */ \
	X(Branch_Absolute,            "ba",          r       ) /* pc <- R1 */ \
	X(Branch_Relative,            "br",          r       ) /* pc <- pc + SR1 */ \
	X(Skip_If_Zero,               "sz",          r       ) /* if R1 = 0 then pc += 3 */ \
	X(Skip_If_Non_Zero,           "snz",         r       ) /* if R1 /= 0 then pc += 3 */ \
	/* Memory */                                           \
	X(Read_Address_Byte,          "rab",         rr      ) /* R1 <- memory[R2] */ \
	X(Read_Address_Two_Byte,      "rad",         rr      ) /* R1 <- memory[R2] */ \
	X(Write_Address_Byte,         "wab",         rr      ) /* memory[R1] <- R2 */ \
	X(Write_Address_Two_Byte,     "wad",         rr      ) /* memory[R1] <- R2 */ \
	/* External/Device IO */                               \
	X(Port_Write,                 "portw",       rb      ) /* port(B2) <- R1  */ \
	X(Port_Read,                  "portr",       rb      ) /* R1 <- port(B2) */ \
	/* Subroutines/Stack */ \
	X(Push,                       "push",        r       ) /* r15 <- r15 + 2, memory[r15] <- R1 */ \
	X(Pop,                        "pop",         r       ) /* R1 <- memory[r15], r15 <- r15 + 1 */ \
	X(Call_Immediate_Relative,    "callir",      d       ) /* push absolute return address, branch immediate relative */ \
	X(Call_Immediate_Absolute,    "callia",      d       ) /* push absolute return address, branch immediate absolute */ \
	X(Call_Relative,              "callr",       r       ) /* push absolute return address, branch relative */ \
	X(Call_Absolute,              "calla",       r       ) /* push absolute return address, branch absolute */ \
	X(Return,                     "ret",         none    ) /* pop address, branch absolute */ \


// TODO:
// bit test/scan?
// repeat/loop instructions?

#define X(name, mnemonic, encoding) vm_op_##name,
typedef enum vm_op { vm_x_instructions(X) } vm_op;
#undef X

#endif // OPS_H
