## YuASM - Work In Progress

An assembler I'm working on for fun for a fictional ISA. Programs can be run with [Yuemu](https://github.com/yusacetin/yuemu). It's still a work in progress so there are some known issues and testing hasn't been done thoroughly. For example, the last instruction isn't read if there isn't a trailing new line character after it.

### System Overview

The architecture operates on 256 general purpose registers. Why so many? I wanted to have 32 but I also wanted the program binaries to be easily readable in hexadecimal so I didn't want to split up a hexadecimal digit into different fields. The high number of registers enables registers to be used as variable storages in small programs instead of the memory. Values can be written to and read from memory using all common addressing modes.

### Syntax

#### Overview

A line in yuasm code can be one of the following: a line comment or part of a block comment, a preprocessor directive, a function/section definition, or an instruction. Leading and trailing spaces are always (or almost always) allowed between tokens. It is recommended to define an entry section (such as 'main') and use a jump instruction to it at the beginning of programs. The programs won't automatically start running from a particularly named section (such as 'main'). Semicolons are allowed optionally at the end of lines.

#### Comments

Line comments and block comments are supported. Line comments start with two slashes (`//`) and continue until the end of the current line. Block comments start with `/*` and end with `*/`. Block comments can't be nested, so `/* first block /* second block */ third block */` would be illegal since 'third block' would not be considered part of the comment. Block comments can be placed between tokens.

```
add a b c // adding b to c and saving to a
mul 1 b /* this is also allowed */ 3
```

#### Preprocessor Directives

There are currently two preprocessor directives: `define` and `include`. Preprocessor directives are used with the prefix `#` which must be attached to the preprocessing tokens. They're both used as the same purpose as in C/C++. File paths after `include` must begin and end with double quote marks (`"`). Macro values defined with `define` can't include spaces or special characters.

```
#define macro_name 123 // any occurrence of the word 'macro_name' will be replaced by '123'
#include "../libraries/my_file.yuh" // .yuh is the 'official' header extension for yuasm
```

#### Functions/Sections

The terms 'function' and 'section' are used interchangably throughout the source code and in any documentation. Sections can be defined using a dot (`.`) followed by the section name followed by a semicolon (and then followed by either comments or a new line). There can be as many spaces as desired between the dot, the section name, and the semicolon.

```
.main: // OK
.  main : // OK
.main   : // OK
```

#### Instructions

See `instructions.txt` for a complete list of instructions and their bit maps. Instructions are written as the instruction name followed by any parameters. Number of parameters is fixed for each instruction. See the example in the [Comments](#comments) section.

### License

GNU General Public License version 3 or later.
