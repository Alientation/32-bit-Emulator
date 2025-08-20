#pragma once

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

    inline ShortString (const std::string &str) noexcept
    {
        const size_t slen = str.length ();
        m_len = (slen > kMaxLength) ? kMaxLength : slen;
        memcpy (m_str, str.c_str (), m_len);
        m_str[m_len] = '\0';
    }

    inline U32 len () const
    {
        return m_len;
    }

    inline const char *str () const
    {
        return m_str;
    }

  private:
    char m_str[kMaxLength + 1];
    U32 m_len;
};