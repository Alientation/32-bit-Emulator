#pragma once

#include "util/common.h"
#include "util/types.h"

#include <cstring>
#include <string>

///
/// @brief                  Static string class. Will not dynamically resize, instead truncates.
/// @todo                   TODO: we should warn if this happens, I think we can bring in the logger here.
/// @todo                   TODO: easy string conversions from basic types. ints, floats, etc.
///
/// @tparam kMaxLength      Max characters that this can hold excluding the null terminator.
///
template<U32 kMaxLength = 63>
class ShortString
{
  public:
    ///
    /// @brief              Constructs an empty string.
    ///
    inline ShortString () noexcept :
        m_len (0)
    {
        m_str[0] = '\0';
    }

    ///
    /// @brief              Constructs a string from a c string.
    ///                     Truncates the string to fit within the buffer.
    ///
    /// @param str          C style string.
    ///
    inline ShortString (const char *__restrict__ str) noexcept
    {
        const size_t slen = strlen (str);
        m_len = (slen > kMaxLength) ? kMaxLength : slen;
        memcpy (m_str, str, m_len);
        m_str[m_len] = '\0';
    }

    ///
    /// @brief              Compile time construction of a ShortString from a C string.
    ///
    /// @tparam kN          Length of compile time C string.
    ///
    /// @param str          C style string.
    ///
    template<U32 kN>
    inline constexpr ShortString (const char (&str)[kN]) noexcept :
        m_len (kN - 1)
    {
        static_assert (kN > 0, "String cannot be empty.");
        static_assert (kMaxLength >= kN, "String literal is too long for ShortString capacity.");

        for (U32 i = 0; i < kN - 1; i++)
        {
            m_str[i] = str[i];
        }
        m_str[m_len] = '\0';
    }

    ///
    /// @brief              Copy constructs a string of the same capacity.
    ///
    /// @param other        Other ShortString.
    ///
    inline ShortString (const ShortString &other) noexcept :
        m_len (other.len ())
    {
        // No need to truncate since both have the same buffer size.
        memcpy (m_str, other.str (), m_len);
        m_str[m_len] = '\0';
    }

    ///
    /// @brief              Copy constructs a string of a different capacity.
    ///                     Truncates the string to fit within the buffer.
    ///
    /// @tparam kMaxLength2 Max length of the other string.
    ///
    /// @param other        Other ShortString.
    template<U32 kMaxLength2>
    inline ShortString (const ShortString<kMaxLength2> &other) noexcept
    {
        m_len = (other.len () > kMaxLength) ? kMaxLength : other.len ();
        memcpy (m_str, other.str (), m_len);
        m_str[m_len] = '\0';
    }

    ///
    /// @brief              Get length of string not including null terminator.
    ///
    /// @return             String length.
    ///
    inline U32 len () const noexcept
    {
        return m_len;
    }

    ///
    /// @brief              Get c string.
    ///                     Gauranteed to be null terminated.
    ///
    /// @return             C string.
    ///
    inline const char *str () const noexcept
    {
        return m_str;
    }

    ///
    /// @brief              Substring starting at pos through the end of the string.
    ///                     If pos is outside the string, substring is the empty string.
    ///
    /// @param pos          Position of the substring.
    ///
    /// @return             This.
    ///
    inline ShortString &substring (const U32 pos)
    {
        if (UNLIKELY (pos >= m_len))
        {
            m_len = 0;
            m_str[m_len] = '\0';
            return *this;
        }

        memmove (m_str, m_str + pos, m_len - pos);
        m_len -= pos;
        m_str[m_len] = '\0';
        return *this;
    }

    ///
    /// @brief              Substring at pos with length of len.
    ///                     If pos is outside the string, substring is the empty string.
    ///                     Length is truncated to valid length.
    ///
    /// @param pos          Position of the substring.
    /// @param len          Length of the substring.
    ///
    /// @return             This.
    ///
    inline ShortString &substring (const U32 pos, const U32 len)
    {
        if (UNLIKELY (pos >= m_len))
        {
            m_len = 0;
            m_str[m_len] = '\0';
            return *this;
        }

        const U32 new_len = (pos + len > m_len) ? (m_len - pos) : len;
        memmove (m_str, m_str + pos, new_len);
        m_len = new_len;
        m_str[m_len] = '\0';
        return *this;
    }

    ///
    /// @brief              Finds the position of the first occurence of a string.
    ///
    /// @tparam kMaxPatternLength   Max length of the pattern string.
    ///
    /// @param pattern      Pattern string.
    ///
    /// @return             Position of first occurence, U32(-1) if not found.
    ///
    template<U32 kMaxPatternLength>
    inline U32 find (const ShortString<kMaxPatternLength> &pattern)
    {
        return find_from (pattern, 0);
    }

    ///
    /// @brief              Finds the position of the first occurence of a string after pos.
    ///
    /// @tparam kMaxPatternLength   Max length of the pattern string.
    ///
    /// @param pattern      Pattern string.
    /// @param pos          Position to start searching for occurence.
    ///
    /// @return             Position of first occurence after pos, U32(-1) if not found.
    ///
    template<U32 kMaxPatternLength>
    inline U32 find_from (const ShortString<kMaxPatternLength> &pattern, const U32 pos)
    {
        for (U32 i = pos; i < m_len - pattern.len (); i++)
        {
            bool matches = true;
            for (U32 j = 0; j < pattern.len () && matches; j++)
            {
                matches = m_str[i + j] == pattern.str ()[j];
            }

            if (matches)
            {
                return i;
            }
        }

        return U32 (-1);
    }

    
    inline ShortString split (const U32 pos, const U32 len = 0)
    {
        return "";
    }

    template<U32 kMaxPatternLength>
    inline ShortString split (const ShortString<kMaxPatternLength> &pattern)
    {
        return "";
    }

    template<U32 kMaxPatternLength>
    inline ShortString split_from (const ShortString<kMaxPatternLength> &pattern, const U32 pos)
    {
        return "";
    }

    ///
    /// @brief              Inserts a string at a position.
    ///                     If position is past the length of the string (including null terminator)
    ///                     spaces are inserted. Truncated to fit inside buffer.
    ///
    /// @tparam kMaxInsertLength    Max buffer size of insert string.
    ///
    /// @param insert       String to insert.
    /// @param pos          Positon to insert at. Insert string will begin at this position.
    ///
    /// @return             This.
    ///
    template<U32 kMaxInsertLength>
    inline ShortString &insert (const ShortString<kMaxInsertLength> &insert, const U32 pos)
    {
        if (pos > m_len)
        {
            for (; m_len < pos && m_len < kMaxLength; m_len++)
            {
                m_str[m_len] = '\0';
            }

            *this += insert;
            return *this;
        }

        const U32 add_len = (pos + insert.len () > kMaxLength) ? kMaxLength - pos : insert.len ();
        memmove (m_str + pos, m_str + pos + add_len, m_len - pos);
        memcpy (m_str + pos, insert.str (), add_len);
        m_len += add_len;
        m_str[m_len] = '\0';
        return *this;
    }

    ///
    /// @brief              Removes a section of the string.
    ///                     If pos is outside of the string, then no operation is performed.
    ///                     If the specified section exceeds the string length, length of section
    ///                     is reduced.
    ///
    /// @param pos          Position of the section to remove.
    /// @param len          Length of sectio nto remove.
    ///
    /// @return             This.
    ///
    inline ShortString &remove (const U32 pos, const U32 len)
    {
        return replace (pos, len, ShortString (""));
    }

    ///
    /// @brief              Replace section of string.
    ///                     Truncates end of string to fit inside the internal buffer.
    ///
    ///                     If pos is outside of the string, then no operation is performed.
    ///                     If the specified section exceeds the string length, length of section
    ///                     is reduced.
    ///
    /// @tparam kMaxReplacementLength   Size of buffer in replacement string.
    ///
    /// @param pos          Position of substring to replace.
    /// @param len          Length of substring to replace.
    /// @param replacement  Replacement string.
    ///
    /// @return             This.
    ///
    template<U32 kMaxReplacementLength>
    inline ShortString &replace (const U32 pos, const U32 len,
                                 const ShortString<kMaxReplacementLength> &replacement)
    {
        const U32 replace_len = replacement.len ();

        if (pos > m_len)
        {
            return *this;
        }

        const U32 section_len = (pos + len > m_len) ? (m_len - pos) : len;

        // If replacing with replacement string exceeds internal storage buffer.
        if (pos + replace_len > kMaxLength)
        {
            memcpy (m_str + pos, replacement.str (), kMaxLength - pos);
            m_len = kMaxLength;
            m_str[m_len] = '\0';
            return *this;
        }

        // How many characters at the end should be truncated.
        const U32 truncated =
            (replace_len > section_len && replace_len - section_len + m_len > kMaxLength)
                ? replace_len - section_len + m_len - kMaxLength
                : 0;
        const S32 diff = replace_len - section_len;

        // Shift to make just enough room for replacement string.
        memmove (m_str + pos + replace_len, m_str + pos + section_len,
                 m_len - pos - section_len - truncated);
        m_len += diff - truncated;
        memcpy (m_str + pos, replacement.str (), replace_len);
        m_str[m_len] = '\0';

        return *this;
    }

    ///
    /// @brief              Replace all occurences of the pattern.
    ///                     Truncates to fit inside the internal buffer.
    ///
    /// @tparam kMaxPatternLength       Size of buffer in pattern string.
    /// @tparam kMaxReplacementLength   Size of buffer in replacement string.
    ///
    /// @param pattern      Pattern to replace.
    /// @param replacement  Replacement string.
    ///
    /// @return             This.
    ///
    template<U32 kMaxPatternLength, U32 kMaxReplacementLength>
    inline ShortString &replace_all (const ShortString<kMaxPatternLength> &pattern,
                                     const ShortString<kMaxReplacementLength> &replacement)
    {
        const U32 pat_len = pattern.len ();
        const U32 replace_len = replacement.len ();

        if (pat_len == 0 || pat_len > m_len)
        {
            return *this;
        }

        // Temporary buffer to contain the final string.
        ShortString new_str;
        for (U32 i = 0; i <= m_len - pat_len && new_str.m_len != kMaxLength; i++)
        {
            // Whether the patern matches the substring starting at character i.
            bool matches = true;
            for (U32 j = 0; j < pat_len && matches; j++)
            {
                matches = m_str[i + j] == pattern.str ()[j];
            }

            if (matches)
            {
                // Add the replacement string and truncate.
                const U32 add_len = (new_str.m_len + replace_len > kMaxLength)
                                        ? (kMaxLength - new_str.m_len)
                                        : replace_len;

                memcpy (new_str.m_str + new_str.m_len, replacement.str (), add_len);
                new_str.m_len += add_len;

                // Skip past the pattern characters.
                i += pat_len - 1;
            }
            else
            {
                // Keep the character.
                new_str.m_str[new_str.m_len++] = m_str[i];
            }
        }

        // Copy end of string.
        for (U32 i = m_len - pat_len + 1; i < m_len && new_str.m_len != kMaxLength; i++)
        {
            new_str.m_str[new_str.m_len++] = m_str[i];
        }

        memcpy (m_str, new_str.m_str, new_str.m_len);
        m_len = new_str.m_len;
        m_str[m_len] = '\0';
        return *this;
    }

    ///
    /// @brief              Replace first occurence of the pattern.
    ///                     Truncates to fit inside the internal buffer.
    ///
    /// @tparam kMaxPatternLength       Size of buffer in pattern string.
    /// @tparam kMaxReplacementLength   Size of buffer in replacement string.
    ///
    /// @param pattern      Pattern to replace.
    /// @param replacement  Replacement string.
    ///
    /// @return             This.
    ///
    template<U32 kMaxPatternLength, U32 kMaxReplacementLength>
    inline ShortString &replace_first (const ShortString<kMaxPatternLength> &pattern,
                                       const ShortString<kMaxReplacementLength> &replacement)
    {
        const U32 pat_len = pattern.len ();

        if (pat_len == 0 || pat_len > m_len)
        {
            return *this;
        }

        U32 pos;
        for (pos = 0; pos <= m_len - pat_len; pos++)
        {
            // Whether the patern matches the substring starting at pos.
            bool matches = true;
            for (U32 j = 0; j < pat_len && matches; j++)
            {
                matches = m_str[pos + j] == pattern.str ()[j];
            }

            if (matches)
            {
                break;
            }
        }

        return replace (pos, pat_len, replacement);
    }

    ///
    /// @brief              Replace last occurence of the pattern.
    ///                     Truncates to fit inside the internal buffer.
    ///
    /// @tparam kMaxPatternLength       Size of buffer in pattern string.
    /// @tparam kMaxReplacementLength   Size of buffer in replacement string.
    ///
    /// @param pattern      Pattern to replace.
    /// @param replacement  Replacement string.
    ///
    /// @return             This.
    ///
    template<U32 kMaxPatternLength, U32 kMaxReplacementLength>
    inline ShortString &replace_last (const ShortString<kMaxPatternLength> &pattern,
                                      const ShortString<kMaxReplacementLength> &replacement)
    {
        const U32 pat_len = pattern.len ();

        if (pat_len == 0 || pat_len > m_len)
        {
            return *this;
        }

        U32 pos;
        for (pos = m_len - pat_len; pos != U32 (-1); pos--)
        {
            // Whether the patern matches the substring starting at pos.
            bool matches = true;
            for (U32 j = 0; j < pat_len && matches; j++)
            {
                matches = m_str[pos + j] == pattern.str ()[j];
            }

            if (matches)
            {
                break;
            }
        }

        return replace (pos, pat_len, replacement);
    }

    ///
    /// @brief              Repeat operator.
    ///                     New string contains the current string repeated a number of times,
    ///                     truncated to within the buffer.
    ///
    /// @param rhs          Number of times to repeat.
    ///
    /// @return             This.
    ///
    inline ShortString &operator*= (const U32 rhs) noexcept
    {
        const U32 original_len = m_len;
        const U32 target_len = m_len * rhs;
        m_len = (target_len > kMaxLength) ? kMaxLength : target_len;

        // How many full copies can be made.
        const U32 max_copies = m_len / original_len;
        for (U32 i = 1; i < max_copies; i++)
        {
            memcpy (m_str + i * original_len, m_str, original_len);
        }

        // Fill in the last partial copy (if there was one).
        const U32 start_partial_copy = max_copies * original_len;
        for (U32 i = start_partial_copy; i < m_len; i++)
        {
            m_str[i] = m_str[i - start_partial_copy];
        }

        m_str[m_len] = '\0';
        return *this;
    }

    ///
    /// @brief              Concatenation operator.
    ///                     Adds a ShortString to this and truncates if necessary.
    ///
    /// @tparam kMaxLength2 Buffer size of the string to add.
    ///
    /// @param rhs          ShortString to add.
    ///
    /// @return             This.
    ///
    template<U32 kMaxLength2>
    inline ShortString &operator+= (const ShortString<kMaxLength2> &rhs) noexcept
    {
        const U32 add_len = (m_len + rhs.len ()) > kMaxLength ? (kMaxLength - m_len) : rhs.len ();
        memcpy (m_str + m_len, rhs.str (), add_len);
        m_len += add_len;
        m_str[m_len] = '\0';
        return *this;
    }

    ///
    /// @brief              Concatenation operator.
    ///                     Adds a c style string to this and truncates if necessary.
    ///
    /// @param rhs          C style string to add.
    ///
    /// @return             This.
    ///
    inline ShortString &operator+= (const char *__restrict__ const rhs) noexcept
    {
        const size_t slen = strlen (rhs);
        const U32 add_len = (m_len + slen) > kMaxLength ? (kMaxLength - m_len) : slen;
        memcpy (m_str + m_len, rhs, add_len);
        m_len += add_len;
        m_str[m_len] = '\0';
        return *this;
    }

  private:
    /// @brief              Internal buffer for string.
    ///                     Sized to accomodate ending null terminator.
    char m_str[kMaxLength + 1];

    /// @brief              Length of string, not including null terminator.
    U32 m_len;
};

///
/// @brief              Repeat operator.
///
/// @tparam kMaxLength1 Buffer size of first string.
///
/// @param lhs          R-value reference to temporary object to repeat.
/// @param rhs          Number of times to repeat.
///
/// @return             New ShortString object. Same buffer size as the lhs string.
///
template<U32 kMaxLength1>
inline ShortString<kMaxLength1> operator* (ShortString<kMaxLength1> &&lhs, const U32 rhs) noexcept
{
    lhs *= rhs;
    return lhs;
}

///
/// @brief              Repeat operator.
///
/// @tparam kMaxLength1 Buffer size of first string.
///
/// @param lhs          Reference to ShortString to repeat, not modified.
/// @param rhs          Number of times to repeat.
///
/// @return             New ShortString object. Same buffer size as the lhs string.
///
template<U32 kMaxLength1>
inline ShortString<kMaxLength1> operator* (const ShortString<kMaxLength1> &lhs,
                                           const U32 rhs) noexcept
{
    ShortString<kMaxLength1> ss = lhs;
    ss *= rhs;
    return ss;
}

///
/// @brief              Concatenation operator.
///                     Adds a C style string to a ShortString.
///
/// @tparam kMaxLength2 Buffer size of second string.
///
/// @param lhs          C style string.
/// @param rhs          Reference to string object on the right hand side of the operator.
///
/// @return             New ShortString. Same buffer size as rhs.
///
template<U32 kMaxLength2>
inline ShortString<kMaxLength2> operator+ (const char *const lhs,
                                           const ShortString<kMaxLength2> &rhs) noexcept
{
    ShortString<kMaxLength2> ss (lhs);
    ss += rhs;
    return ss;
}

///
/// @brief              Concatenation operator.
///                     Adds two ShortStrings together.
///
/// @tparam kMaxLength1 Buffer size of first string.
/// @tparam kMaxLength2 Buffer size of second string.
///
/// @param lhs          R-value reference to the temporary string object on the left hand
///                     side of the operator.
/// @param rhs          Reference to string object on the right hand side of the operator.
///
/// @return             New ShortString. Same buffer size as lhs.
///
template<U32 kMaxLength1, U32 kMaxLength2>
inline ShortString<kMaxLength1> operator+ (ShortString<kMaxLength1> &&lhs,
                                           const ShortString<kMaxLength2> &rhs) noexcept
{
    lhs += rhs;
    return lhs;
}

///
/// @brief              Concatenation operator.
///                     Adds two ShortStrings together.
///
/// @tparam kMaxLength1 Buffer size of first string.
/// @tparam kMaxLength2 Buffer size of second string.
///
/// @param lhs          Reference to string object on the left hand side of the operator.
/// @param rhs          Reference to string object on the right hand side of the operator.
///
/// @return             New ShortString. Same buffer size as lhs.
///
template<U32 kMaxLength1, U32 kMaxLength2>
inline ShortString<kMaxLength1> operator+ (const ShortString<kMaxLength1> &lhs,
                                           const ShortString<kMaxLength2> &rhs) noexcept
{
    ShortString ss = lhs;
    ss += rhs;
    return ss;
}

///
/// @brief              Adds a C style string to a ShortString.
///                     Avoids an extra copy with the user having to convert the C style string
///                     to a ShortString.
///
/// @tparam kMaxLength1 Buffer size of the first string.
///
/// @param lhs          R-value reference to the string object on the left hand side of the operator.
/// @param rhs          C style string on the right hand side of the operator.
///
/// @return             New ShortString. Same buffer size as rhs string.
///
template<U32 kMaxLength1>
inline ShortString<kMaxLength1> operator+ (ShortString<kMaxLength1> &&lhs,
                                           const char *__restrict__ const rhs) noexcept
{
    lhs += rhs;
    return lhs;
}

///
/// @brief              Adds a C style string to a ShortString.
///                     Avoids an extra copy with the user having to convert the C style string
///                     to a ShortString.
///
/// @tparam kMaxLength1 Buffer size of the first string.
///
/// @param lhs          Reference to the string object on the left hand side of the operator.
/// @param rhs          C style string on the right hand side of the operator.
///
/// @return             New ShortString. Same buffer size as lhs string.
///
template<U32 kMaxLength1>
inline ShortString<kMaxLength1> operator+ (const ShortString<kMaxLength1> &lhs,
                                           const char *__restrict__ const rhs) noexcept
{
    ShortString<kMaxLength1> ss (lhs);
    ss += rhs;
    return ss;
}

/// @brief              Class Template Argument Deduction to guide the compiler to
///                     reserve only necessary space for compile time c strings.
template<U32 N>
ShortString (const char (&str)[N]) -> ShortString<N - 1>;