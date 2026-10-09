#pragma once

#include <regex>
#include <string>
#include <vector>

namespace string_util
{
/// Repeats a string.
///
/// @param str the string to repeat
/// @param times how many copies to concatenate
/// @return `times` copies of the string
inline static std::string repeat(std::string str, int times)
{
    std::string res;
    for (int i = 0; i < times; i++)
    {
        res += str;
    }
    return res;
}

/// Replaces every occurrence of a pattern. The text that is put in is not searched again.
///
/// @param str the string to change
/// @param pattern the text to replace
/// @param replacement the text to put in its place
/// @return the changed string
inline static std::string replace_all(std::string str, const std::string &pattern,
                                      const std::string &replacement)
{
    size_t pos;
    while ((pos = str.find(pattern)) != std::string::npos)
    {
        str.replace(pos, pattern.size(), replacement);
    }
    return str;
}

/// Replaces `match.length()` characters, from the first position where any character of `match`
/// occurs, with `replacement`.
///
/// @param str the string to change
/// @param match the characters to look for
/// @param replacement the text to put in their place
/// @return the changed string, or the string as it was if no character of `match` occurs
inline static std::string replaceFirst(std::string str, const std::string &match,
                                       const std::string &replacement)
{
    size_t index = str.find_first_of(match);
    if (index == std::string::npos)
    {
        return str;
    }

    return str.replace(index, match.length(), replacement);
}

/// Trims whitespace from the left side of a string
///
/// @param str the string to trim
///
/// @return the trimmed string
inline static std::string leftTrim(std::string str)
{
    str.erase(str.begin(), std::find_if(str.begin(), str.end(),
                                        [](unsigned char c) { return !std::isspace(c); }));
    return str;
}

/// Trims whitespace from the right side of a string
///
/// @param str the string to trim
///
/// @return the trimmed string
inline static std::string rightTrim(std::string str)
{
    str.erase(
        std::find_if(str.rbegin(), str.rend(), [](unsigned char c) { return !std::isspace(c); })
            .base(),
        str.end());
    return str;
}

/// Trims whitespace from the left and right side of a string
///
/// @param str the string to trim
///
/// @return the trimmed string
inline static std::string trimString(std::string str)
{
    return leftTrim(rightTrim(str));
}

/// Trims whitespace from the left and right side of a string
///
/// @param str the string to trim
/// @param leftTrim the number of characters to trim from the left side of the string
/// @param rightTrim the number of characters to trim from the right side of the string
///
/// @return the trimmed string
inline static std::string trimString(std::string str, int leftTrim, int rightTrim)
{
    return str.substr(leftTrim, str.length() - rightTrim);
}

/// Splits a string into a vector of strings separated by the given regex delimiter.
///
/// @param str the string to split
/// @param delimiter the regex delimiter to split the string by
/// @param trim whether or not to trim each split string
///
/// @return a vector of strings separated by the given regex delimiter
inline static std::vector<std::string> split(std::string str, std::string delimRegex,
                                             bool trim = false)
{
    std::vector<std::string> result;

    if (str.empty())
    {
        return result;
    }

    std::regex rgx(delimRegex);
    std::sregex_token_iterator iter(str.begin(), str.end(), rgx, -1);
    std::sregex_token_iterator end;

    while (iter != end)
    {
        std::string token = *iter;
        if (trim)
        {
            token = trimString(token);
        }

        result.push_back(token);
        ++iter;
    }

    return result;
}

/// Puts `value` in the place of the first `{...}` that is left in `str`, writes what comes before
/// it to `oss` and removes everything up to and including the `}` from `str`.
template<typename T>
inline static void format_helper(std::ostringstream &oss, std::string &str, const T &value)
{
    std::size_t openBracket = str.find('{');
    if (openBracket == std::string::npos)
    {
        return;
    }
    std::size_t closeBracket = str.find('}', openBracket + 1);
    if (closeBracket == std::string::npos)
    {
        return;
    }
    oss << str.substr(0, openBracket) << value;
    str = str.substr(closeBracket + 1);
}

/// Formats like `std::format`, except that whatever is between the braces is ignored: each `{...}`
/// is replaced by the next argument, in order.
///
/// @param str the format string
/// @param args the values to put in
/// @return the formatted string
template<class... Targ>
inline static std::string format(std::string str, Targ &&...args)
{
    std::ostringstream oss;
    (format_helper(oss, str, args), ...);
    oss << str;
    return oss.str();
}
} // namespace string_util