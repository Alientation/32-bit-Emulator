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
    void add(const std::string &name, word address);

    /// The address of a symbol.
    std::optional<word> find(const std::string &name) const;

    /// Whether a symbol is exactly at the address.
    bool has_symbol_at(word address) const;

    /// `name` or `name+0xOFFSET` for the closest symbol at or below the address, empty if there is
    /// none.
    std::string describe(word address) const;

    bool empty() const
    {
        return m_by_name.empty();
    }

  private:
    std::map<word, std::string> m_by_address;
    std::map<std::string, word> m_by_name;
};
