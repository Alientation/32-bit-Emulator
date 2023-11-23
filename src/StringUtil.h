#include <vector>
#include <string>
#include <regex>

#ifndef STRINGUTIL_H
#define STRINGUTIL_H

/**
 * Trims whitespace from the left side of a string
 * 
 * @param str the string to trim
 * 
 * @return the trimmed string
 */
static std::string leftTrim(std::string str) {
	str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char c) {
		return !std::isspace(c);
	}));
	return str;
}

/**
 * Trims whitespace from the right side of a string
 * 
 * @param str the string to trim
 * 
 * @return the trimmed string
 */
static std::string rightTrim(std::string str) {
	str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char c) {
		return !std::isspace(c);
	}).base(), str.end());
	return str;
}

/**
 * Trims whitespace from the left and right side of a string
 * 
 * @param str the string to trim
 * 
 * @return the trimmed string
 */
static std::string trimString(std::string str) {
	return leftTrim(rightTrim(str));
}


/**
 * Splits a string into a vector of strings separated by the given regex delimiter.
 * 
 * @param str the string to split
 * @param delimiter the regex delimiter to split the string by
 * @param trim whether or not to trim each split string
 * 
 * @return a vector of strings separated by the given regex delimiter
 */
static std::vector<std::string> split(std::string str, std::string delimRegex, bool trim = false) {
	std::vector<std::string> result;
	std::regex rgx(delimRegex);
	std::sregex_token_iterator iter(str.begin(), str.end(), rgx, -1);
	std::sregex_token_iterator end;

	while (iter != end) {
		std::string token = *iter;
		if (trim) {
			token = trimString(token);
		}

		result.push_back(token);
		++iter;
	}

	return result;
}


#endif