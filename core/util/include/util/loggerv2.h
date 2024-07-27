#pragma once
#ifndef LOGGERV2_H
#define LOGGERV2_H

#include "util/console_color.h"

#include <cstdarg>
#include <iostream>
#include <stdio.h>
#include <sstream>

/*

what i want for a logger

- 	toggleable logs (levels + each unit that uses a log has the ability to easily disable logs in that file)
- 	print + noprint, dump to file with filters, query logs
- 	color print
- 	automatically detect the line and file a log occurs in, this means a macro. problem with macros is
	they do not show types well
-	ability to use printf to log (so pass variadic args to printf), also can use std::stringstream, though
	problem with this is how do we capture the result of printf to save to file when needed??
-	fast, but not a high concern as for performance we can disable all low level logs

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
namespace logger
{
	inline void log_debug(const char* format, const char* file, int line, ...) {
		if (PRINT_ENABLED && LOG_ENABLED && LOG_LEVEL >= LOG_DEBUG) {
			std::cout << "[" << ccolor::BOLD << ccolor::MAGENTA << "DEBUG" << ccolor::RESET << "] <"
					<< ccolor::GRAY << file << ccolor::RESET << ":" << ccolor::YELLOW << line
					<< ccolor::RESET << ">: ";

			va_list args;
			va_start(args, line);
			vprintf(format, args);
			va_end(args);
			std::cout << "\n";
		}

		if (LOG_ENABLED) {
			/*TODO: track log */
		}
	}

	inline void log_debug(const std::stringstream& log, const char* file, int line) {
		if (PRINT_ENABLED && LOG_ENABLED && LOG_LEVEL >= LOG_DEBUG) {
			std::cout << "[" << ccolor::BOLD << ccolor::MAGENTA << "DEBUG" << ccolor::RESET << "] <"
					<< ccolor::GRAY << file << ccolor::RESET << ":" << ccolor::YELLOW << line
					<< ccolor::RESET << ">: " << log.str() << "\n";
		}

		if (LOG_ENABLED) {
			/* TODO: track log */
		}
	}

	// inline void log_info(const char* format, const char* file, int line, ...) {

	// }

	// inline void log_info(const std::stringstream& log, const char* file, int line) {

	// }

	// inline void log_warn(const char* format, const char* file, int line, ...) {

	// }

	// inline void log_warn(const std::stringstream& log, const char* file, int line) {

	// }

	// inline void log_error(const char* format, const char* file, int line, ...) {

	// }

	// inline void log_error(const std::stringstream& log, const char* file, int line) {

	// }


	// Wrapper functions with variadic templates
	template<typename... Args>
	inline void log_debug(const char* format, Args&&... args) {
		log_debug(format, __FILE__, __LINE__, std::forward<Args>(args)...);
	}

	// template<typename... Args>
	// inline void log_info(const char* format, Args&&... args) {
	// 	log_info(format, __FILE__, __LINE__, std::forward<Args>(args)...);
	// }

	// template<typename... Args>
	// inline void log_warn(const char* format, Args&&... args) {
	// 	log_warn(format, __FILE__, __LINE__, std::forward<Args>(args)...);
	// }

	// template<typename... Args>
	// inline void log_error(const char* format, Args&&... args) {
	// 	log_error(format, __FILE__, __LINE__, std::forward<Args>(args)...);
	// }

	inline void log_debug(const std::stringstream& log) {
		log_debug(log, __FILE__, __LINE__);
	}

	// inline void log_info(const std::stringstream& log) {
	// 	log_info(log, __FILE__, __LINE__);
	// }

	// inline void log_warn(const std::stringstream& log) {
	// 	log_warn(log, __FILE__, __LINE__);
	// }

	// inline void log_error(const std::stringstream& log) {
	// 	log_error(log, __FILE__, __LINE__);
	// }
};

#endif /* LOGGERV2_H */