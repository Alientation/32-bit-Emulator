#include "assembler/assembler.h"
#include "emulator32bit/emulator32bit.h"

#define AEMU_ONLY_CRITICAL_LOG
#include "util/logger.h"
#include "util/types.h"

#include <fstream>
#include <regex>

Assembler::Assembler (const Process *process, const File processed_file,
                      const std::string &output_path) :
    m_process (process),
    m_in_file (processed_file)
{
    // Create the output object file.
    if (output_path.empty ())
    {
        m_out_obj_file =
            File (m_in_file.get_name (), OBJECT_EXTENSION, processed_file.get_dir_str (), true);
    }
    else
    {
        m_out_obj_file = File (output_path, true);
    }

    EXPECT_TRUE_SS (m_process->valid_processed_file (processed_file),
                    std::stringstream () << "Assembler::Assembler() - Invalid processed file: "
                                         << processed_file.get_extension ());

    m_state = State::NOT_ASSEMBLED;

    // Convert the input file into tokens.
    m_tokenizer = Tokenizer (processed_file,
                             Tokenizer::Options{.keep_comments = false, .keep_whitespace = false});
}

void Assembler::assemble ()
{
    if (m_state != State::NOT_ASSEMBLED)
    {
        DEBUG ("Assembler::assemble() - Already assembled file: %s",
               m_in_file.get_name ().c_str ());
        return;
    }

    DEBUG ("Assembler::assemble() - Assembling file: %s", m_in_file.get_name ().c_str ());

    m_state = State::ASSEMBLING;

    // Clear the object file.
    m_out_obj_file.clear ();

    // Add appropriate sections to the object file.
    m_obj.add_section (".text", ObjectFile::SectionHeader::Type::TEXT);
    m_obj.add_section (".data", ObjectFile::SectionHeader::Type::DATA);
    m_obj.add_section (".bss", ObjectFile::SectionHeader::Type::BSS);
    m_obj.add_section (".symtab", ObjectFile::SectionHeader::Type::SYMTAB);
    m_obj.add_section (".rel.text", ObjectFile::SectionHeader::Type::REL_TEXT);
    m_obj.add_section (".rel.data", ObjectFile::SectionHeader::Type::REL_DATA);
    m_obj.add_section (".rel.bss", ObjectFile::SectionHeader::Type::REL_BSS);
    m_obj.add_section (".strtab", ObjectFile::SectionHeader::Type::STRTAB);

    // Parse tokens.
    DEBUG ("Assembler::assemble() - Parsing tokens.");
    while (m_tokenizer.has_next ())
    {
        const Tokenizer::Token &token = m_tokenizer.get_token ();
        DEBUG ("Assembler::assemble() - Assembling token %d: %s", i, token.to_string ().c_str ());

        if (token.type == Tokenizer::LABEL)
        {
            // Handle label.
            if (m_cur_section == Section::NONE)
            {
                ERROR ("Assembler::assemble() - Label must be located in a section.");
                m_state = State::ASSEMBLER_ERROR;
                break;
            }

            // The symbol name depends on its scope level. This allows for nested scopes to have
            // the same label names while refering to different locations in code.
            // However if two symbols with the same name are in the same scope block, this will
            // cause the later symbol to overshadow the prior symbol and likely is unintended.
            // TODO: Warn the user if this is the case. Keep track at each scope level what are
            // the registered labels thus far.
            const std::string symbol =
                token.value.substr (0, token.value.size () - 1)
                + (m_scopes.empty () ? "" : "::SCOPE:" + std::to_string (m_scopes.back ()));

            // Track the offset in the section that this label is in.
            if (m_cur_section == Section::TEXT)
            {
                m_obj.add_symbol (symbol, m_obj.get_text_section_size (),
                                  ObjectFile::SymbolTableEntry::BindingInfo::LOCAL, 0);
            }
            else if (m_cur_section == Section::DATA)
            {
                m_obj.add_symbol (symbol, m_obj.get_data_section_size (),
                                  ObjectFile::SymbolTableEntry::BindingInfo::LOCAL, 1);
            }
            else if (m_cur_section == Section::BSS)
            {
                m_obj.add_symbol (symbol, m_obj.get_bss_section_size (),
                                  ObjectFile::SymbolTableEntry::BindingInfo::LOCAL, 2);
            }
            else
            {
                ERROR ("Assembler::assemble() - Label %s is not located in a valid "
                       "section. Valid sections are TEXT, DATA, and BSS",
                       token.value.c_str ());
                m_state = State::ASSEMBLER_ERROR;
                break;
            }
            m_tokenizer.consume ();
        }
        else if (m_instruction_handlers.find (token.type) != m_instruction_handlers.end ())
        {
            // Handle instruction.
            if (m_cur_section != Section::TEXT)
            {
                ERROR ("Assembler::assemble() - Code must be located in .text section.");
                m_state = State::ASSEMBLER_ERROR;
                break;
            }
            (this->*m_instruction_handlers[token.type]) ();
        }
        else if (m_directive_handlers.find (token.type) != m_directive_handlers.end ())
        {
            // Handle assembler directive.
            (this->*m_directive_handlers[token.type]) ();
        }
        else
        {
            // Unknown token.
            ERROR ("Assembler::assemble() - Cannot parse token %d %s", m_tokenizer.get_toki (),
                   token.to_string ().c_str ());
            m_state = State::ASSEMBLER_ERROR;
            break;
        }
    }
    DEBUG ("Assembler::assemble() - Finished parsing tokens.");

    // If there was a warning, the object file is still valid.
    if (m_state == State::ASSEMBLING || m_state == State::ASSEMBLER_WARNING)
    {
        // Parse through second time to fill in local symbol values.
        fill_local ();

        m_obj.write_object_file (m_out_obj_file);
        DEBUG ("Assembler::assemble() - Assembled file: %s", m_in_file.get_name ().c_str ());
    }

    if (m_state == State::ASSEMBLING)
    {
        m_state = State::ASSEMBLED;
    }
}

File Assembler::get_output_file () const
{
    return m_out_obj_file;
}

Assembler::State Assembler::get_state () const
{
    return this->m_state;
}

void Assembler::fill_local ()
{
    const std::vector<Tokenizer::Token> &tokens = m_tokenizer.get_tokens ();
    size_t tok_i = 0;

    DEBUG ("Assembler::fill_local() - Parsing relocation entries to fill in known values.");
    std::vector<int> local_scope;
    int local_count_scope = 0;
    for (size_t i = 0; i < m_obj.rel_text.size (); i++)
    {
        ObjectFile::RelocationEntry &rel = m_obj.rel_text.at (i);
        DEBUG ("Assembler::fill_local() - Evaluating relocation entry %s",
               m_obj.strings[m_obj.symbol_table[rel.symbol].symbol_name].c_str ());

        while (tok_i < rel.token && tok_i < tokens.size ())
        {
            if (tokens[tok_i].type == Tokenizer::ASSEMBLER_SCOPE)
            {
                local_scope.push_back (local_count_scope++);
            }
            else if (tokens[tok_i].type == Tokenizer::ASSEMBLER_SCEND)
            {
                local_scope.pop_back ();
            }

            tok_i++;
        }

        // First find if symbol is defined in local scope.
        ObjectFile::SymbolTableEntry symbol_entry;
        bool found_local = false;
        std::string symbol = m_obj.strings[m_obj.symbol_table[rel.symbol].symbol_name];
        for (size_t scopeI = local_scope.size () - 1; scopeI + 1 != 0; scopeI--)
        {
            std::string local_symbol_name =
                symbol + "::SCOPE:" + std::to_string (local_scope[scopeI]);
            if (m_obj.string_table.find (local_symbol_name) == m_obj.string_table.end ())
            {
                continue;
            }

            symbol_entry = m_obj.symbol_table[m_obj.string_table[local_symbol_name]];
            found_local = true;
            break;
        }

        if (!found_local)
        {
            if (m_obj.symbol_table.at (rel.symbol).binding_info
                    != ObjectFile::SymbolTableEntry::BindingInfo::WEAK
                && m_obj.symbol_table.at (rel.symbol).section == m_obj.section_table[".text"])
            {
                symbol_entry = m_obj.symbol_table.at (rel.symbol);
            }
            else
            {
                continue;
            }
        }
        else
        {
            rel.symbol = symbol_entry.symbol_name;
            continue;
        }

        // Only fixes relative offsets, we cannot fix absolute references since
        // that must be done when the executable is loaded into memory.
        switch (rel.type)
        {
        case ObjectFile::RelocationEntry::Type::R_EMU32_B_OFFSET22:
            EXPECT_TRUE_SS ((symbol_entry.symbol_value & 0b11) == 0,
                            std::stringstream ()
                                << "Assembler::fill_local() - Expected relocation value for "
                                   "R_EMU32_B_OFFSET22 to be 4 byte aligned. Got "
                                << symbol_entry.symbol_value);
            m_obj.text_section[rel.offset / 4] =
                mask_0 (m_obj.text_section[rel.offset / 4], 0, 22)
                + bitfield_unsigned (
                    bitfield_signed (symbol_entry.symbol_value, 2, 22) - rel.offset / 4, 0, 22);
            break;
        case ObjectFile::RelocationEntry::Type::UNDEFINED:
        default:
            ERROR ("Assembler::fill_local() - Unknown relocation entry type.");
        }

        // For now, simply delete from vector.
        // TODO: In future look to optimize.
        m_obj.rel_text.erase (m_obj.rel_text.begin () + i);

        // Offset the for loop increment.
        i--;
    }

    DEBUG ("Assembler::fill_local() - Finished parsing relocation entries.");
}
