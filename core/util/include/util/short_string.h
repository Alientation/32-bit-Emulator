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
    /// @brief              Repeat operator.
    ///
    /// @param lhs          R-value reference to temporary object to repeat.
    /// @param rhs          Number of times to repeat.
    ///
    /// @return             New ShortString object. Same buffer size as the lhs string.
    ///
    inline friend ShortString operator* (ShortString &&lhs, U32 rhs) noexcept
    {
        lhs *= rhs;
        return lhs;
    }

    ///
    /// @brief              Repeat operator.
    ///
    /// @param lhs          Reference to ShortString to repeat, not modified.
    /// @param rhs          Number of times to repeat.
    ///
    /// @return             New ShortString object. Same buffer size as the lhs string.
    ///
    inline friend ShortString operator* (const ShortString &lhs, U32 rhs) noexcept
    {
        ShortString ss = lhs;
        ss *= rhs;
        return ss;
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

    ///
    /// @brief              Concatenation operator.
    ///                     Adds two ShortStrings together.
    ///
    /// @tparam kMaxLength2 Buffer size of second string.
    ///
    /// @param lhs          R-value reference to the temporary string object on the left hand
    ///                     side of the operator.
    /// @param rhs          Reference to string object on the right hand side of the operator.
    ///
    /// @return             New ShortString. Same buffer size as lhs.
    ///
    template<U32 kMaxLength2>
    inline friend ShortString operator+ (ShortString &&lhs,
                                         const ShortString<kMaxLength2> &rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    ///
    /// @brief              Concatenation operator.
    ///                     Adds two ShortStrings together.
    ///
    /// @tparam kMaxLength2 Buffer size of second string.
    ///
    /// @param lhs          Reference to string object on the left hand side of the operator.
    /// @param rhs          Reference to string object on the right hand side of the operator.
    ///
    /// @return             New ShortString. Same buffer size as lhs.
    ///
    template<U32 kMaxLength2>
    inline friend ShortString operator+ (const ShortString &lhs,
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
    /// @tparam kMaxLength2 Buffer size of the first string.
    ///
    /// @param lhs          R-value reference to the string object on the left hand side of the operator.
    /// @param rhs          C style string on the right hand side of the operator.
    ///
    /// @return             New ShortString. Same buffer size as rhs string.
    ///
    template<U32 kMaxLength1>
    inline friend ShortString operator+ (ShortString<kMaxLength1> &&lhs,
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
    inline friend ShortString operator+ (const ShortString<kMaxLength1> &lhs,
                                         const char *__restrict__ rhs) noexcept
    {
        ShortString<kMaxLength1> ss (lhs);
        ss += rhs;
        return ss;
    }

  private:
    /// @brief              Internal buffer for string.
    ///                     Sized to accomodate ending null terminator.
    char m_str[kMaxLength + 1];

    /// @brief              Length of string, not including null terminator.
    U32 m_len;
};