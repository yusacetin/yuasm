## YuASM - Work In Progress

An assembler I'm working on for fun for a fictional ISA. Programs can be run with [Yuemu](https://github.com/yusacetin/yuemu). It's still a work in progress so there are some known issues and testing hasn't been done thoroughly. For example, the last instruction isn't read if there isn't a trailing new line character after it.

## Instructions

### Legend

The most significant 4 bits of any instruction denote the instruction category, and the next 4 bits denote the instruction identifier. The most significant byte thus contains the opcode.
r[i]: value at register number i
m[i]: value at address i
names beginning with r represent registers that contain the related info
rd: destination register
rs: source register
'm' at the end of an instruction name stands for iMmediate
'd' at the end of an instruction name stands for Direct
'r' at the end of an instruction name stands for Register indirect
'n' at the end of an instruction name stands for iNdirect
pc: program counter

### Memory (category 0x0)

```
loadm rd val: r[rd] = val --> 0000_0000_8-bits-rd_16-bits-val
loadr rd raddr: r[rd] = m[r[raddr]] --> 0000_0001_8-bits-rd_8-bits-raddr_8-bits-skip
storen raddr rs: m[r[raddr]] = r[rs] --> 0000_0010_8-bits-raddr_8-bits-rs_8-bits-skip
stored addr rs:  m[addr] = r[rs] --> 0000_0011_16-bits-addr_8-bits-rs
loadd rd addr: r[rd] = m[addr] --> 0000_0100_8-bits-rd_16-bits-addr
```

### Arithmetic (category 0x1)

```
add rd rs1 rs2: r[rd] = r[rs1] + r[rs2] --> 0001_0000_8-bits-rd_8-bits-rs1_8-bits-rs2
sub rd rs1 rs2: r[rd] = r[rs1] - r[rs2] --> 0001_0001_8-bits-rd_8-bits-rs1_8-bits-rs2
mul rd rs1 rs2: r[rd] = r[rs1] * r[rs2] --> 0001_0010_8-bits-rd_8-bits-rs1_8-bits-rs2
div rd rs1 rs2: r[rd] = r[rs1] / r[rs2] --> 0001_0011_8-bits-rd_8-bits-rs1_8-bits-rs2
```

### Control (category 0x2)

Note: "val" in jump, jumpif, br, and brif should only be function/section names. Avoid using actual numbers.

```
jump val: pc += val --> 0010_0000_24-bits-val
jumpd rs: pc += r[rs] --> 0010_0001_8-bits-rs_skip-16-bits

jumpif val rcond: pc += val if r[rcond] > 0 --> 0010_0010_16-bits-val_8-bits-rcond
jumpifd rs rcond: pc += r[rs] if r[rcond] > 0 --> 0010_0011_8-bits-rs_8-bits-skip_8-bits-rcond

ret: return to caller --> 0010_00100_24-bits-skip
end: infinite self loop --> 0010_00101_24-bits-skip

br val: pc += val, push return addr to stack --> 0010_0110_24-bits-val
brif val rcond: pc += val if r[rcond] > 0, push return addr to stack --> 0010_0111_16-bits-val_8-bits-rcond
```

### Logical (category 0x3)

```
and rd rs1 rs2: r[rd] = r[rs1] & r[rs2] --> 0011_0000_8-bits-rd_8-bits-rs1_8-bits-rs2
or rd rs1 rs2: r[rd] = r[rs1] | r[rs2] --> 0011_0001_8-bits-rd_8-bits-rs1_8-bits-rs2

nand rd rs1 rs2: r[rd] = ~(r[rs1] & r[rs2]) --> 0011_0010_8-bits-rd_8-bits-rs1_8-bits-rs2
nor rd rs1 rs2: r[rd] = ~(r[rs1] | r[rs2]) --> 0011_0011_8-bits-rd_8-bits-rs1_8-bits-rs2

xor rd rs1 rs2: r[rd] = r[rs1] ^ r[rs2] --> 0011_0100_8-bits-rd_8-bits-rs1_8-bits-rs2
```

### Shift (category 0x4)

```
lshift rd rs1 rs2: r[rd] = r[rs1] << r[rs2] --> 0001_0000_8-bits-rd_8-bits-rs1_8-bits-rs2
rshift rd rs1 rs2: r[rd] = r[rs1] >> r[rs2] --> 0001_0001_8-bits-rd_8-bits-rs1_8-bits-rs2
```

### Comparison (category 0x5)

```
lt rd rs1 rs2: r[rd] = r[rs1] < r[rs2] --> 0001_0000_8-bits-rd_8-bits-rs1_8-bits-rs2
lte rd rs1 rs2: r[rd] = r[rs1] <= r[rs2] --> 0001_0001_8-bits-rd_8-bits-rs1_8-bits-rs2
gt rd rs1 rs2: r[rd] = r[rs1] > r[rs2] --> 0001_0010_8-bits-rd_8-bits-rs1_8-bits-rs2
gte rd rs1 rs2: r[rd] = r[rs1] >= r[rs2] --> 0001_0011_8-bits-rd_8-bits-rs1_8-bits-rs2
eq rd rs1 rs2: r[rd] = r[rs1] == r[rs2] --> 0001_0100_8-bits-rd_8-bits-rs1_8-bits-rs2
```