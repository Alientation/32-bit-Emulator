#pragma once
#ifndef EMULATOR32BIT_H
#define EMULATOR32BIT_H

#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/system_bus.h"

#include <string>

class MMU;  /* Forward declare from 'better_virtual_memory.h' */
class Timer; /* Forward declare from 'timer.h' */

/**
 * @brief                    IDs for special registers
 *
 *
 *    Stack grows downwards
 * <--------STACK_TOP-------->
 *          Saved FP
 *          Saved LR                <--- fp
 *      local variables
 *          <...>
 *          <...>
 *          <...>
 *      local variables             <---- sp
 *
 * Link register stores the previous pc, the next instruction is what will be
 * executed.
 *
 */
constexpr int NUM_REG = 32; /* Number of general purpose stack registers */
constexpr int NR = 8;       /* Number register for syscalls */
constexpr int FP = 28;      /* Frame Pointer - points to saved (FP,LR) in stack */
constexpr int LINKR = 29;   /* Link Register */
constexpr int SP = 30;      /* Stack Pointer */
constexpr int XZR = 31;     /* Zero Register */

/**
 * @brief                     Flag bit locations in _pstate register
 *
 */
constexpr int N_FLAG = 0;       /* Negative Flag */
constexpr int Z_FLAG = 1;       /* Zero Flag */
constexpr int C_FLAG = 2;       /* Carry Flag */
constexpr int V_FLAG = 3;       /* Overflow Flag */
constexpr int USER_FLAG = 8;    /* User mode flag */
constexpr int REAL_FLAG = 9;    /* Real memory mode flag */

/**
 * @brief                    Which bit of the instruction determines whether flags will be updated
 *
 */
constexpr int S_BIT = 25;       /* Update Flag Bit */

constexpr int NUM_INSTRUCTIONS = 64;


/**
 * @brief                     32 bit Emulator
 * @paragraph                Modeled off of the ARM architecture with many simplifications. A software simulated processor.
 *
 */
class Emulator32bit
{
    public:
        Emulator32bit();
        Emulator32bit(word ram_npages, word ram_start_page, const byte rom_data[], word rom_npages, word rom_start_page);
        Emulator32bit(RAM *ram, ROM *rom, Disk *disk);
        ~Emulator32bit();
        void print();

        enum InterruptType
        {
            /* TODO */
            BAD_REG,
            BAD_INSTR,
            HALT_INSTR,
            FAILED_ASSERT,
            BAD_PAGEDIR,
            PAGEFAULT,
        };

        struct InterruptFrame
        {
            word saved_reg[NUM_REG];
            word saved_px;
            word saved_pstate;
            word saved_pagedir;


        };

        class Exception : public std::exception
        {
            private:
                InterruptType type;
                std::string message;

            public:
                Exception(InterruptType type, const std::string& msg);
                const char* what() const noexcept override;
        };

        enum class ConditionCode
        {
            EQ = 0,                 /* Equal                        : Z==1 */
            NE = 1,                 /* Not Equal                    : Z==0 */
            CS = 2, HS = 2,         /* Unsigned higher or same        : C==1 */
            CC = 3, LO = 3,         /* Unsigned lower                : C==0 */
            MI = 4,                 /* Negative                        : N==1 */
            PL = 5,                 /* Nonnegative                    : N==0 */
            VS = 6,                 /* Signed overflow                 : V==1 */
            VC = 7,                 /* No signed overflow            : V==0 */
            HI = 8,                 /* Unsigned higher                : (C==1) && (Z==0) */
            LS = 9,                 /* Unsigned lower or same         : (C==0) || (Z==0) */
            GE = 10,                /* Signed greater than or equal    : N==V */
            LT = 11,                /* Signed less than                : N!=V */
            GT = 12,                /* Signed greater than            : (Z==0) && (N==V) */
            LE = 13,                /* Signed less than or equal     : (Z==1) || (N!=V) */
            AL = 14,                /* Always Executed                : NONE */
            NV = 15,                /* Never Executed                 : NONE */
        };

        enum ShiftType
        {
            SHIFT_LSL, SHIFT_LSR, SHIFT_ASR, SHIFT_ROR
        };

        enum AddrType
        {
            ADDR_OFFSET, ADDR_PRE_INC, ADDR_POST_INC
        };

        static const word RAM_NPAGES;     /* Default size of RAM memory in bytes */
        static const word RAM_START_PAGE;    /* Default 32 bit start address of RAM memory */
        static const word ROM_NPAGES;     /* Default size of ROM memory in bytes */
        static const word ROM_START_PAGE;    /* Default 32 bit start address of ROM memory */
        static const byte ROM_DATA[];       /* Data stored in ROM, should be of the same length specified in @ref ROM_NPAGES */

        RAM *ram;
        ROM *rom;
        Disk *disk;
        VirtualMemory *mmu;
        SystemBus system_bus;

        Timer *timer;

        word _pagedir;                                  /* Pointer to Page directory for virtual address space. */

        /**
         * @brief            Run the emulator for a given number of instructions
         *
         * @param             instructions: Number of instructions to run, if 0 run until HLT instruction or exception is thrown
         * @throws            Exception
         */
        void run(unsigned long long instructions);

        /**
         * @brief            Resets the processor state
         *
         */
        void reset();

        inline void set_pc(word pc)
        {
            _pc = pc;
        }

        inline word get_pc()
        {
            return _pc;
        }

        inline word read_reg(byte reg)
        {
            return ((word) _x[reg]) & ((word) (_x[reg] >> 32));
        }

        inline void write_reg(byte reg, word val)
        {
            _x[reg] = (((word) _x[reg]) ^ ((dword) val << 32));
        }

        /**
         * @brief             Sets the @ref _pstate NZCV flags
         *
         * @param             N: Negative flag
         * @param             Z: Zero flag
         * @param             C: Carry flag
         * @param             V: Overflow flag
         */
        inline void set_NZCV(bool N, bool Z, bool C, bool V)
        {
            _pstate = set_bit(_pstate, N_FLAG, N);
            _pstate = set_bit(_pstate, Z_FLAG, Z);
            _pstate = set_bit(_pstate, C_FLAG, C);
            _pstate = set_bit(_pstate, V_FLAG, V);
        }

        /**
         * @brief           Sets flags in the process state register
         *
         * @param           flag: Bit to set
         * @param           value: Flag value
         */
        inline void set_flag(int flag, bool value)
        {
            _pstate = set_bit(_pstate, flag, value);
        }

        inline bool get_flag(int flag)
        {
            return test_bit(_pstate, flag);
        }

        /* @todo determine if fp registers are needed */
        // word fpcr;
        // word fpsr;

    private:
        /**
         * General purpose registers, x0-x29, xzr, and SP. x29 is the link register.
         *
         * Format: top 32 bits register value, bottom 32 bits mask value (for xzr register)
         */
        dword _x[NUM_REG];
        word _pc;                                        /* Program counter */
        word _pstate;                                    /* Program state. Bits 0-3 are NZCV flags. Rest are TODO */

        typedef void (Emulator32bit::*InstructionFunction)(word);
        InstructionFunction _instructions[NUM_INSTRUCTIONS];

        // note, stringstreams cannot use the static const for some reason
        #define _INSTR(func_name, opcode) \
        private: void _##func_name(word instr); \
        public: static constexpr word _op_##func_name = opcode;
        void fill_out_instructions();

        word calc_mem_addr(word xn, sword offset, byte addr_mode);

        inline void execute(word instr)
        {
            (this->*_instructions[bitfield_u32(instr, 26, 6)])(instr);
        }

        inline bool check_cond(word pstate, byte cond)
        {
            bool N = test_bit(pstate, N_FLAG);
            bool Z = test_bit(pstate, Z_FLAG);
            bool C = test_bit(pstate, C_FLAG);
            bool V = test_bit(pstate, V_FLAG);

            switch((ConditionCode) cond)
            {
                case ConditionCode::EQ:            /* EQUAL */
                    return Z == 1;
                case ConditionCode::NE:            /* NOT EQUAL */
                    return Z == 0;
                case ConditionCode::CS:            /* CARRY SET */
                    return C == 1;
                case ConditionCode::CC:            /* CARRY CLEAR */
                    return C == 0;
                case ConditionCode::MI:            /* NEGATIVE */
                    return N == 1;
                case ConditionCode::PL:            /* NONNEGATIVE */
                    return N == 0;
                case ConditionCode::VS:            /* OVERFLOW SET */
                    return V == 1;
                case ConditionCode::VC:            /* OVERFLOW CLEAR */
                    return V == 0;
                case ConditionCode::HI:            /* UNSIGNED HIGHER */
                    return C == 1 && Z == 0;
                case ConditionCode::LS:            /* UNSIGNED LOWER OR EQUAL */
                    return C == 0 || Z == 1;
                case ConditionCode::GE:            /* SIGNED GREATER OR EQUAL */
                    return N==V;
                case ConditionCode::LT:            /* SIGNED LOWER */
                    return N!=V;
                case ConditionCode::GT:            /* SIGNED GREATER */
                    return Z == 0 && (N==V);
                case ConditionCode::LE:            /* SIGNED LOWER OR EQUAL */
                    return Z == 1 || (N!=V);
                case ConditionCode::AL:            /* ALWAYS */
                    return true;
                case ConditionCode::NV:            /* NEVER */
                    return false;
            }

            /*
                Shouldn't ever reach this, but to be safe, return false to clearly
                indicate a incorrect instruction
            */
            return false;
        }

        // instruction handling
        _INSTR(special_instructions, 0b000000)

        void _hlt(const word instr);
        void _nop(const word instr);
        void _msr(const word instr);
        void _mrs(const word instr);
        void _tlbi(const word instr);
        void _atomic(const word instr);
        void _swp(const word instr);
        void _ldadd(const word instr);
        void _ldclr(const word instr);
        void _ldset(const word instr);


        _INSTR(add, 0b000001)
        _INSTR(sub, 0b000010)
        _INSTR(rsb, 0b000011)
        _INSTR(adc, 0b000100)
        _INSTR(sbc, 0b000101)
        _INSTR(rsc, 0b000110)
        _INSTR(mul, 0b000111)
        _INSTR(umull, 0b001000)
        _INSTR(smull, 0b001001)

        _INSTR(vabs, 0b001010)
        _INSTR(vneg, 0b001011)
        _INSTR(vsqrt, 0b001100)
        _INSTR(vadd, 0b001101)
        _INSTR(vsub, 0b001110)
        _INSTR(vdiv, 0b001111)
        _INSTR(vmul, 0b010000)
        _INSTR(vcmp, 0b010001)
        _INSTR(vsel, 0b010010)
        _INSTR(vcint, 0b010011)
        _INSTR(vcflo, 0b010100)
        _INSTR(vmov, 0b010101)

        _INSTR(and, 0b010110)
        _INSTR(orr, 0b010111)
        _INSTR(eor, 0b011000)
        _INSTR(bic, 0b011001)
        _INSTR(lsl, 0b011010)
        _INSTR(lsr, 0b011011)
        _INSTR(asr, 0b011100)
        _INSTR(ror, 0b011101)

        _INSTR(cmp, 0b011110)
        _INSTR(cmn, 0b011111)
        _INSTR(tst, 0b100000)
        _INSTR(teq, 0b100001)

        _INSTR(mov, 0b100010)
        _INSTR(mvn, 0b100011)

        _INSTR(ldr, 0b100100)
        _INSTR(ldrb, 0b100101)
        _INSTR(ldrh, 0b100110)
        _INSTR(str, 0b100111)
        _INSTR(strb, 0b101000)
        _INSTR(strh, 0b101001)
        // _INSTR(swp, 0b101010)
        // _INSTR(swpb, 0b101011)
        // _INSTR(swph, 0b101100)
        _INSTR(b, 0b101101)
        _INSTR(bl, 0b101110)
        _INSTR(bx, 0b101111)
        _INSTR(blx, 0b110000)
        _INSTR(swi, 0b110001)

        _INSTR(adrp, 0b110010)

        // _INSTR(nop_, 0b110100)
        // _INSTR(nop_, 0b110101)
        // _INSTR(nop_, 0b110110)
        // _INSTR(nop_, 0b110111)
        // _INSTR(nop_, 0b111000)
        // _INSTR(nop_, 0b111001)
        // _INSTR(nop_, 0b111010)
        // _INSTR(nop_, 0b111011)
        // _INSTR(nop_, 0b111100)
        // _INSTR(nop_, 0b111101)
        // _INSTR(nop_, 0b111110)

        // _INSTR(nop, 0b111111)

        #undef _INSTR

        /* Software Interrupt Handling */
        void _emu_print();
        void _emu_printr(byte reg_id);
        void _emu_printm(word mem_addr, byte size, bool little_endian);
        void _emu_printp();
        void _emu_assertr(byte reg_id, word min_value, word max_value);
        void _emu_assertm(word mem_addr, byte size, bool little_endian, word min_value, word max_value);
        void _emu_assertp(byte p_state_id, bool expected_value);
        void _emu_log(word str);
        void _emu_err(word err);


    public:
        // help assemble instructions
        static word asm_hlt();
        static word asm_nop();
        static word asm_msr(word sysreg, bool imm, word xn_or_imm16);
        static word asm_mrs(word xn, word sysreg);
        static word asm_tlbi(word xt, bool isxt, word imm16);
        static word asm_atomic(word xt, word xn, word xm, byte width, byte atop);

        static word asm_format_o(byte opcode, bool s, int xd, int xn, int imm14);
        static word asm_format_o(byte opcode, bool s, int xd, int xn, int xm, ShiftType shift, int imm5);
        static word asm_format_o1(byte opcode, int xd, int xn, bool imm, int xm, int imm5);
        static word asm_format_o2(byte opcode, bool s, int xlo, int xhi, int xn, int xm);
        static word asm_format_o3(byte opcode, bool s, int xd, int imm19);
        static word asm_format_o3(byte opcode, bool s, int xd, int xn, int imm14);
        static word asm_format_m(byte opcode, bool sign, int xt, int xn, int xm, ShiftType shift, int imm5, AddrType adr);
        static word asm_format_m(byte opcode, bool sign, int xt, int xn, int simm12, AddrType adr);
        static word asm_format_m1(byte opcode, int xd, int imm20);
        static word asm_format_b1(byte opcode, ConditionCode cond, sword simm22);
        static word asm_format_b2(byte opcode, ConditionCode cond, int xd);

        static std::string disassemble_instr(word instr);

        static constexpr word _opspec_hlt = 0b0000;
        static constexpr word _opspec_nop = 0b1111;
        static constexpr word _opspec_msr = 0b0001;
        static constexpr word _opspec_mrs = 0b0010;
        static constexpr word _opspec_tlbi = 0b0011;
        static constexpr word _opspec_atomic = 0b0100;

        static constexpr int ATOMIC_SWP = 0b0000;
        static constexpr int ATOMIC_LDADD = 0b0001;
        static constexpr int ATOMIC_LDCLR = 0b0010;
        static constexpr int ATOMIC_LDSET = 0b0011;

        static constexpr int ATOMIC_WIDTH_WORD = 0b00;
        static constexpr int ATOMIC_WIDTH_BYTE = 0b01;
        static constexpr int ATOMIC_WIDTH_HWORD = 0b10;

        static constexpr word SYSREG_PSTATE = 1;
};

#endif /* EMULATOR32BIT_H */