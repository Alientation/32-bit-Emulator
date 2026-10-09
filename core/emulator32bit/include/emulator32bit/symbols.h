#pragma once

#include "util/types.h"

#include <map>
#include <optional>
#include <string>

/// Names for addresses, used by the trace and the debugger to show `main+0x8` instead of a number.
class SymbolMap
{
  public:
    /// Adds a symbol. If another symbol is already at the address, the first one is kept for
    /// `describe` (both can be looked up by name).
    ///
    /// @param name the name of the symbol
    /// @param address the address it stands for
    void add(const std::string &name, word address);

    /// Looks a symbol up by name.
    ///
    /// @param name the name of the symbol
    /// @return the address of the symbol, or nothing if there is none
    std::optional<word> find(const std::string &name) const;

    /// @param address an address
    /// @return whether a symbol is exactly at the address
    bool has_symbol_at(word address) const;

    /// Names an address by the closest symbol at or below it.
    ///
    /// @param address an address
    /// @return `name` or `name+0xOFFSET`, empty if there is no such symbol
    std::string describe(word address) const;

    /// @return whether there are no symbols
    bool empty() const
    {
        return m_by_name.empty();
    }

  private:
    std::map<word, std::string> m_by_address;
    std::map<std::string, word> m_by_name;
};
