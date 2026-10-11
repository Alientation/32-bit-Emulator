#include "util/types.h"

#include "util/console_color.h"

#include <iomanip>
#include <sstream>

std::string to_hex_str(const U64 hex, const int digits)
{
    std::stringstream stream;
    stream << kHexPrefix << std::setfill('0') << std::setw(digits) << std::hex << hex;
    return stream.str();
}

std::string to_hex_str(const U32 hex)
{
    return to_hex_str(hex, kWordHexDigits);
}

std::string to_color_hex_str(const U64 hex, const int digits)
{
    return color_val_str(to_hex_str(hex, digits));
}

std::string to_color_hex_str(const U32 hex)
{
    return to_color_hex_str(hex, kWordHexDigits);
}

std::string color_val_str(const std::string &string)
{
    std::string result = ccolor::GRAY;
    auto iterator = string.begin();

    // The prefix is gray with the leading zeros.
    if (string.starts_with(kHexPrefix))
    {
        result += kHexPrefix;
        iterator += std::string_view(kHexPrefix).size();
    }

    // Gray out the leading zeros, except for the last digit.
    while (iterator + 1 < string.end() && *iterator == '0')
    {
        result += *iterator;
        iterator++;
    }

    // Bold the remaining digits.
    result += ccolor::BOLD_WHITE;
    while (iterator != string.end())
    {
        result += *iterator;
        iterator++;
    }

    result += ccolor::RESET;
    return result;
}
