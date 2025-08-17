#include "util/types.h"

std::string to_hex_str (U64 hex, int digits)
{
    std::stringstream stream;
    stream << kHexPrefix << std::setfill ('0') << std::setw (digits) << std::hex << hex;
    std::string result (stream.str ());
    return result;
}

std::string to_hex_str (U32 hex)
{
    return to_hex_str (hex, 8);
}

std::string to_hex_str (U16 hex)
{
    return to_hex_str (hex, 4);
}

std::string to_hex_str (U8 hex)
{
    return to_hex_str (hex, 2);
}

std::string to_color_hex_str (U64 hex, int digits)
{
    return color_val_str (to_hex_str (hex, digits));
}

std::string to_color_hex_str (U32 hex)
{
    return to_color_hex_str (hex, 8);
}

std::string to_color_hex_str (U16 hex)
{
    return to_color_hex_str (hex, 4);
}

std::string to_color_hex_str (U8 hex)
{
    return to_color_hex_str (hex, 2);
}

std::string color_val_str (std::string string)
{
    std::string result = ccolor::GRAY;
    auto iterator = string.begin ();
    if ((*iterator) == '$' || (*iterator) == '%' || (*iterator) == '#' || (*iterator) == '@')
    {
        iterator++;
    }

    // gray out any leading zeros except for the last zero
    while ((*iterator) == '0' && iterator != string.end () - 1)
    {
        result += *iterator;
        iterator++;
    }

    // bold the remaining digits
    result += ccolor::BOLD_WHITE;
    while (iterator != string.end ())
    {
        result += *iterator;
        iterator++;
    }

    result += ccolor::RESET;
    return result;
}

std::string to_bin_str (U64 bin)
{
    std::stringstream stream;
    stream << kBinaryPrefix << std::bitset<64> (bin);
    std::string result (stream.str ());
    return result;
}

std::string to_bin_str (U32 bin)
{
    std::stringstream stream;
    stream << kBinaryPrefix << std::bitset<32> (bin);
    std::string result (stream.str ());
    return result;
}

std::string to_bin_str (U16 bin)
{
    std::stringstream stream;
    stream << kBinaryPrefix << std::bitset<16> (bin);
    std::string result (stream.str ());
    return result;
}

std::string to_bin_str (U8 bin)
{
    std::stringstream stream;
    stream << kBinaryPrefix << std::bitset<8> (bin);
    std::string result (stream.str ());
    return result;
}