#include "util/types.h"


std::string to_hex_str(u64 hex, int digits)
{
    std::stringstream stream;
    stream << HEXADECIMAL_PREFIX << std::setfill('0') << std::setw(digits) << std::hex << hex;
    std::string result(stream.str());
    return result;
}

std::string to_hex_str(u32 hex)
{
    return to_hex_str(hex,8);
}

std::string to_hex_str(u16 hex)
{
    return to_hex_str(hex,4);
}

std::string to_hex_str(u8 hex)
{
    return to_hex_str(hex,2);
}

std::string to_color_hex_str(u64 hex, int digits)
{
	return color_val_str(to_hex_str(hex, digits));
}

std::string to_color_hex_str(u32 hex)
{
	return to_color_hex_str(hex, 8);
}

std::string to_color_hex_str(u16 hex)
{
	return to_color_hex_str(hex, 4);
}

std::string to_color_hex_str(u8 hex)
{
	return to_color_hex_str(hex, 2);
}

std::string color_val_str(std::string string)
{
    std::string result = ccolor::GRAY;
    auto iterator = string.begin();
    if ((*iterator) == '$' || (*iterator) == '%' || (*iterator) == '#' || (*iterator) == '@') {
        iterator++;
    }

	// gray out any leading zeros except for the last zero
    while ((*iterator) == '0' && iterator != string.end() - 1) {
        result += *iterator;
        iterator++;
    }

	// bold the remaining digits
    result += ccolor::BOLD_WHITE;
    while (iterator != string.end()) {
        result += *iterator;
        iterator++;
    }

    result += ccolor::RESET;
    return result;
}

std::string to_bin_str(u64 bin)
{
    std::stringstream stream;
    stream << BINARY_PREFIX << std::bitset<64>(bin);
    std::string result(stream.str());
    return result;
}

std::string to_bin_str(u32 bin)
{
    std::stringstream stream;
    stream << BINARY_PREFIX << std::bitset<32>(bin);
    std::string result(stream.str());
    return result;
}

std::string to_bin_str(u16 bin)
{
    std::stringstream stream;
    stream << BINARY_PREFIX << std::bitset<16>(bin);
    std::string result(stream.str());
    return result;
}

std::string to_bin_str(u8 bin)
{
    std::stringstream stream;
    stream << BINARY_PREFIX << std::bitset<8>(bin);
    std::string result(stream.str());
    return result;
}