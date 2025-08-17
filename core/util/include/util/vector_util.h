#pragma once

namespace vector_util
{
template<typename T>
inline void append (std::vector<T> &vec, const std::vector<T> &other)
{
    vec.insert (vec.end (), other.begin (), other.end ());
}
}; // namespace vector_util