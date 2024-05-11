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
					Logger::LogType logType;
					std::string group;
					std::string msg;
					std::time_t timestamp;

					LogMessage(Logger::LogType logType, std::string msg, std::string group) : logType(logType), msg(msg), 
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

			static std::string LOGTYPE_TO_STRING(Logger::LogType log_type);
			static std::string LOGTYPE_TO_PRINT(Logger::LogType log_type);

			Logger(std::string logger_id, CONFIG config);
			~Logger();

			void log(Logger::LogType log_type, const std::string& msg, const std::string& group = "");
			void log(Logger::LogType log_type, const std::stringstream& msg, const std::string& group = "");
			void EXPECT_TRUE(bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");
			void EXPECT_FALSE(bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");
			void flush();
			void dump(FileWriter &writer, const std::set<Logger::LogType> &queried_log_types = {}, const std::set<std::string> &queried_log_groups = {});

			static void dump_all(FileWriter &writer, const std::set<std::string> &queried_log_ids = {},
					const std::set<Logger::LogType> &queried_log_types = {}, const std::set<std::string> &queried_log_groups = {});

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

	void log(Logger::LogType log_type, const std::string& msg, const std::string& group = "");
	void log(Logger::LogType log_type, const std::stringstream& msg, const std::string& group = "");
	void EXPECT_TRUE(bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");	
	void EXPECT_FALSE(bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group = "");
};

#endif