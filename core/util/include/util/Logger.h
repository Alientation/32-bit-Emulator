#pragma once
#ifndef LOGGER_H
#define LOGGER_H

#include <util/ConsoleColor.h>
#include <util/File.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

// append some extra logging information when calling the function
#define log(args...) log_f(__FILE__, __func__, __LINE__, ##args)
#define EXPECT_TRUE(args...) EXPECT_TRUE_f(__FILE__, __func__, __LINE__, ##args)
#define EXPECT_FALSE(args...) EXPECT_FALSE_f(__FILE__, __func__, __LINE__, ##args)
/**
 * IDEA
 * 
 * global list of loggers so that it remains persistent across the program.
 * access the logger by getting that specific instance by name (creates if does not exist)
 * create logger
 */
namespace lgr {
class Logger {
	public:
		enum class LogType {
			LOG, ERROR, WARN, INFO, DEBUG, TEST
		};

		class LogMessage {
			public:
				std::string file;
				std::string func;
				int line_num;
				Logger::LogType logType;
				std::string group;
				std::string msg;
				std::time_t timestamp;

				LogMessage(std::string file, std::string func, int line_num, Logger::LogType logType, std::string msg, std::string group) 
						: file(file), func(func), line_num(line_num), logType(logType), msg(msg), 
						timestamp(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())),
						group(group) {}

				std::string to_string();
				std::string to_print_string();
		};

		/**
		 * Hold configuration information about a specific logger.
		 */
		class CONFIG {
			friend class Logger;
			public:
				CONFIG();
				~CONFIG();

				CONFIG& output_file(std::string output_file);
				CONFIG& print_logs(bool print_logs = true, std::function<std::string(Logger::LogMessage)> print_log_func = {});
				CONFIG& throw_on_error(bool throw_on_error = true);
				CONFIG& flush_every_log(bool flush_every_log = true);
			private:
				std::string _output_file;
				bool _print_logs; 
				std::function<std::string(Logger::LogMessage)> _print_log_func;
				bool _throw_on_error;

				bool _flush_every_log;
		};

		/**
		 * Query information for log dumps
		 */
		class LOG_DUMP {
			friend class Logger;
			public:
				LOG_DUMP();
				~LOG_DUMP();
				bool evaluate(const Logger::LogMessage& log);

				LOG_DUMP& files(std::set<std::string> files);
				LOG_DUMP& funcs(std::set<std::string> funcs);
				LOG_DUMP& lines(std::set<int> lines);
				LOG_DUMP& groups(std::set<std::string> groups);
				LOG_DUMP& log_types(std::set<Logger::LogType> log_types);
				LOG_DUMP& logger_ids(std::set<std::string> logger_ids);
			private:
				std::set<std::string> _files;
				std::set<std::string> _funcs;
				std::set<int> _lines;
				std::set<std::string> _groups;
				std::set<Logger::LogType> _log_types;
				std::set<std::string> _logger_ids;
		};

		static std::string LOGTYPE_TO_STRING(Logger::LogType log_type);
		static std::string LOGTYPE_TO_PRINT(Logger::LogType log_type);

		Logger(std::string logger_id, CONFIG config);
		~Logger();

		// dummy functions to help see the log api functions that will be routed to the macro instead
		#pragma push_macro("log")
		#pragma push_macro("EXPECT_TRUE")
		#pragma push_macro("EXPECT_FALSE")
		#undef log
		#undef EXPECT_TRUE
		#undef EXPECT_FALSE
		void log(Logger::LogType log_type, const std::string& msg, const std::string& group = "");
		void log(Logger::LogType log_type, const std::stringstream& msg, const std::string& group = "");
		void EXPECT_TRUE(bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");
		void EXPECT_FALSE(bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");
		#pragma pop_macro("log")
		#pragma pop_macro("EXPECT_TRUE")
		#pragma pop_macro("EXPECT_FALSE")

		void log_f(std::string file, std::string func, int line_num, Logger::LogType log_type, const std::string& msg, const std::string& group = "");
		void log_f(std::string file, std::string func, int line_num, Logger::LogType log_type, const std::stringstream& msg, const std::string& group = "");
		void EXPECT_TRUE_f(std::string file, std::string func, int line_num, bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");
		void EXPECT_FALSE_f(std::string file, std::string func, int line_num, bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");
		void flush();
		void dump(FileWriter &writer, LOG_DUMP log_dump_query);

		static void dump_all(FileWriter &writer, LOG_DUMP log_dump_query);

	private:
		std::string _logger_id;
		FileWriter* _file_writer;
		File* _log_file;
		CONFIG _config;

		std::vector<LogMessage> _logs;
};

Logger* get_logger(const std::string &logger_id);
Logger* create_logger(const std::string &logger_id, Logger::CONFIG config);
Logger* remove_logger(const std::string &logger_id);

// todo see if we can replace these with macros that will append get_logger("") instead of having this bloat of a mess
void log_f(std::string file, std::string func, int line_num, Logger::LogType log_type, const std::string& msg, const std::string& group = "");
void log_f(std::string file, std::string func, int line_num, Logger::LogType log_type, const std::stringstream& msg, const std::string& group = "");
void EXPECT_TRUE_f(std::string file, std::string func, int line_num, bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");	
void EXPECT_FALSE_f(std::string file, std::string func, int line_num, bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");

// dummy functions to help see the log api functions that will be routed to the macro instead
#pragma push_macro("log")
#pragma push_macro("EXPECT_TRUE")
#pragma push_macro("EXPECT_FALSE")
#undef log
#undef EXPECT_TRUE
#undef EXPECT_FALSE
void log(Logger::LogType log_type, const std::string& msg, const std::string& group = "");
void log(Logger::LogType log_type, const std::stringstream& msg, const std::string& group = "");
void EXPECT_TRUE(bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");
void EXPECT_FALSE(bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");
#pragma pop_macro("log")
#pragma pop_macro("EXPECT_TRUE")
#pragma pop_macro("EXPECT_FALSE")
};

#endif