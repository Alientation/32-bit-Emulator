#include <vector>
#include <string>

#ifndef STRINGUTIL_H
#define STRINGUTIL_H

/**
 * Splits a string into a vector of strings separated by the given delimiter.
 * 
 * @param str the string to split
 * @param delimiter the delimiter to split the string by
 * @param trim whether or not to trim each split string
 * 
 * @return a vector of strings separated by the given delimiter
 */
static std::vector<std::string> split(std::string str, std::string delimiter, bool trim = false) {
	std::vector<std::string> result;
	size_t pos = 0;
	std::string token;
	while ((pos = str.find(delimiter)) != std::string::npos) {
		token = str.substr(0, pos);

		if (trim) {
			token.erase(0, token.find_first_not_of(" \t\n\r\f\v"));
			token.erase(token.find_last_not_of(" \t\n\r\f\v") + 1);
		}

		result.push_back(token);
		str.erase(0, pos + delimiter.length());
	}

	if (trim) {
		str.erase(0, str.find_first_not_of(" \t\n\r\f\v"));
		str.erase(str.find_last_not_of(" \t\n\r\f\v") + 1);
	}

	result.push_back(str);

	return result;
}


#endif