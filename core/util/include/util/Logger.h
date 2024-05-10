#pragma once
#ifndef LOGGER_H
#define LOGGER_H

#include "util/ConsoleColor.h"
#include "util/File.h"

#include <chrono>
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
			static std::string LOGTYPE_TO_STRING(Logger::LogType log_type);
			static std::string LOGTYPE_TO_PRINT(Logger::LogType log_type);

			Logger(bool print_logs, bool throw_on_error, const std::string &log_file_path);
			~Logger();

			void log(Logger::LogType logType, std::string msg, std::string group = "");
			void EXPECT_TRUE(bool condition, Logger::LogType logType, std::string msg, std::string group = "");
			void EXPECT_FALSE(bool condition, Logger::LogType logType, std::string msg, std::string group = "");
			void dump(FileWriter &writer, const std::set<Logger::LogType> &queried_log_types = {}, const std::set<std::string> &queried_log_groups = {});

		private:
			FileWriter* file_writer;
			File* log_file;
			bool print_logs;
			bool throw_on_error;

			class LogMessage {
				public:
					const Logger::LogType logType;
					const std::string group;
					const std::string msg;
					const std::time_t timestamp;

					LogMessage(Logger::LogType logType, std::string msg, std::string group) : logType(logType), msg(msg), 
							timestamp(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())),
							group(group) {}
			};
			
			std::vector<LogMessage> logs;
	};

	Logger get_logger(const std::string &logger_id);
	Logger create_logger(const std::string &logger_id, bool print_logs, bool throw_on_error, const std::string &log_file_path = "");
};



enum class LogType {
	LOG, ERROR, WARN, INFO, DEBUG, TEST
};


/**
 * Logs a message to the console.
 * 
 * @param logType The log type
 * @param msg The message to log
 */
static void log(LogType logType, std::stringstream msg) {
	// construct the log header
	using namespace ccolor;
	std::string logTypeStr;
	switch (logType) {
	case LogType::LOG:
		logTypeStr = BOLD + WHITE + "LOG" + RESET;
		break;
	case LogType::ERROR:
		logTypeStr = BOLD + RED + "ERROR" + RESET;
		break;
	case LogType::WARN:
		logTypeStr = BOLD + YELLOW + "WARN" + RESET;
		break;
	case LogType::INFO:
		logTypeStr = BOLD + BLUE + "INFO" + RESET;
		break;
	case LogType::DEBUG:
		logTypeStr = BOLD + MAGENTA + "DEBUG" + RESET;
		break;
	case LogType::TEST:
		logTypeStr = BOLD + CYAN + "TEST" + RESET;
		break;
	}

	// output the log message
	switch (logType) {
		case LogType::LOG:
		case LogType::WARN:
		case LogType::INFO:
		case LogType::DEBUG:
		case LogType::TEST:
			std::cout << "[" << logTypeStr << "] " << msg.str() << std::endl;
			break;
		case LogType::ERROR:
			// std::stringstream msgStream;
			// msgStream << "[" << logTypeStr << "] " << msg.str();
			// throw std::runtime_error(msgStream.str());
            std::cerr << "[" << logTypeStr << "] " << msg.str() << std::endl;
            exit(EXIT_FAILURE);
	}
}

/**
 * Checks if the condition is true, otherwises logs the message
 * 
 * @param condition The condition to check
 * @param logType The log type to output if the condition if false
 * @param msg The message to output if the condition is false
 */
static void EXPECT_TRUE(bool condition, LogType logType, std::stringstream msg) {
	if (!condition) {
		log(logType, std::stringstream() << msg.str());
	}
}

/**
 * Checks if the condition is false, otherwises logs the message
 * 
 * @param condition The condition to check
 * @param logType The log type to output if the condition if true
 * @param msg The message to output if the condition is true
 */
static void EXPECT_FALSE(bool condition, LogType logType, std::stringstream msg) {
	if (condition) {
		log(logType, std::stringstream() << msg.str());
	}
}


#endif