#include <string>
#include <map>

#include <../src/Motherboard/AlienCPU.h>

/**
 * Map of processor instructions to their opcodes respective to their addressing modes.
 * 
 * Used to determine if a processor instruction is valid for a given addressing mode.
 */
std::map<std::string, std::map<AddressingMode, u8>> instructionMap = {
    {"LDA", 
        {
            {IMMEDIATE, 0xA9}, 
            {ABSOLUTE, 0xAD}, 
            {ABSOLUTE_XINDEXED, 0xBD}, 
            {ABSOLUTE_YINDEXED, 0xB9},
            {ZEROPAGE, 0xA5},
            {ZEROPAGE_XINDEXED, 0xB5},
            {XINDEXED_INDIRECT, 0xA1},
            {INDIRECT_YINDEXED, 0xB1}
        }
    }, {"LDX",
        {
            {IMMEDIATE, 0xA2},
            {ABSOLUTE, 0xAE},
            {ABSOLUTE_YINDEXED, 0xBE},
            {ZEROPAGE, 0xA6},
            {ZEROPAGE_YINDEXED, 0xB6}
        }
    }, {"LDY", 
        {
            {IMMEDIATE, 0xA0},
            {ABSOLUTE, 0xAC},
            {ABSOLUTE_XINDEXED, 0xBC},
            {ZEROPAGE, 0xA4},
            {ZEROPAGE_XINDEXED, 0xB4}
        }
    }, {"STA",
        {
            {ABSOLUTE, 0x8D},
            {ABSOLUTE_XINDEXED, 0x9D},
            {ABSOLUTE_YINDEXED, 0x99},
            {ZEROPAGE, 0x85},
            {ZEROPAGE_XINDEXED, 0x95},
            {XINDEXED_INDIRECT, 0x81},
            {INDIRECT_YINDEXED, 0x91}
        }
    }, {"STX",
        {
            {ABSOLUTE, 0x8E},
            {ZEROPAGE, 0x86},
            {ZEROPAGE_YINDEXED, 0x96}
        }
    }, {"STY",
        {
            {ABSOLUTE, 0x8C},
            {ZEROPAGE, 0x84},
            {ZEROPAGE_XINDEXED, 0x94}
        }
    }, {"TAX",
        {
            {IMPLIED, 0xAA}
        }
    }, {"TAY",
        {
            {IMPLIED, 0xA8}
        }
    }, {"TSX",
        {
            {IMPLIED, 0xBA}
        }
    }, {"TXA",
        {
            {IMPLIED, 0x8A}
        }
    }, {"TXS",
        {
            {IMPLIED, 0x9A}
        }
    }, {"TYA",
        {
            {IMPLIED, 0x98}
        }
    }, {"PHA",
        {
            {IMPLIED, 0x48}
        }
    }, {"PHP",
        {
            {IMPLIED, 0x08}
        }
    }, {"PLA",
        {
            {IMPLIED, 0x68}
        }
    }, {"PLP",
        {
            {IMPLIED, 0x28}
        }
    }, {"DEC",
        {
            {ABSOLUTE, 0xCE},
            {ABSOLUTE_XINDEXED, 0xDE},
            {ZEROPAGE, 0xC6},
            {ZEROPAGE_XINDEXED, 0xD6}
        }
    }, {"DEX",
        {
            {IMPLIED, 0xCA}
        }
    }, {"DEY",
        {
            {IMPLIED, 0x88}
        }
    }, {"INC",
        {
            {ABSOLUTE, 0xEE},
            {ABSOLUTE_XINDEXED, 0xFE},
            {ZEROPAGE, 0xE6},
            {ZEROPAGE_XINDEXED, 0xF6}
        }
    }, {"INX",
        {
            {IMPLIED, 0xE8}
        }
    }, {"INY",
        {
            {IMPLIED, 0xC8}
        }
    }, {"ADC",
        {
            {IMMEDIATE, 0x69},
            {ABSOLUTE, 0x6D},
            {ABSOLUTE_XINDEXED, 0x7D},
            {ABSOLUTE_YINDEXED, 0x79},
            {ZEROPAGE, 0x65},
            {ZEROPAGE_XINDEXED, 0x75},
            {XINDEXED_INDIRECT, 0x61},
            {INDIRECT_YINDEXED, 0x71}
        }
    }, {"SBC",
        {
            {IMMEDIATE, 0xE9},
            {ABSOLUTE, 0xED},
            {ABSOLUTE_XINDEXED, 0xFD},
            {ABSOLUTE_YINDEXED, 0xF9},
            {ZEROPAGE, 0xE5},
            {ZEROPAGE_XINDEXED, 0xF5},
            {XINDEXED_INDIRECT, 0xE1},
            {INDIRECT_YINDEXED, 0xF1}
        }
    }, {"AND",
        {
            {IMMEDIATE, 0x29},
            {ABSOLUTE, 0x2D},
            {ABSOLUTE_XINDEXED, 0x3D},
            {ABSOLUTE_YINDEXED, 0x39},
            {ZEROPAGE, 0x25},
            {ZEROPAGE_XINDEXED, 0x35},
            {XINDEXED_INDIRECT, 0x21},
            {INDIRECT_YINDEXED, 0x31}
        }
    }, {"EOR",
        {
            {IMMEDIATE, 0x49},
            {ABSOLUTE, 0x4D},
            {ABSOLUTE_XINDEXED, 0x5D},
            {ABSOLUTE_YINDEXED, 0x59},
            {ZEROPAGE, 0x45},
            {ZEROPAGE_XINDEXED, 0x55},
            {XINDEXED_INDIRECT, 0x41},
            {INDIRECT_YINDEXED, 0x51}
        }
    }, {"ORA",
        {
            {IMMEDIATE, 0x09},
            {ABSOLUTE, 0x0D},
            {ABSOLUTE_XINDEXED, 0x1D},
            {ABSOLUTE_YINDEXED, 0x19},
            {ZEROPAGE, 0x05},
            {ZEROPAGE_XINDEXED, 0x15},
            {XINDEXED_INDIRECT, 0x01},
            {INDIRECT_YINDEXED, 0x11}
        }
    }, {"ASL",
        {
            {ACCUMULATOR, 0x0A},
            {ABSOLUTE, 0x0E},
            {ABSOLUTE_XINDEXED, 0x1E},
            {ZEROPAGE, 0x06},
            {ZEROPAGE_XINDEXED, 0x16}
        }
    }, {"LSR",
        {
            {ACCUMULATOR, 0x4A},
            {ABSOLUTE, 0x4E},
            {ABSOLUTE_XINDEXED, 0x5E},
            {ZEROPAGE, 0x46},
            {ZEROPAGE_XINDEXED, 0x56}
        }
    }, {"ROL",
        {
            {ACCUMULATOR, 0x2A},
            {ABSOLUTE, 0x2E},
            {ABSOLUTE_XINDEXED, 0x3E},
            {ZEROPAGE, 0x26},
            {ZEROPAGE_XINDEXED, 0x36}
        }
    }, {"ROR",
        {
            {ACCUMULATOR, 0x6A},
            {ABSOLUTE, 0x6E},
            {ABSOLUTE_XINDEXED, 0x7E},
            {ZEROPAGE, 0x66},
            {ZEROPAGE_XINDEXED, 0x76}
        }
    }, {"CLC",
        {
            {IMPLIED, 0x18}
        }
    }, {"CLD", 
        {
            {IMPLIED, 0xD8}
        }
    }, {"CLI",
        {
            {IMPLIED, 0x58}
        }
    }, {"CLV",
        {
            {IMPLIED, 0xB8}
        }
    }, {"SEC",
        {
            {IMPLIED, 0x38}
        }
    }, {"SED",
        {
            {IMPLIED, 0xF8}
        }
    }, {"SEI",
        {
            {IMPLIED, 0x78}
        }
    }, {"CMP",
        {
            {IMMEDIATE, 0xC9},
            {ABSOLUTE, 0xCD},
            {ABSOLUTE_XINDEXED, 0xDD},
            {ABSOLUTE_YINDEXED, 0xD9},
            {ZEROPAGE, 0xC5},
            {ZEROPAGE_XINDEXED, 0xD5},
            {XINDEXED_INDIRECT, 0xC1},
            {INDIRECT_YINDEXED, 0xD1}
        }
    }, {"CPX",
        {
            {IMMEDIATE, 0xE0},
            {ABSOLUTE, 0xEC},
            {ZEROPAGE, 0xE4}
        }
    }, {"CPY",
        {
            {IMMEDIATE, 0xC0},
            {ABSOLUTE, 0xCC},
            {ZEROPAGE, 0xC4}
        }
    }, {"BCC",
        {
            {RELATIVE, 0x90}
        }
    }, {"BCS",
        {
            {RELATIVE, 0xB0}
        }
    }, {"BEQ",
        {
            {RELATIVE, 0xF0}
        }
    }, {"BPL",
        {
            {RELATIVE, 0x10}
        }
    }, {"BMI",
        {
            {RELATIVE, 0x30}
        }
    }, {"BNE",
        {
            {RELATIVE, 0xD0}
        }
    }, {"BVC", 
        {
            {RELATIVE, 0x50}
        }
    }, {"BVS",
        {
            {RELATIVE, 0x70}
        }
    }, {"JMP",
        {
            {ABSOLUTE, 0x4C},
            {INDIRECT, 0x6C}
        }
    }, {"JSR",
        {
            {ABSOLUTE, 0x20}
        }
    }, {"RTS",
        {
            {IMPLIED, 0x60}
        }
    }, {"BRK",
        {
            {IMPLIED, 0x00}
        }
    }, {"RTI",
        {
            {IMPLIED, 0x40}
        }
    }, {"BIT",
        {
            {ABSOLUTE, 0x2C},
            {ZEROPAGE, 0x24}
        }
    }
};


/**
 * Assembles the given assembly source code into machine code.
 * 
 * Documentation on the assembly language can be found SOMEWHERE
 * 
 * @param AlienCPU The AlienCPU to assemble the source code for.
 * @param source The assembly source code to assemble into machine code.
 */
static void assemble(AlienCPU& AlienCPU, std::string source) {
    
}


/**
 * Determines if the given processor instruction is valid for the given addressing mode.
 * 
 * @param instruction The processor instruction to check.
 * @param addressingMode The addressing mode to check.
 * @return True if the processor instruction is valid for the given addressing mode, false otherwise.
 */
static bool validProcessorInstructionAddressingMode(std::string instruction, AddressingMode addressingMode) {
    if (instructionMap.count(instruction) == 0) {
        return false;
    }
    
    return instructionMap.at(instruction).count(addressingMode) > 0;
}


/**
 * Gets the opcode for the given processor instruction and addressing mode.
 * 
 * @param instruction The processor instruction to get the opcode for.
 * @param addressingMode The addressing mode to get the opcode for.
 * 
 * @return The opcode for the given processor instruction and addressing mode.
 */
static u8 getProcessorInstructionOpcode(std::string instruction, AddressingMode addressingMode) {
    return instructionMap.at(instruction).at(addressingMode);
}


main() {
    AlienCPU AlienCPU;
    assemble(AlienCPU, "LDA #$FFFF");
}