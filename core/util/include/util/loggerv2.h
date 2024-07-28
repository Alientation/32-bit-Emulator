#pragma once
#ifndef LOGGERV2_H
#define LOGGERV2_H

#include "util/console_color.h"
#include "util/string_util.h"

#include <cstdarg>
#include <iostream>
#include <stdio.h>
#include <string>
#include <sstream>

/*

what i want for a logger

// - 	toggleable logs (levels + each unit that uses a log has the ability to easily disable logs in that file)
- 	print + noprint, dump to file with filters, query logs
// - 	color print
// - 	automatically detect the line and file a log occurs in, this means a macro. problem with macros is
	// they do not show types well
-	ability to use printf to log (so pass variadic args to printf), also can use std::stringstream, though
	problem with this is how do we capture the result of printf to save to file when needed??
// -	fast, but not a high concern as for performance we can disable all low level logs

*/

#define LOG_DEBUG 4
#define LOG_INFO 3
#define LOG_WARN 2
#define LOG_ERROR 1
#define LOG_NONE 0

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#endif /* LOG_LEVEL */

#ifndef LOG_ENABLED
#define LOG_ENABLED true
#endif /* LOG_DISABLED */

#ifndef PRINT_ENABLED
#define PRINT_ENABLED true
#endif /* PRINT_DISABLED */

#ifdef _WIN32
    #define FILE_SEPARATOR '\\'
#else
    #define FILE_SEPARATOR '/'
#endif /* _WIN32 */

#ifndef PROJECT_ROOT_DIR
	#define PROJECT_ROOT_DIR std::string(__FILE__).substr(0, std::string(__FILE__).rfind("core"))
#endif /* PROJECT_ROOT_DIR */

namespace logger
{
	inline void print_header(const std::string& header, const char* file, int line, const char* func) {
		std::cout << "[" << ccolor::BOLD << header << ccolor::RESET << "] ["
				<< ccolor::BLUE << string_util::replaceFirst(file, PROJECT_ROOT_DIR, "")
				<< ccolor::RESET << ":" << ccolor::MAGENTA << line << ccolor::RESET << "] ["
				<< ccolor::YELLOW << func << ccolor::RESET << "]: ";
	}

	inline void log_debug(const char* format, const char* file, int line, const char* func, ...) {
		if (PRINT_ENABLED && LOG_ENABLED && LOG_LEVEL >= LOG_DEBUG) {
			print_header(ccolor::MAGENTA + "DBG", file, line, func);

			va_list args;
			va_start(args, func);
			vprintf(format, args);
			va_end(args);
			std::cout << "\n";
		}

		if (LOG_ENABLED) {
			/*TODO: track log */
		}
	}

	inline void log_info(const char* format, const char* file, int line, const char* func, ...) {
		if (PRINT_ENABLED && LOG_ENABLED && LOG_LEVEL >= LOG_DEBUG) {
			print_header(ccolor::BLUE + "INF", file, line, func);

			va_list args;
			va_start(args, func);
			vprintf(format, args);
			va_end(args);
			std::cout << "\n";
		}

		if (LOG_ENABLED) {
			/*TODO: track log */
		}
	}

	inline void log_warn(const char* format, const char* file, int line, const char* func, ...) {
		if (PRINT_ENABLED && LOG_ENABLED && LOG_LEVEL >= LOG_DEBUG) {
			print_header(ccolor::YELLOW + "WRN", file, line, func);

			va_list args;
			va_start(args, func);
			vprintf(format, args);
			va_end(args);
			std::cout << "\n";
		}

		if (LOG_ENABLED) {
			/*TODO: track log */
		}
	}

	inline void log_error(const char* format, const char* file, int line, const char* func, ...) {
		if (PRINT_ENABLED && LOG_ENABLED && LOG_LEVEL >= LOG_DEBUG) {
			print_header(ccolor::RED + "ERR", file, line, func);

			va_list args;
			va_start(args, func);
			vprintf(format, args);
			va_end(args);
			std::cout << "\n";
		}

		if (LOG_ENABLED) {
			/*TODO: track log */
		}
	}


	#define log_debug(format, ...) log_debug(format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
	#define log_debug_ss(msg) log_debug((msg).str().c_str(), __FILE__, __LINE__, __func__)

	#define log_info(format, ...) log_info(format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
	#define log_info_ss(msg) log_info((msg).str().c_str(), __FILE__, __LINE__, __func__)

	#define log_warn(format, ...) log_warn(format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
	#define log_warn_ss(msg) log_warn((msg).str().c_str(), __FILE__, __LINE__, __func__)

	#define log_error(format, ...) log_error(format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
	#define log_error_ss(msg) log_error((msg).str().c_str(), __FILE__, __LINE__, __func__)
};

#endif /* LOGGERV2_H */