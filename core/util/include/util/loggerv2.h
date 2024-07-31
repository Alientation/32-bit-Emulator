#pragma once
#ifndef LOGGERV2_H
#define LOGGERV2_H

#include "util/console_color.h"
#include "util/string_util.h"

#include <iostream>

/*
	what i want for a logger

	// - 	toggleable logs (levels + each unit that uses a log has the ability to easily disable logs in that file)
	// - 	print + noprint
	-	dump to file with filters, query logs
	// - 	color print
	// - 	automatically detect the line and file a log occurs in, this means a macro. problem with macros is
		// they do not show types well
	// -	ability to use printf to log (so pass variadic args to printf), also can use std::stringstream, though
		// problem with this is how do we capture the result of printf to save to file when needed??
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

#ifndef EXCEPT_ON_ERROR
#define EXCEPT_ON_ERROR true
#endif /* EXCEPT_ON_ERROR */

#ifndef PROJECT_ROOT_DIR
	#define PROJECT_ROOT_DIR std::string(__FILE__).substr(0, std::string(__FILE__).rfind("core"))
#endif /* PROJECT_ROOT_DIR */

namespace logger
{
	/**
	 * @brief			Tracks a log into memory.
	 *
	 * @param type 		The type of log stringified.
	 * @param format 	Format of the string. (passed into printf)
	 * @param file		The file the log occured in.
	 * @param line		Line of code the log occured in.
	 * @param func		Function name the log occured in.
	 * @param ...		Any extra arguments used with the supplied format to construct the log.
	 */
	void track(const std::string& type, const char* format, const char* file, int line,
			   const char* func, ...);

	/* TODO: query logs, dump to file, etc */

	/**
	 * @brief 			Prints the formated header.
	 *
	 * @param type		Type of log.
	 * @param file		File the log occured in.
	 * @param line		Line of code the log occured in.
	 * @param func		Function name the log occured in.
	 */
	inline void print_header(const std::string& type, const char* file, int line,
							 const char* func)
	{
		std::cout << "[" << ccolor::BOLD << type << ccolor::RESET << "] ["
				<< ccolor::BLUE << string_util::replaceFirst(file, PROJECT_ROOT_DIR, "")
				<< ccolor::RESET << ":" << ccolor::MAGENTA << line << ccolor::RESET << "] ["
				<< ccolor::YELLOW << func << ccolor::RESET << "]: ";
	}

	/**
	 * @brief 			Debug log.
	 *
	 * @tparam Args		Variadic arguments to be passed into printf.
	 * @param format	Format of the printf.
	 * @param file		File the log occured in.
	 * @param line		Line of code the log occured in.
	 * @param func		Function name the log occured in.
	 * @param args		Arguments passed into printf.
	 */
	template <typename... Args>
	inline void log_debug(const char* format, const char* file, int line, const char* func, Args&&... args)
	{
		if (LOG_ENABLED) {
			if (PRINT_ENABLED && LOG_LEVEL >= LOG_DEBUG) {
				print_header(ccolor::MAGENTA + "DBG", file, line, func);
				printf(format, args...);
				std::cout << "\n";
			}

			track("DBG", format, file, line, func, args...);
		}
	}

	/**
	 * @brief 			Info log.
	 *
	 * @tparam Args		Variadic arguments to be passed into printf.
	 * @param format	Format of the printf.
	 * @param file		File the log occured in.
	 * @param line		Line of code the log occured in.
	 * @param func		Function name the log occured in.
	 * @param args		Arguments passed into printf.
	 */
	template <typename... Args>
	inline void log_info(const char* format, const char* file, int line, const char* func, Args&&... args)
	{
		if (LOG_ENABLED) {
			if (PRINT_ENABLED && LOG_LEVEL >= LOG_DEBUG) {
				print_header(ccolor::BLUE + "INF", file, line, func);
				printf(format, args...);
				std::cout << "\n";
			}

			track("INF", format, file, line, func, args...);
		}
	}

	/**
	 * @brief 			Warn log.
	 *
	 * @tparam Args		Variadic arguments to be passed into printf.
	 * @param format	Format of the printf.
	 * @param file		File the log occured in.
	 * @param line		Line of code the log occured in.
	 * @param func		Function name the log occured in.
	 * @param args		Arguments passed into printf.
	 */
	template <typename... Args>
	inline void log_warn(const char* format, const char* file, int line, const char* func, Args&&... args)
	{
		if (LOG_ENABLED) {
			if (PRINT_ENABLED && LOG_LEVEL >= LOG_DEBUG) {
				print_header(ccolor::YELLOW + "WRN", file, line, func);
				printf(format, args...);
				std::cout << "\n";
			}

			track("WRN", format, file, line, func, args...);
		}
	}

	/**
	 * @brief 			Error log.
	 * @note			This will not terminate the program.
	 *
	 * @tparam Args		Variadic arguments to be passed into printf.
	 * @param format	Format of the printf.
	 * @param file		File the log occured in.
	 * @param line		Line of code the log occured in.
	 * @param func		Function name the log occured in.
	 * @param args		Arguments passed into printf.
	 */
	template <typename... Args>
	inline void log_error(const char* format, const char* file, int line, const char* func, Args&&... args)
	{
		if (LOG_ENABLED) {
			if (PRINT_ENABLED && LOG_LEVEL >= LOG_DEBUG) {
				print_header(ccolor::RED + "ERR", file, line, func);
				printf(format, args...);
				std::cout << "\n";
			}

			track("ERR", format, file, line, func, args...);
		}

		if (EXCEPT_ON_ERROR) {
			exit(EXIT_FAILURE);
		}
	}

	template <typename... Args>
	inline void expect_true(bool condition, const char* format, const char* file, int line,
							const char* func, Args&&... args)
	{
		if (!condition) {
			log_error(format, file, line, func, args...);
		}
	}

	template <typename... Args>
	inline void expect_false(bool condition, const char* format, const char* file, int line,
							 const char* func, Args&&... args)
	{
		if (condition) {
			log_error(format, file, line, func, args...);
		}
	}

	template <typename T1, typename T2, typename... Args>
	inline void expect_equal(T1 t1, T2 t2, const char* format, const char* file, int line,
							 const char* func, Args&&... args)
	{
		if (t1 != t2) {
			log_error(format, file, line, func, args...);
		}
	}

	template <typename T1, typename T2, typename... Args>
	inline void expect_not_equal(T1 t1, T2 t2, const char* format, const char* file, int line,
								 const char* func, Args&&... args)
	{
		if (t1 == t2) {
			log_error(format, file, line, func, args...);
		}
	}

	/**
	 * @def				DEBUG(format, ...)
	 * @brief 			Wrap log to gather information like file, LOC, and function name.
	 * @param format	The format string.
	 * @param ...		Additional arguments for the format string.
	 */
	#define DEBUG(format, ...) logger::log_debug(format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

	/**
	 * @def				DEBUG_SS(msg)
	 * @brief 			Wrap log to gather information like file, LOC, and function name.
	 * @param msg		stringstream log.
	 */
	#define DEBUG_SS(msg) logger::log_debug((msg).str().c_str(), __FILE__, __LINE__, __func__)

	/**
	 * @def				INFO(format, ...)
	 * @brief 			Wrap log to gather information like file, LOC, and function name.
	 * @param format	The format string.
	 * @param ...		Additional arguments for the format string.
	 */
	#define INFO(format, ...) logger::log_info(format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

	/**
	 * @def				INFO_SS(msg)
	 * @brief 			Wrap log to gather information like file, LOC, and function name.
	 * @param msg		stringstream log.
	 */
	#define INFO_SS(msg) logger::log_info((msg).str().c_str(), __FILE__, __LINE__, __func__)

	/**
	 * @def				WARN(format, ...)
	 * @brief 			Wrap log to gather information like file, LOC, and function name.
	 * @param format	The format string.
	 * @param ...		Additional arguments for the format string.
	 */
	#define WARN(format, ...) logger::log_warn(format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

	/**
	 * @def				WARN_SS(msg)
	 * @brief 			Wrap log to gather information like file, LOC, and function name.
	 * @param msg		stringstream log.
	 */
	#define WARN_SS(msg) logger::log_warn((msg).str().c_str(), __FILE__, __LINE__, __func__)

	/**
	 * @def				ERROR(format, ...)
	 * @brief 			Wrap log to gather information like file, LOC, and function name.
	 * @param format	The format string.
	 * @param ...		Additional arguments for the format string.
	 */
	#define ERROR(format, ...) logger::log_error(format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

	/**
	 * @def				ERROR_SS(msg)
	 * @brief 			Wrap log to gather information like file, LOC, and function name.
	 * @param msg		stringstream log.
	 */
	#define ERROR_SS(msg) logger::log_error((msg).str().c_str(), __FILE__, __LINE__, __func__)

	#define EXPECT_TRUE(condition, format, ...) logger::expect_true(condition, format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
	#define EXPECT_TRUE_SS(condition, msg) logger::expect_true(condition, (msg).str().c_str(), __FILE__, __LINE__, __func__)
	#define EXPECT_FALSE(condition, format, ...) logger::expect_false(condition, format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
	#define EXPECT_FALSE_SS(condition, msg) logger::expect_false(condition, (msg).str().c_str(), __FILE__, __LINE__, __func__)
	#define EXPECT_EQUAL(t1, t2, format, ...) logger::expect_equal(t1, t2, format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
	#define EXPECT_EQUAL_SS(t1, t2, msg) logger::expect_equal(t1, t2, (msg).str().c_str(), __FILE__, __LINE__, __func__)
	#define EXPECT_NOT_EQUAL(t1, t2, format, ...) logger::expect_equal(t1, t2, format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
	#define EXPECT_NOT_EQUAL_SS(t1, t2, msg) logger::expect_equal(t1, t2, (msg).str().c_str(), __FILE__, __LINE__, __func__)
};

#endif /* LOGGERV2_H */