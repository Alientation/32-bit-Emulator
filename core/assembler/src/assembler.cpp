#include "assembler/assembler.h"
#include "assembler/relocation.h"
#include "emulator32bit/emulator32bit.h"

#include "util/logger.h"
#include "util/types.h"

#include <fstream>
#include <optional>
#include <span>

Assembler::Assembler(const File processed_file, const std::string &output_path) :
    m_in_file(processed_file),
    m_sources(std::make_shared<basm::SourceManager>())
{
    init(processed_file, output_path);

    // Convert the input file into tokens.
    const basm::SourceId source = m_sources->add_file(processed_file.get_path());
    AEMU_CHECK(source != basm::kInvalidSource, "Assembler::Assembler() - Cannot read '{}'.",
               processed_file.get_path());
    basm::LexResult lexed = basm::lex(*m_sources, source);
    basm::fatal_if_errors(*m_sources, lexed);
    m_tokens = std::move(lexed.tokens);
    m_cursor = basm::TokenCursor(std::span<const basm::Token>(m_tokens));
}

Assembler::Assembler(const File processed_file, basm::PreprocessedSource source,
                     const std::string &output_path) :
    m_in_file(processed_file),
    m_sources(std::move(source.sources)),
    m_tokens(std::move(source.tokens))
{
    init(processed_file, output_path);
    AEMU_CHECK(m_sources != nullptr, "Assembler::Assembler() - The preprocessed source is empty.");
    m_cursor = basm::TokenCursor(std::span<const basm::Token>(m_tokens));
}

void Assembler::init(const File &processed_file, const std::string &output_path)
{
    // Create the output object file.
    if (output_path.empty())
    {
        m_out_obj_file =
            File(m_in_file.get_name(), OBJECT_EXTENSION, processed_file.get_dir_str(), true);
    }
    else
    {
        m_out_obj_file = File(output_path, true);
    }

    AEMU_CHECK(processed_file.get_extension() == PROCESSED_EXTENSION,
               "Assembler::Assembler() - Invalid processed file: {}",
               processed_file.get_extension());
}

void Assembler::assemble()
{
    if (m_assembled)
    {
        AEMU_DEBUG("Assembler::assemble() - Already assembled file: {}", m_in_file.get_name());
        return;
    }

    AEMU_DEBUG("Assembler::assemble() - Assembling file: {}", m_in_file.get_name());

    m_assembled = true;

    // Clear the object file.
    m_out_obj_file.clear();

    m_obj.file_type = ObjectFile::kRelocatableFileType;
    m_obj.target_machine = ObjectFile::kEMU32MachineId;
    m_obj.flags = 0;

    // Add appropriate sections to the object file.
    m_obj.add_section(".text", ObjectFile::SectionHeader::Type::TEXT);
    m_obj.add_section(".data", ObjectFile::SectionHeader::Type::DATA);
    m_obj.add_section(".bss", ObjectFile::SectionHeader::Type::BSS);
    m_obj.add_section(".symtab", ObjectFile::SectionHeader::Type::SYMTAB);
    m_obj.add_section(".rel.text", ObjectFile::SectionHeader::Type::REL_TEXT);
    m_obj.add_section(".rel.data", ObjectFile::SectionHeader::Type::REL_DATA);
    m_obj.add_section(".rel.bss", ObjectFile::SectionHeader::Type::REL_BSS);
    m_obj.add_section(".strtab", ObjectFile::SectionHeader::Type::STRTAB);
    for (const ObjectFile::ByteSection &section : ObjectFile::byte_sections())
    {
        if (!m_obj.section_table.count(section.name))
        {
            m_obj.add_section(section.name, section.type);
            m_obj.add_section(section.rel_name, section.rel_type);
        }
    }

    // Parse tokens.
    AEMU_DEBUG("Assembler::assemble() - Parsing tokens.");
    while (!m_cursor.at_end())
    {
        const basm::Token &token = m_cursor.peek();
        AEMU_DEBUG("Assembler::assemble() - Assembling token {}: {}", m_cursor.position(),
                   basm::describe(token));

        if (token.is(basm::TokenType::NEWLINE))
        {
            m_cursor.next();
            continue;
        }

        m_statement = &token;
        if (token.type == basm::TokenType::LABEL)
        {
            // Handle label.
            if (m_cur_section == Section::NONE)
            {
                fail(token, "label must be located in a section");
            }

            // The symbol name depends on its scope level. This allows for nested scopes to have
            // the same label names while refering to different locations in code.
            // However if two symbols with the same name are in the same scope block, this will
            // cause the later symbol to overshadow the prior symbol and likely is unintended.
            // TODO: Warn the user if this is the case. Keep track at each scope level what are
            // the registered labels thus far.
            const std::string symbol = scoped_name(token.str());
            if (m_constants.count(symbol) != 0)
            {
                fail(token, "'" + token.str() + "' is already a constant");
            }

            // Track the offset in the section that this label is in.
            m_obj.add_symbol(symbol, m_obj.get_section_size(m_cur_section_index),
                             ObjectFile::SymbolTableEntry::BindingInfo::LOCAL, m_cur_section_index);

            // A label is not a statement of its own, an instruction can follow on the same line.
            m_cursor.next();
        }
        else if (basm::is_instruction(token.type))
        {
            // Handle instruction.
            if (m_cur_section != Section::TEXT)
            {
                fail(token, "code must be located in the .text section");
            }
            assemble_instruction(basm::instruction_spec(token.type));
            expect_end_of_statement();
        }
        else if (m_directive_handlers.find(token.type) != m_directive_handlers.end())
        {
            // Handle assembler directive.
            (this->*m_directive_handlers[token.type])();
            expect_end_of_statement();
        }
        else
        {
            // Unknown token.
            fail(token, "cannot parse " + basm::describe(token));
        }
    }
    AEMU_DEBUG("Assembler::assemble() - Finished parsing tokens.");

    if (!m_scope_sites.empty() && !m_stopped)
    {
        fail(*m_scope_sites.back(), ".scope is never closed with .scend");
    }

    // Fill in the branches to labels of this file. An error above never gets here.
    fill_local();

    m_obj.write_object_file(m_out_obj_file);
    AEMU_DEBUG("Assembler::assemble() - Assembled file: {}", m_in_file.get_name());
}

File Assembler::get_output_file() const
{
    return m_out_obj_file;
}

const ObjectFile &Assembler::object() const
{
    return m_obj;
}

void Assembler::set_warnings_as_errors(bool enabled)
{
    m_warnings_as_errors = enabled;
}

void Assembler::fill_local()
{
    AEMU_DEBUG("Assembler::fill_local() - Parsing relocation entries to fill in known values.");
    fill_local(m_obj.rel_text, true);
    for (const ObjectFile::ByteSection &section : ObjectFile::byte_sections())
    {
        fill_local(m_obj.*section.relocations, false);
    }
    AEMU_DEBUG("Assembler::fill_local() - Finished parsing relocation entries.");
}

// A relocation names the label of the innermost scope that has it, if there is one, and a branch
// to a label of this file does not depend on where the file ends up, so it is filled in now.
// Absolute addresses (e.g. adrp to a .text label) are only known after linking, so those are left
// for the linker.
void Assembler::fill_local(std::vector<ObjectFile::RelocationEntry> &relocations,
                           bool fill_branches)
{
    const std::vector<basm::Token> &tokens = m_tokens;
    size_t tok_i = 0;

    std::vector<int> local_scope;
    int local_count_scope = 0;
    for (size_t i = 0; i < relocations.size(); i++)
    {
        ObjectFile::RelocationEntry &rel = relocations.at(i);
        AEMU_DEBUG("Assembler::fill_local() - Evaluating relocation entry {}",
                   m_obj.strings[m_obj.symbol_table[rel.symbol].symbol_name]);

        while (tok_i < rel.token && tok_i < tokens.size())
        {
            if (tokens[tok_i].type == basm::TokenType::ASSEMBLER_SCOPE)
            {
                local_scope.push_back(local_count_scope++);
            }
            else if (tokens[tok_i].type == basm::TokenType::ASSEMBLER_SCEND)
            {
                local_scope.pop_back();
            }

            tok_i++;
        }

        // First find if symbol is defined in local scope.
        ObjectFile::SymbolTableEntry symbol_entry;
        bool found_local = false;
        std::string symbol = m_obj.strings[m_obj.symbol_table[rel.symbol].symbol_name];
        for (size_t scopeI = local_scope.size() - 1; scopeI + 1 != 0; scopeI--)
        {
            std::string local_symbol_name =
                symbol + "::SCOPE:" + std::to_string(local_scope[scopeI]);
            if (m_obj.string_table.find(local_symbol_name) == m_obj.string_table.end())
            {
                continue;
            }

            symbol_entry = m_obj.symbol_table[m_obj.string_table[local_symbol_name]];
            found_local = true;
            break;
        }

        if (!found_local)
        {
            // A weak symbol may be replaced by another file, so the branch is the linker's.
            if (m_obj.symbol_table.at(rel.symbol).binding_info
                    != ObjectFile::SymbolTableEntry::BindingInfo::WEAK
                && m_obj.symbol_table.at(rel.symbol).binding_info
                       != ObjectFile::SymbolTableEntry::BindingInfo::WEAK_DECLARED
                && m_obj.symbol_table.at(rel.symbol).section == m_obj.section_table[".text"])
            {
                symbol_entry = m_obj.symbol_table.at(rel.symbol);
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

        if (!fill_branches || rel.type != ObjectFile::RelocationEntry::Type::R_EMU32_B_OFFSET22)
        {
            continue;
        }
        m_obj.text_section[rel.offset / 4] =
            apply_relocation(rel.type, m_obj.text_section[rel.offset / 4], rel.offset,
                             symbol_entry.symbol_value + word(rel.addend));

        // For now, simply delete from vector.
        // TODO: In future look to optimize.
        relocations.erase(relocations.begin() + i);

        // Offset the for loop increment.
        i--;
    }
}
