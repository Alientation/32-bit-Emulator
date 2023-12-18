#ifndef VECTORUTIL_H
#define VECTORUTIL_H

namespace vector_util {
	template<typename T>
	static void append(std::vector<T>& vec, std::vector<T>& other) {
		vec.insert(vec.end(), other.begin(), other.end());
	}
}

#endif // VECTORUTIL_H