#pragma once
#ifndef OBJECT_FILE_H
#define OBJECT_FILE_H

#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <unordered_map>
#include <vector>

#define RELOCATABLE_FILE_TYPE 1
#define EXECUTABLE_FILE_TYPE 2
// todo #define SHARED_OBJECT_FILE_TYPE 3
#define EMU_32BIT_MACHINE_ID 1

class ObjectFile
{
    friend class Linker;
    public:
        ObjectFile();
        ObjectFile(File obj_file);

        void read_object_file(File object_file);
        void read_object_file(std::vector<byte>& bytes);
        void write_object_file(File object_file);

        /**
         * @brief                     Symbols defined in this unit
         *
         */
        struct SymbolTableEntry
        {
            /* TODO: This should be unsigned.. Can't have a negative index. */
            int symbol_name;                                        /* index into string table */
            word symbol_value;                                        /* value of symbol */
            enum class BindingInfo
            {
                LOCAL=0, GLOBAL=1, WEAK=2
            } binding_info;                                            /* type of symbol */
            int section;                                            /* index into section table, -1 indicates no section */
        };

        struct SectionHeader
        {
            int section_name;                                        /* index into string table */
            enum class Type
            {
                UNDEFINED, TEXT, DATA, BSS, SYMTAB, REL_TEXT, REL_DATA, REL_BSS, DEBUG, STRTAB,
            } type;                                                    /* type of section */
            word section_start;                                        /* start offset of section */
            word section_size;                                        /* size of section in bytes */
            word entry_size;                                        /* size of entry in section, todo this has not use imo, figure out why ELF has it listed */

            bool load_at_physical_address = false;
            word address = 0;
        };

        struct RelocationEntry
        {
            word offset;                                            /* offset from beginning of section to the symbol */
            int symbol;                                                /* index into symbol table */
            enum class Type
            {
                UNDEFINED,
                R_EMU32_O_LO12, R_EMU32_ADRP_HI20,                    /* Format O instructions and ADRP */
                R_EMU32_MOV_LO19, R_EMU32_MOV_HI13,                    /* MOV/MVN instructions */
                R_EMU32_B_OFFSET22,                                    /* Branch offset, +/- 24 bit value (last 2 bits are 0) */
            } type;                                                    /* type of relocation */
            word shift;                                                /* constant to be added to the value of the symbol */
            size_t token;                                            /* token index that the relocation entry is used on. Used to fill local symbols */
        };

        static const int BELF_HEADER_SIZE = 24;
        static const int SECTION_HEADER_SIZE = 45;
        static const int BSS_SECTION_SIZE = 8;
        static const int RELOCATION_ENTRY_SIZE = 28;
        static const int SYMBOL_TABLE_ENTRY_SIZE = 26;

        hword file_type;
        hword target_machine;
        hword flags;
        hword n_sections;

        std::vector<word> text_section;                                /* instructions stored in .text section */
        std::vector<byte> data_section;                                /* data stored in .data section */
        word bss_section = 0;                                        /* size of .bss section */
        std::unordered_map<int, SymbolTableEntry> symbol_table;        /* maps string index to symbol */
        std::vector<RelocationEntry> rel_text;                        /* references to symbols that need to be relocated */
        std::vector<RelocationEntry> rel_data;                        /* For now, no purpose */
        std::vector<RelocationEntry> rel_bss;                        /* For now, no purpose, this won't ever be used, get rid of this */
        /* Possbly in future add separate string table for section headers */
        /* Also, reword string table so that it stores the offset of the first character of a string in the string table, not the position of it in the array */
        std::vector<std::string> strings;                            /* stores all strings */
        std::unordered_map<std::string, int> string_table;            /* maps strings to index in the table*/
        std::vector<SectionHeader> sections;                        /* section headers */
        std::unordered_map<std::string, int> section_table;         /* map section name to index in sections */

        int add_section(const std::string& section_name, SectionHeader header);
        int add_string(const std::string& string);
        void add_symbol(const std::string& symbol, word value, SymbolTableEntry::BindingInfo binding_info, int section);

        std::string get_symbol_name(int symbol);

    private:
        enum class State
        {
            NO_STATE,
            DISASSEMBLING, DISASSEMBLED_SUCCESS, DISASSEMBLED_ERROR,
            WRITING, WRITING_SUCCESS, WRITING_ERROR,
        };

        State m_state;                                                /* state of the disassembly */
        File m_obj_file;

        void disassemble(std::vector<byte>& bytes);
        void print();
};

#endif /* OBJECT_FILE_H */