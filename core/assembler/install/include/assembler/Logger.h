#include <sstream>
#include <iostream>
#include "ConsoleColor.h"

#ifndef LOGGER_H
#define LOGGER_H


enum LogType {
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
	std::string logTypeStr;
	switch (logType) {
	case LOG:
		logTypeStr = BOLD + WHITE + "LOG" + RESET;
		break;
	case ERROR:
		logTypeStr = BOLD + RED + "ERROR" + RESET;
		break;
	case WARN:
		logTypeStr = BOLD + YELLOW + "WARN" + RESET;
		break;
	case INFO:
		logTypeStr = BOLD + BLUE + "INFO" + RESET;
		break;
	case DEBUG:
		logTypeStr = BOLD + MAGENTA + "DEBUG" + RESET;
		break;
	case TEST:
		logTypeStr = BOLD + CYAN + "TEST" + RESET;
		break;
	}

	// output the log message
	switch (logType) {
		case LOG:
		case WARN:
		case INFO:
		case DEBUG:
		case TEST:
			std::cout << "[" << logTypeStr << "] " << msg.str() << std::endl;
			break;
		case ERROR:
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