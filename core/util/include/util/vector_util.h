#pragma once
#ifndef VECTOR_UTIL_H
#define VECTOR_UTIL_H

namespace vector_util
{
    template<typename T>
    inline void append(std::vector<T>& vec, const std::vector<T>& other)
    {
        vec.insert(vec.end(), other.begin(), other.end());
    }
};

#endif // VECTOR_UTIL_H