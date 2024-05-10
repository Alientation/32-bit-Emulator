#include "util/Logger.h"
#include "util/File.h"

#include <algorithm>
#include <format>
#include <iomanip>
#include <map>

namespace lgr {
	Logger::CONFIG::CONFIG() {
		this->_output_file = "";
		this->_print_logs = true;
		this->_throw_on_error = true;
	}

	Logger::CONFIG::~CONFIG() {

	}

	Logger::CONFIG* Logger::CONFIG::output_file(std::string output_file) {
		this->_output_file = output_file;
		return this;
	}

	Logger::CONFIG* Logger::CONFIG::print_logs(bool print_logs) {
		this->_print_logs = print_logs;
		return this;
	}

	Logger::CONFIG* Logger::CONFIG::throw_on_error(bool throw_on_error) {
		this->_throw_on_error = throw_on_error;
		return this;
	}

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
		using namespace ccolor;
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

	Logger* get_logger(const std::string &logger_id) {
		if (loggers.find(logger_id) == loggers.end()) {
			create_logger(logger_id, Logger::CONFIG());
		}
		return loggers.at(logger_id);
	}

	Logger* create_logger(const std::string &logger_id, Logger::CONFIG config) {
		Logger *logger = new Logger(config);
		loggers[logger_id] = logger;
		return logger;
	}

	void Logger::dump_all(FileWriter &writer, const std::set<std::string> &queried_log_ids, 
			const std::set<Logger::LogType> &queried_log_types, const std::set<std::string> &queried_log_groups) {
		std::vector<Logger::LogMessage> sorted_logs;

		for (const std::string& log_id : queried_log_ids) {
			if (loggers.find(log_id) == loggers.end()) {
				continue;
			}

			for (Logger::LogMessage& log : loggers.at(log_id)->logs) {
				if ((queried_log_types.empty() || queried_log_types.find(log.logType) != queried_log_types.end()) &&
					(queried_log_groups.empty() || queried_log_groups.find(log.group) != queried_log_groups.end())) {
					sorted_logs.push_back(log);
				}
			}
		}

		std::sort(sorted_logs.begin(), sorted_logs.end(), [](const Logger::LogMessage& lhs, const Logger::LogMessage& rhs) {
			if (lhs.timestamp != rhs.timestamp) {
				return std::difftime(lhs.timestamp, rhs.timestamp) < 0;
			} else if (lhs.group != rhs.group) {
				return lhs.group.compare(rhs.group) < 0;
			} else if (lhs.logType != rhs.logType) {
				return lhs.logType < rhs.logType;
			} else {
				return true;
			}
		});

		for (LogMessage log : sorted_logs) {
			writer.writeString(log.to_string() + "\n");
		}
	}

	Logger::Logger(CONFIG config) {
		this->_config = config;

		if (this->_config._output_file.empty()) {
			file_writer = nullptr;
			log_file = nullptr;
		} else {
			log_file = new File(this->_config._output_file);
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
			file_writer->writeString(log.to_string() + "\n");
		}

		if (this->_config._throw_on_error && log_type == Logger::LogType::ERROR) {
			std::cerr << log.to_print_string() << std::endl;
			exit(EXIT_FAILURE);
		}

		if (this->_config._print_logs) {
			std::cout << log.to_print_string() << std::endl;
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
				writer.writeString(log.to_string() + "\n");
			}
		}
	}
};