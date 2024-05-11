#include <util/Logger.h>
#include <util/File.h>

#include <algorithm>
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

	std::string Logger::LogMessage::to_string() {
		std::stringstream ss;
		ss << "[" << std::put_time(std::localtime(&timestamp), "%H:%M:%S") << "] [" 
			<< group << ":" << Logger::LOGTYPE_TO_STRING(logType) << "]: " << msg;
		return ss.str();
	}

	std::string Logger::LogMessage::to_print_string() {
		std::stringstream ss;
		ss << "[" << ccolor::GRAY << std::put_time(std::localtime(&timestamp), "%H:%M:%S") << ccolor::RESET << "] [" 
		<< ccolor::CYAN << group << ccolor::RESET << ":" << Logger::LOGTYPE_TO_PRINT(logType) << "]: " << msg;
		return ss.str();
	}


	Logger::CONFIG::CONFIG() {
		this->_output_file = "";
		this->_print_logs = true;
		this->_throw_on_error = true;
		this->_flush_every_log = true;
	}

	Logger::CONFIG::~CONFIG() {

	}

	Logger::CONFIG& Logger::CONFIG::output_file(std::string output_file) {
		this->_output_file = output_file;
		return *this;
	}

	Logger::CONFIG& Logger::CONFIG::print_logs(bool print_logs, std::function<std::string(Logger::LogMessage)> print_log_func) {
		this->_print_logs = print_logs;
		this->_print_log_func = print_log_func;
		return *this;
	}

	Logger::CONFIG& Logger::CONFIG::throw_on_error(bool throw_on_error) {
		this->_throw_on_error = throw_on_error;
		return *this;
	}

	Logger::CONFIG& Logger::CONFIG::flush_every_log(bool flush_every_log) {
		this->_flush_every_log = flush_every_log;
		return *this;
	}


	Logger* get_logger(const std::string &logger_id) {
		if (loggers.find(logger_id) == loggers.end()) {
			create_logger(logger_id, Logger::CONFIG());
		}
		return loggers.at(logger_id);
	}

	Logger* create_logger(const std::string &logger_id, Logger::CONFIG config) {
		Logger *logger = new Logger(logger_id, config);
		loggers[logger_id] = logger;
		return logger;
	}

	Logger* remove_logger(const std::string &logger_id) {
		if (loggers.find(logger_id) == loggers.end()) {
			return nullptr;
		}
		Logger* logger = loggers.at(logger_id);
		loggers.erase(logger_id);
		return logger;
	}

	void Logger::dump_all(FileWriter &writer, const std::set<std::string> &queried_log_ids, 
			const std::set<Logger::LogType> &queried_log_types, const std::set<std::string> &queried_log_groups) {
		std::vector<Logger::LogMessage> sorted_logs;

		for (const std::string& log_id : queried_log_ids) {
			if (loggers.find(log_id) == loggers.end()) {
				continue;
			}

			for (Logger::LogMessage& log : loggers.at(log_id)->_logs) {
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


	Logger::Logger(std::string logger_id, CONFIG config) {
		_logger_id = logger_id;
		_config = config;

		if (_config._output_file.empty()) {
			_file_writer = nullptr;
			_log_file = nullptr;
		} else {
			_log_file = new File(_config._output_file);
			_file_writer = new FileWriter(_log_file);
		}
	}

	Logger::~Logger() {
		loggers.erase(_logger_id);

		delete _file_writer;
		delete _log_file;
	}
	
	
	void Logger::log(Logger::LogType log_type, std::string msg, std::string group) {
		Logger::LogMessage log(log_type, msg, group);
		_logs.push_back(log);

		if (_file_writer != nullptr) {
			_file_writer->writeString(log.to_string() + "\n");
			if (_config._flush_every_log) {
				_file_writer->flush();
			}
		}

		if (this->_config._throw_on_error && log_type == Logger::LogType::ERROR) {
			std::cerr << log.to_print_string() << std::endl;
			exit(EXIT_FAILURE);
		}

		if (this->_config._print_logs) {
			if (this->_config._print_log_func) {
				std::cout << (this->_config._print_log_func)(log);
			} else {
				std::cout << log.to_print_string() << std::endl;
			}
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

	void Logger::flush() {
		if (_file_writer) {
			_file_writer->flush();
		}
	}

	void Logger::dump(FileWriter &writer, const std::set<Logger::LogType> &queried_log_types, 
			const std::set<std::string> &queried_log_groups) {
		for (LogMessage log : _logs) {
			if ((queried_log_types.empty() || queried_log_types.find(log.logType) != queried_log_types.end()) &&
				(queried_log_groups.empty() || queried_log_groups.find(log.group) != queried_log_groups.end())) {
				writer.writeString(log.to_string() + "\n");
			}
		}
	}
};