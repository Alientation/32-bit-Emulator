# 32 bit emulator (with assembler and linker)
## Inspiration and Purpose
todo

## Details
### 32 bit emulator
- based off the ARM instruction set, with a few particular changes to keep it simple
- high test coverage to ensure correctness, will aim to increase in the future once the whole assembler-link-execute process is complete
- supports up to 64 instructions, currently ~50 are used
- custom bit formats for instructions to fit my purposes
### Assembler
- preprocesses the source/header files and converts into relocatable object files
- supports some C preprocessors like includes, defines, conditional blocks, and macros
- object files are in a custom format that is based off the ELF format
- supports a variety of assembler directives to help control the assembler process
- supports relocatable symbol referencing
### Linker
- processes all object files and creates an executable file
- relocates symbols as needed
### Executable loader
- todo

## Progress
todo

## How to run
*build.sh is broken currently, fill fix asap*
Run the build.bat or build.sh script located in the app subdirectory which will configure and build the project
*Uses Ninja and C++17 to build

