#pragma once

#include "util/types.h"

#include <map>
#include <optional>
#include <string>

/// The name of a symbol as it is written in the source. The assembler and the linker add to a name
/// what tells apart two symbols that are called the same: `name::SCOPE:<scope number>` for a label
/// in a `.scope` and `name:LOCAL:<file number>` for a symbol that is private to its file.
///
/// @param[in] linked_name The name of a symbol in an executable.
/// @return The name up to the first colon, which a name in the source does not have.
std::string source_name(const std::string &linked_name);

/// Names for addresses, used by the trace and the debugger to show `main+0x8` instead of a number.
class SymbolMap
{
  public:
    /// Adds a symbol. If another symbol is already at the address, the first one is kept for
    /// `describe` (both can be looked up by name).
    ///
    /// @param[in] name The name of the symbol.
    /// @param[in] address The address it stands for.
    void add(const std::string &name, word address);

    /// Looks a symbol up by name.
    ///
    /// @param[in] name The name of the symbol.
    /// @return The address of the symbol, or nothing if there is none.
    std::optional<word> find(const std::string &name) const;

    /// @param[in] address An address.
    /// @return Whether a symbol is exactly at the address.
    bool has_symbol_at(word address) const;

    /// Names an address by the closest symbol at or below it.
    ///
    /// @param[in] address An address.
    /// @return `name` or `name+0xOFFSET`, empty if there is no such symbol.
    std::string describe(word address) const;

    /// @return Whether there are no symbols.
    bool empty() const
    {
        return m_by_name.empty();
    }

  private:
    using ByAddress = std::map<word, std::string>;
    using ByName = std::map<std::string, word>;

    ByAddress m_by_address;
    ByName m_by_name;
};
