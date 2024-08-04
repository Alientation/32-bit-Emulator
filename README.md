# 32 bit emulator

Mimics a computer processor by simulating a curated set of **ARM-like** instructions.
Comes packaged with a preprocessor, assembler, linker, and executable loader to run **basm** assembly code.
*Easily* run the build process by passing arguments into the app program.

* supports 64 instructions, currently **~50** are in use
* high **test coverage** to ensure correctness of emulator
* supports *many* **preprocessors** and **assembler directives**

<p align="center">
  <img src="./img/objdump.PNG" alt="Objdump of assembled code" width = "45% style="display: inline-block; margin: 0 10px;">
  <img src="./img/fibonacci_example.PNG" alt="Fibonacci example basm code" width = "45% style="display: inline-block; margin: 0 10px;">
</p>
<div align="center">
  <strong>Objdump example</strong> (<i>left</i>) and <strong>fibonacci program</strong> (<i>right</i>)
</div>


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
&mdash; links all object files, resolving symbols, and creates an **executable file** (`.bexe`)\
&mdash; relocates symbols as needed
4. **Executable loader**\
&mdash; loads a `.bexe` file into the emulator memory *for now, stores programs at the beginning of memory*

## Progress
* Sep '23 - May '24
  * First iteration of this project (**6502 emulator** and attempted assembler)
  * Ultimately transitioned project to focus on an ARM-like emulator
* June '24 - July '24
  * Near MVP, with most functionality operational
* July '24 - Present
  * Added Disk memory with cache system
  * Added virtual memory - simulated in c++ but planning on porting over to a program ran on the emulator
  * Redesigned new logging system to be more compact and versatile
  * Added simple profiler to assist with optimizing
  * Began documentation process and cleaning up the codebase

## Future Goals
* ~~forward arguments passed into the executable to the assembler build process so that recompilation of the project is not necessary
to build different executables~~ ***Added 7/16/24***
* add floating point instructions, flesh out more relocation entry types, and add section directives to help partition code
* ~~test emulator with simple and fun programs~~ ***Added fibonacci and palindrome example programs 7/17/24***
* create simple OS with a CLI
* Support dynamically linked libraries
* Simple compiled language (like C)
* System libraries
* ~~Disk Memory~~ ***Added 7/21/24***
* ~~Virtual Memory~~ ***Added 7/25/24***
* File System
* Benchmarking system.. Goal instr/s would be around 100 million.
  * Haven't optimized *too* much, but reaching upwards of 13 million instructions per second with a simple loop in assembly
  * Voiding the logger macros significantly improves performance (roughly 25% speed up), need to have an option to void the logger macros
  * WOah, 2.5x speed up when reading instruction word by assuming 4 byte alignment by simply casting the byte array to a word array and indexing into it
  * Directly accessing RAM to read instructions leads to another 3x speed up to 39 million instructions per second, though this does not use virtual memory...
    * Up to 42 million when inlining many core functions
  * 55 million when removing memory out of bounds check.. might still want to keep that, but oh boy i need that gain
* Clean up and HEAVILY refactor code :~)
* Documentation!

## How to run
Run the `build.bat` or `build.sh` script located in the app subdirectory which will configure and build the entire project
* make sure to run it from the directory the .bat/.sh script is located  (***core/app***)
* Run the executable which will be located in the `build/debug` and `build/release` directory
* Uses **Ninja** and **C++17** to build \**have not tested with other versions\**

## Usage
To build a specific program, pass a build argument to the executable\
Note, currently the build process argument parser is extremely rudimentary so options that take an argument must have a space inbetween\
*Example:* `-I ./programs/include -o ./programs/build/palindrome ./programs/src/palindrome.basm -outdir ./programs/build`\
Some useful options
* -o <path>: output file path relative to the *app* subdirectory (output file is an executable `.bexe` unless otherwise specified)
* -I <path>: add a directory from where the `#include` preprocessor will search for `.binc` files
* -l <path>: links given library file `.ba` to the rest of the code in the linker phase
* -makedir: instead of an executable file output, create a `.ba` library file to link with in the future
#### More options can be found in the source code

## Documentation
*todo*
