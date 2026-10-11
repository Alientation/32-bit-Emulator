#pragma once

namespace vector_util
{
/// Appends the elements of one vector to another.
///
/// @param[in,out] vec The vector that gets the elements.
/// @param[in] other The vector to copy the elements from.
template<typename T>
inline void append(std::vector<T> &vec, const std::vector<T> &other)
{
    vec.insert(vec.end(), other.begin(), other.end());
}
}; // namespace vector_util