# YuASM

An assembler I'm working on for fun for a fictional ISA. Programs can be run with [Yuemu](https://github.com/yusacetin/yuemu).

## System Overview

The architecture operates on 16 general purpose registers. Values can be written to and read from memory using all common addressing modes.

## Syntax

### Overview

A line in yuasm code can be one of the following: a line comment or part of a block comment, a preprocessor directive, a function/section definition, or an instruction. Leading and trailing spaces are always (or almost always) allowed between tokens. It is recommended to define an entry section (such as 'main') and use a jump instruction to it at the beginning of programs. The programs won't automatically start running from a particularly named section (such as 'main'). Semicolons are allowed optionally at the end of lines.

### Comments

Line comments and block comments are supported. Line comments start with two slashes (`//`) and continue until the end of the current line. Block comments start with `/*` and end with `*/`. Block comments can't be nested, so `/* first block /* second block */ third block */` would be illegal since 'third block' would not be considered part of the comment. Block comments can be placed between tokens.

```
add a b c // adding b to c and saving to a
mul 1 b /* this is also allowed */ 3
```

### Preprocessor Directives

There are currently two preprocessor directives: `define` and `include`. Preprocessor directives are used with the prefix `#` which must be attached to the preprocessing tokens. They're both used as the same purpose as in C/C++. File paths after `include` must begin and end with double quote marks (`"`). Macro values defined with `define` can't include spaces or special characters.

```
#define macro_name 123 // any occurrence of the word 'macro_name' will be replaced by '123'
#include "../libraries/my_file.yuh" // .yuh is the 'official' header extension for yuasm
```

### Functions/Sections

The terms 'function' and 'section' are used interchangably throughout the source code and in any documentation. Sections can be defined using a dot (`.`) followed by the section name followed by a semicolon (and then followed by either comments or a new line). There can be as many spaces as desired between the dot, the section name, and the semicolon.

```
.main: // OK
.  main : // OK
.main   : // OK
```

### Instructions

Instructions are written as the instruction name followed by any parameters. Number of parameters is fixed for each instruction.

```
add a b c
mul 1 b 3
```

## Instruction list

#### Legend

The most significant 4 bits of any instruction denote the instruction category, and the next 4 bits denote the instruction identifier. The most significant byte thus comprises the opcode. By convention names beginning with 'r' represent registers that contain the related information.

* r[i]: value at register number i
* m[i]: value at address i
* rd: destination register
* rs: source register
* raddr: address register
* addr: memory address
* DC: Don't Care
* 'm' at the end of an instruction name stands for iMmediate
* 'd' at the end of an instruction name stands for Direct
* 'r' at the end of an instruction name stands for Register indirect
* 'n' at the end of an instruction name stands for iNdirect
* pc: program counter

### Memory

#### uloadm (unsigned load immediate)

| Opcode (Category) | Opcode (ID) | rd       | Value     |
| :--:              | :---:       | :---:    | :---:     |
| `0b0000`          | `0b0000`    | (4 bits) | (20 bits) |

Syntax: `uloadm rd value`

Operation: `r[rd] = value`

#### loadm (signed load immediate)

| Opcode (Category) | Opcode (ID) | rd       | Value     |
| :--:              | :---:       | :---:    | :---:     |
| `0b0000`          | `0b0001`    | (4 bits) | (20 bits) |

Syntax: `loadm rd value`

Operation: `r[rd] = sign_extend(value)`

#### loadr (load register indirect)

| Opcode (Category) | Opcode (ID) | rd       | raddr    | DC         |
| :--:              | :---:       | :---:    | :---:    | :---:      |
| `0b0000`          | `0b0010`    | (4 bits) | (4 bits) | (16 bits)  |

Syntax: `loadr rd raddr`

Operation: `r[rd] = m[r[raddr]]`

#### storen (store indirect)

| Opcode (Category) | Opcode (ID) | raddr    | rs       | DC         |
| :--:              | :---:       | :---:    | :---:    | :---:      |
| `0b0000`          | `0b0011`    | (4 bits) | (4 bits) | (16 bits)  |

Syntax: `storen raddr rs`

Operation: `m[r[raddr]] = r[rs]`

#### stored (store direct)

| Opcode (Category) | Opcode (ID) | addr       | rs       |
| :--:              | :---:       | :---:      | :---:    |
| `0b0000`          | `0b0100`    | (20 bits)  | (4 bits) |

Syntax: `stored addr rs`

Operation: `m[addr] = r[rs]`

#### loadd (load direct)

| Opcode (Category) | Opcode (ID) | rd          | addr       |
| :--:              | :---:       | :---:       | :---:      |
| `0b0000`          | `0b0101`    | (4 bits)    | (20 bits)  |

Syntax: `loadd rd addr`

Operation: `r[rd] = m[addr]`

### Arithmetic

Arithmetic operations are always signed and use register addressing.

#### add (add)

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0001`          | `0b0000`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `add rd rs1 rs2`

Operation: `r[rd] = r[rs1] + r[rs2]`

#### sub (subtract)

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0001`          | `0b0001`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `sub rd rs1 rs2`

Operation: `r[rd] = r[rs1] - r[rs2]`

#### mul (multiply)

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0001`          | `0b0010`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `mul rd rs1 rs2`

Operation: `r[rd] = r[rs1] * r[rs2]`

#### div (divide)

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0001`          | `0b0011`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `div rd rs1 rs2`

Operation: `r[rd] = r[rs1] / r[rs2]`

### Control

For `jump`, `jumpif`, `br`, and `brif`, function/section name should be provided as value in assembly code. The linker detects the appropriate jump amount and substitutes.

#### jump (unconditional jump immediate)

| Opcode (Category) | Opcode (ID) | Value     |
| :---:             | :---:       | :---:     |
| `0b0010`          | `0b0000`    | (24 bits) |

Syntax: `jump value`

Operation: `pc += value`

#### jumpd (unconditional jump register)

| Opcode (Category) | Opcode (ID) | rs       | DC        |
| :---:             | :---:       | :---:    | :---:     |
| `0b0010`          | `0b0001`    | (4 bits) | (20 bits) |

Syntax: `jumpd rs`

Operation: `pc += r[rs]`

#### jumpif (conditional jump immediate)

| Opcode (Category) | Opcode (ID) | Value     | rcond    |
| :---:             | :---:       | :---:     | :---:    |
| `0b0010`          | `0b0010`    | (20 bits) | (4 bits) |

Syntax: `jumpif value rcond`

Operation: `pc += value if r[rcond] != 0`

#### jumpifd (conditional jump register)

| Opcode (Category) | Opcode (ID) | rs       | DC        | rcond    |
| :---:             | :---:       | :---:    | :---:     | :---:    |
| `0b0010`          | `0b0011`    | (4 bits) | (16 bits) | (4 bits) |

Syntax: `jumpifd rs rcond`

Operation: `pc += r[rs] if r[rcond] != 0`

#### ret (return)

| Opcode (Category) | Opcode (ID) | DC        |
| :---:             | :---:       | :---:     |
| `0b0010`          | `0b0100`    | (24 bits) |

Syntax: `ret`

Operation: Return to caller (i.e. latest `br` or `brif` call on stack)

#### end

| Opcode (Category) | Opcode (ID) | DC        |
| :---:             | :---:       | :---:     |
| `0b0010`          | `0b0101`    | (24 bits) |

Syntax: `end`

Operation: Indefinite self loop

#### br (unconditional branch immediate)

| Opcode (Category) | Opcode (ID) | Value     |
| :---:             | :---:       | :---:     |
| `0b0010`          | `0b0110`    | (24 bits) |

Syntax: `br value`

Operation: `pc += val, push return address to stack`

#### brif (conditional branch immediate)

| Opcode (Category) | Opcode (ID) | Value     | rcond    |
| :---:             | :---:       | :---:     | :---:    |
| `0b0010`          | `0b0111`    | (20 bits) | (4 bits) |

Syntax: `brif value rcond`

Operation: `pc += val if r[rcond] != 0, push return address to stack`

### Logical

#### and

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0011`          | `0b0000`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `and rd rs1 rs2`

Operation: `r[rd] = r[rs1] & r[rs2]`

#### or

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0011`          | `0b0001`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `or rd rs1 rs2`

Operation: `r[rd] = r[rs1] | r[rs2]`

#### nand

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0011`          | `0b0010`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `nand rd rs1 rs2`

Operation: `r[rd] = ~(r[rs1] & r[rs2])`

#### nor

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0011`          | `0b0011`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `nor rd rs1 rs2`

Operation: `r[rd] = ~(r[rs1] | r[rs2])`

#### xor

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0011`          | `0b0100`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `xor rd rs1 rs2`

Operation: `r[rd] = r[rs1] ^ r[rs2]`

### Bit Shift

#### lshift (left shift)

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0100`          | `0b0000`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `lshift rd rs1 rs2`

Operation: `r[rd] = r[rs1] << r[rs2]`

#### rshift (right shift)

Preserves sign bit.

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0100`          | `0b0001`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `rshift rd rs1 rs2`

Operation: `r[rd] = r[rs1] >> r[rs2]`

### Comparison

All comparison operation inputs are signed.

#### lt (less than)

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0101`          | `0b0000`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `lt rd rs1 rs2`

Operation: `r[rd] = (r[rs1] < r[rs2]) ? 1 : 0`

#### lte (less than or equal to)

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0101`          | `0b0001`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `lte rd rs1 rs2`

Operation: `r[rd] = (r[rs1] <= r[rs2]) ? 1 : 0`

#### gt (greater than)

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0101`          | `0b0010`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `gt rd rs1 rs2`

Operation: `r[rd] = (r[rs1] > r[rs2]) ? 1 : 0`

#### gte (greater than or equal to)

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0101`          | `0b0011`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `gte rd rs1 rs2`

Operation: `r[rd] = (r[rs1] >= r[rs2]) ? 1 : 0`

#### eq (equal to)

| Opcode (Category) | Opcode (ID) | rd       | rs1      | rs2      | DC        |
| :---:             | :---:       | :---:    | :---:    | :---:    | :---:     |
| `0b0101`          | `0b0100`    | (4 bits) | (4 bits) | (4 bits) | (12 bits) |

Syntax: `eq rd rs1 rs2`

Operation: `r[rd] = (r[rs1] == r[rs2]) ? 1 : 0`

### Examples

See the `.yuasm` files under the `programs` directory for some examples.

## License

GNU General Public License version 3 or later.
