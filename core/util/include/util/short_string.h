#pragma once

#include "util/common.h"
#include "util/types.h"

#include <cstring>
#include <string>

template<U32 kMaxLength = 63>
class ShortString
{
  public:
    inline ShortString () noexcept :
        m_len (0)
    {
        m_str[0] = '\0';
    }

    inline ShortString (const char *__restrict__ str) noexcept
    {
        const size_t slen = strlen (str);
        m_len = (slen > kMaxLength) ? kMaxLength : slen;
        memcpy (m_str, str, m_len);
        m_str[m_len] = '\0';
    }

    inline ShortString (const ShortString &other) noexcept :
        m_len (other.len ())
    {
        printf ("COPY CTOR\n");
        memcpy (m_str, other.str (), m_len);
        m_str[m_len] = '\0';
    }

    template<U32 kMaxLength2>
    inline ShortString (const ShortString<kMaxLength2> &other) noexcept
    {
        printf ("COPY CTOR\n");
        m_len = (other.len () > kMaxLength) ? kMaxLength : other.len ();
        memcpy (m_str, other.str (), m_len);
        m_str[m_len] = '\0';
    }

    inline U32 len () const noexcept
    {
        return m_len;
    }

    inline const char *str () const noexcept
    {
        return m_str;
    }

    inline ShortString &operator*= (U32 rhs) noexcept
    {
        const U32 original_len = m_len;
        const U32 target_len = m_len * rhs;
        m_len = (target_len > kMaxLength) ? kMaxLength : target_len;

        const U32 max_copies = m_len / original_len;

        for (U32 i = 1; i < max_copies; i++)
        {
            memcpy (m_str + i * original_len, m_str, original_len);
        }

        const U32 start_partial_copy = max_copies * original_len;
        for (U32 i = start_partial_copy; i < m_len; i++)
        {
            m_str[i] = m_str[i - start_partial_copy];
        }

        m_str[m_len] = '\0';
        return *this;
    }

    inline friend ShortString operator* (ShortString &&lhs, U32 rhs) noexcept
    {
        lhs *= rhs;
        return lhs;
    }

    inline friend ShortString operator* (const ShortString &lhs, U32 rhs) noexcept
    {
        ShortString ss = lhs;
        ss *= rhs;
        return ss;
    }

    template<U32 kMaxLength2>
    inline ShortString &operator+= (const ShortString<kMaxLength2> &rhs) noexcept
    {
        const U32 add_len = (m_len + rhs.len ()) > kMaxLength ? (kMaxLength - m_len) : rhs.len ();
        memcpy (m_str + m_len, rhs.str (), add_len);
        m_len += add_len;
        m_str[m_len] = '\0';
        return *this;
    }

    inline ShortString &operator+= (const char *__restrict__ rhs) noexcept
    {
        const size_t slen = strlen (rhs);
        const U32 add_len = (m_len + slen) > kMaxLength ? (kMaxLength - m_len) : slen;
        memcpy (m_str + m_len, rhs, add_len);
        m_len += add_len;
        m_str[m_len] = '\0';
        return *this;
    }

    template<U32 kMaxLength2>
    inline friend ShortString operator+ (ShortString &&lhs,
                                         const ShortString<kMaxLength2> &rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    template<U32 kMaxLength2>
    inline friend ShortString operator+ (const ShortString &lhs,
                                         const ShortString<kMaxLength2> &rhs) noexcept
    {
        ShortString ss = lhs;
        ss += rhs;
        return ss;
    }

    template<U32 kMaxLength2>
    inline friend ShortString operator+ (const char *__restrict__ lhs,
                                         const ShortString<kMaxLength2> &rhs) noexcept
    {
        ShortString ss (lhs);
        ss += rhs;
        return ss;
    }

    template<U32 kMaxLength2>
    inline friend ShortString operator+ (ShortString<kMaxLength2> &&lhs,
                                         const char *__restrict__ rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    template<U32 kMaxLength2>
    inline friend ShortString operator+ (const ShortString<kMaxLength2> &lhs,
                                         const char *__restrict__ rhs) noexcept
    {
        ShortString ss (lhs);
        ss += rhs;
        return ss;
    }

  private:
    char m_str[kMaxLength + 1];
    U32 m_len;
};