# 32 bit emulator

Mimics a computer processor by simulating a personally curated set of **ARM-like** instructions.
Comes packaged with a preprocessor, assembler, linker, and executable loader to run **basm** assembly code.

* supports up to 64 instructions, currently **~50** are in use
* high **test coverage** to ensure correctness of emulator
* supports **preprocessors** and **assembler directives**

<p align="center">
  <img src="./img/objdump.PNG" alt="Objdump of assembled code" width = "500">
</p>

## How it works
1. **32 bit emulator**\
&mdash; mostly based off the ARM instruction set, with a few particular changes to simplify\
&mdash; custom bit formats for instructions to achieve a fixed width instruction set (4 bytes)
2. **Assembler**\
&mdash; preprocesses the **source/header** files (`.basm`, `.binc`) into `.bi` files and then converts into **relocatable object** files (`.bo`)\
&mdash; supports some C **preprocessors** like `#include`, `#define`, conditional blocks, and macros\
&mdash; object files are in a custom format that is based off the ELF format (`belf`)\
&mdash; supports a variety of **assembler directives** to help control the assembler process\
&mdash; supports relocatable symbol referencing
3. **Linker**\
&mdash; links all object files, resolving symbols, and creates an **executable file** (`.bexe`)
&mdash; relocates symbols as needed
4. **Executable loader**\
&mdash; loads a `.bexe` file into the emulator memory *for now, stores programs at the beginning of memory*

## Progress
* Sep '23 - May '24
  * First iteration of this project (**6502 emulator** and attempted assembler)
  * Ultimately transitioned project to focus on an ARM-like emulator
* Jun '24 - Present
  * Near complete project, with large portion of work spanning just a couple weeks
  * Happy with the state the project now

## Future Goals
* forward arguments passed into the executable to the assembler build process so that recompilation of the project is not necessary
to build different executables
* add floating point instructions, flesh out more relocation entry types, and add section directives to help partition code
* test emulator with simple and fun programs
* create simple OS with a CLI

## How to run
Run the `build.bat` or `build.sh` script located in the app subdirectory which will configure and build the entire project
* make sure to run it at the directory the .bat/.sh script is located
* Run the executable which will be located in the `build/debug` and `build/release` directory\
* Uses **Ninja** and **C++17** to build *have not tested with other versions*

## Usage
*todo*
