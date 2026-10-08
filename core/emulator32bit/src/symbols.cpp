#include "emulator32bit/symbols.h"

#include <format>

void SymbolMap::add(const std::string &name, const word address)
{
    m_by_address.emplace(address, name);
    m_by_name.emplace(name, address);
}

std::optional<word> SymbolMap::find(const std::string &name) const
{
    const auto it = m_by_name.find(name);
    if (it == m_by_name.end())
    {
        return std::nullopt;
    }
    return it->second;
}

bool SymbolMap::has_symbol_at(const word address) const
{
    return m_by_address.contains(address);
}

std::string SymbolMap::describe(const word address) const
{
    auto it = m_by_address.upper_bound(address);
    if (it == m_by_address.begin())
    {
        return "";
    }
    --it;

    if (it->first == address)
    {
        return it->second;
    }
    return std::format("{}+{:#x}", it->second, address - it->first);
}
