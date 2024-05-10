#include "util/Logger.h"
#include "util/File.h"

#include <format>
#include <iomanip>
#include <map>

namespace lgr {
	std::map<std::string, Logger*> loggers;

	std::string Logger::LOGTYPE_TO_STRING(Logger::LogType log_type) {
		switch(log_type) {
			case Logger::LogType::LOG:
				return "LOG";
			case Logger::LogType::ERROR:
				return "ERROR";
			case Logger::LogType::WARN:
				return "WARN";
			case Logger::LogType::INFO:
				return "INFO";
			case Logger::LogType::DEBUG:
				return "DEBUG";
			case Logger::LogType::TEST:
				return "TEST";
			default:
				return "UNKNOWN";
		}
	}

	std::string Logger::LOGTYPE_TO_PRINT(Logger::LogType log_type) {
		std::string log_name = Logger::LOGTYPE_TO_STRING(log_type);

		switch(log_type) {
			case Logger::LogType::LOG:
				return BOLD + WHITE + log_name + RESET;
			case Logger::LogType::ERROR:
				return BOLD + RED + log_name + RESET;
			case Logger::LogType::WARN:
				return BOLD + YELLOW + log_name + RESET;
			case Logger::LogType::INFO:
				return BOLD + BLUE + log_name + RESET;
			case Logger::LogType::DEBUG:
				return BOLD + MAGENTA + log_name + RESET;
			case Logger::LogType::TEST:
				return BOLD + CYAN + log_name + RESET;
			default:
				return log_name;
		}
	}

	Logger get_logger(const std::string &logger_id) {
		if (loggers.find(logger_id) == loggers.end()) {
			create_logger(logger_id, true, true);
		}
		return *loggers.at(logger_id);
	}

	Logger create_logger(const std::string &logger_id, bool print_logs, bool throw_on_error, const std::string &log_file_path) {
		Logger* logger = new Logger(print_logs, throw_on_error, log_file_path);
		loggers[logger_id] = logger;
		return *logger;
	}

	Logger::Logger(bool print_logs, bool throw_on_error, const std::string &log_file_path) {
		this->print_logs = print_logs;
		this->throw_on_error = throw_on_error;
		if (log_file_path.empty()) {
			file_writer = nullptr;
			log_file = nullptr;
		} else {
			log_file = new File(log_file_path);
			file_writer = new FileWriter(log_file);
		}
	}

	Logger::~Logger() {
		delete file_writer;
		delete log_file;
	}

	void Logger::log(Logger::LogType log_type, std::string msg, std::string group) {
		Logger::LogMessage log(log_type, msg, group);
		logs.push_back(log);

		if (file_writer != nullptr) {
			std::stringstream ss;
			ss << "[" << std::put_time(std::localtime(&log.timestamp), "%T") << "] [" << group << ":" << Logger::LOGTYPE_TO_STRING(log_type) << "]: " << msg << std::endl;
			file_writer->writeString(ss.str());
		}

		if (this->throw_on_error && log_type == Logger::LogType::ERROR) {
			std::cerr << "[" << std::put_time(std::localtime(&log.timestamp), "%T") << "] [" << group << ":" << Logger::LOGTYPE_TO_PRINT(log_type) << "]: " << msg << std::endl;
			exit(EXIT_FAILURE);
		}

		if (this->print_logs) {
			std::cout << "[" << std::put_time(std::localtime(&log.timestamp), "%T") << "] [" << group << ":" << Logger::LOGTYPE_TO_PRINT(log_type) << "]: " << msg << std::endl;
		}
	}

	void Logger::EXPECT_TRUE(bool condition, Logger::LogType logType, std::string msg, std::string group) {
		if (!condition) {
			log(logType, msg, group);
		}
	}

	void Logger::EXPECT_FALSE(bool condition, Logger::LogType logType, std::string msg, std::string group) {
		if (condition) {
			log(logType, msg, group);
		}
	}

	void Logger::dump(FileWriter &writer, const std::set<Logger::LogType> &queried_log_types, 
			const std::set<std::string> &queried_log_groups) {
		for (LogMessage log : logs) {
			if ((queried_log_types.empty() || queried_log_types.find(log.logType) != queried_log_types.end()) &&
				(queried_log_groups.empty() || queried_log_groups.find(log.group) != queried_log_groups.end())) {
				std::stringstream ss;
				ss << "[" << std::put_time(std::localtime(&log.timestamp), "%T") << "] [" << log.group << ":" << Logger::LOGTYPE_TO_STRING(log.logType) << "]: " << log.msg << std::endl;
				writer.writeString(ss.str());
			}
		}
	}
};