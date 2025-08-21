#pragma once

#include "util/common.h"
#include "util/types.h"

#include <cstring>
#include <string>

///
/// @brief                  Static string class. Will not dynamically resize.
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
    /// @todo               TODO: maybe we should warn if this happens? I think we can bring in the logger here.
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
        // TODO: temporary print.
        printf ("COPY CTOR\n");

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
        // TODO: temporary print.
        printf ("COPY CTOR\n");
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
    /// @brief
    ///
    /// @tparam kMaxLength1
    /// @tparam kMaxLength2
    ///
    /// @param pattern
    /// @param replacement
    ///
    /// @return
    ///
    template<U32 kMaxLength1, U32 kMaxLength2>
    inline ShortString &replace_all (const ShortString<kMaxLength1> &pattern,
                                     const ShortString<kMaxLength2> &replacement)
    {
        const U32 pat_len = pattern.len ();
        const U32 replace_len = replacement.len ();

        if (pat_len == 0 || pat_len > m_len)
        {
            return *this;
        }

        // Temporary buffer to contain the final string.
        ShortString new_str;
        for (U32 i = 0; i <= m_len - pat_len; i++)
        {
            // Whether the patern matches the substring starting at character i.
            bool matches = true;
            for (U32 j = 0; j < pat_len && matches; j++)
            {
                matches = m_str[i + j] == pattern.str ()[j];
            }

            if (matches)
            {
                i += pat_len - 1;
                const U32 add_len =
                    (m_len + replace_len > kMaxLength) ? (kMaxLength - m_len) : replace_len;

                memcpy (&m_str[i], replacement.str (), add_len);
                new_str.m_len += add_len;
            }
            else
            {
                // Keep the character.
                new_str.m_str[new_str.m_len++] = m_str[i];
            }
        }

        memcpy (m_str, new_str.m_str, new_str.m_len);
        m_len = new_str.m_len;
        m_str[m_len] = '\0';
        return *this;
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
    inline ShortString &operator*= (U32 rhs) noexcept
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
    inline ShortString &operator+= (const char *__restrict__ rhs) noexcept
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
inline ShortString<kMaxLength1> operator* (ShortString<kMaxLength1> &&lhs, U32 rhs) noexcept
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
inline ShortString<kMaxLength1> operator* (const ShortString<kMaxLength1> &lhs, U32 rhs) noexcept
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
inline ShortString<kMaxLength2> operator+ (const char *lhs,
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
                                           const char *__restrict__ rhs) noexcept
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
                                           const char *__restrict__ rhs) noexcept
{
    ShortString<kMaxLength1> ss (lhs);
    ss += rhs;
    return ss;
}

/// @brief              Class Template Argument Deduction to guide the compiler to
///                     reserve only necessary space for compile time c strings.
template<U32 N>
ShortString (const char (&str)[N]) -> ShortString<N - 1>;