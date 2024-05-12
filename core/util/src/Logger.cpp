#include <util/Logger.h>
#include <util/File.h>
#include <util/StringUtil.h>

#include <algorithm>
#include <format>
#include <iomanip>
#include <map>

#undef log
#undef EXPECT_TRUE
#undef EXPECT_FALSE

namespace lgr {
	std::map<std::string, Logger*> loggers;

	std::string Logger::LOGTYPE_TO_STRING(Logger::LogType log_type) {
		switch(log_type) {
			case Logger::LogType::LOG:
				return "LOG";
			case Logger::LogType::ERROR:
				return "ERR";
			case Logger::LogType::WARN:
				return "WRN";
			case Logger::LogType::INFO:
				return "INF";
			case Logger::LogType::DEBUG:
				return "DBG";
			case Logger::LogType::TEST:
				return "TST";
			default:
				return "UNK";
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
			<< file << ":" << line_num << "] [" << group << ":" << Logger::LOGTYPE_TO_STRING(logType) << "]: " << msg;
		return ss.str();
	}

	std::string Logger::LogMessage::to_print_string() {
		std::stringstream ss;
		// ss << "[" << ccolor::GRAY << std::put_time(std::localtime(&timestamp), "%H:%M:%S") << ccolor::RESET << "] [" 
		// << ccolor::CYAN << group << ccolor::RESET << ":" << Logger::LOGTYPE_TO_PRINT(logType) << "]: " << msg;

		ss << "[" << ccolor::CYAN << group << ccolor::RESET << ":" << Logger::LOGTYPE_TO_PRINT(logType) << "] ["
		<< ccolor::BLUE << string_util::replaceFirst(file,PROJECT_ROOT_DIRECTORY,"") << ccolor::RESET  << ":" << ccolor::MAGENTA 
		<< line_num << ccolor::RESET << "] [" << ccolor::YELLOW << func << ccolor::RESET << "]: " << msg;
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

	void log_f(std::string file, std::string func, int line_num, Logger::LogType log_type, const std::string& msg, const std::string& group) {
		get_logger("")->log_f(file, func, line_num, log_type, msg, group);
	}

	void log_f(std::string file, std::string func, int line_num, Logger::LogType log_type, const std::stringstream& msg, const std::string& group) {
		get_logger("")->log_f(file, func, line_num, log_type, msg, group);
	}

	void EXPECT_TRUE_f(std::string file, std::string func, int line_num, bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group) {
		get_logger("")->EXPECT_TRUE_f(file, func, line_num, condition, logType, msg, group);
	}

	void EXPECT_FALSE_f(std::string file, std::string func, int line_num, bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group) {
		get_logger("")->EXPECT_FALSE_f(file, func, line_num, condition, logType, msg, group);
	}

	void Logger::dump_all(FileWriter &writer, LOG_DUMP log_dump_query) {
		std::vector<Logger::LogMessage> sorted_logs;

		if (log_dump_query._logger_ids.empty()) {
			// add all loggers to the list
			for (std::pair<std::string, Logger*> pair : loggers) {
				log_dump_query._logger_ids.insert(pair.first);
			}
		}

		for (const std::string& log_id : log_dump_query._logger_ids) {
			if (loggers.find(log_id) == loggers.end()) {
				continue;
			}

			for (Logger::LogMessage& log : loggers.at(log_id)->_logs) {
				if (log_dump_query.evaluate(log)) {
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
			writer.write(log.to_string() + "\n");
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
	
	
	void Logger::log_f(std::string file, std::string func, int line_num, Logger::LogType log_type, const std::string& msg, const std::string& group) {
		Logger::LogMessage log = Logger::LogMessage(file, func, line_num, log_type, msg, group);
		_logs.push_back(log);

		if (_file_writer != nullptr) {
			*_file_writer << log.to_string() << "\n";

			// _file_writer->write(log.to_string() + "\n");
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

	void Logger::log_f(std::string file, std::string func, int line_num, Logger::LogType log_type, const std::stringstream& msg, const std::string& group) {
		this->log_f(file, func, line_num, log_type, msg.str(), group);
	}

	void Logger::EXPECT_TRUE_f(std::string file, std::string func, int line_num, bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group) {
		if (!condition) {
			log_f(file, func, line_num, logType, msg, group);
		}
	}

	void Logger::EXPECT_FALSE_f(std::string file, std::string func, int line_num, bool condition, Logger::LogType logType, const std::stringstream& msg, const std::string& group) {
		if (condition) {
			log_f(file, func, line_num, logType, msg, group);
		}
	}

	void Logger::flush() {
		if (_file_writer) {
			_file_writer->flush();
		}
	}

	void Logger::dump(FileWriter &writer, Logger::LOG_DUMP log_dump_query) {
		for (LogMessage& log : _logs) {
			if (log_dump_query.evaluate(log)) {
				writer.write(log.to_string() + "\n");
			}
		}
	}



	Logger::LOG_DUMP::LOG_DUMP() {

	}

	Logger::LOG_DUMP::~LOG_DUMP() {

	}

	bool Logger::LOG_DUMP::evaluate(const Logger::LogMessage& log) {
		#define _CHECK(set,value) (set.empty() || set.find(value) != set.end())
		return _CHECK(_log_types,log.logType) &&
				_CHECK(_groups, log.group) &&
				_CHECK(_files, log.file) &&
				_CHECK(_lines, log.line_num) &&
				_CHECK(_funcs, log.func);
		#undef _CHECK
	}

	Logger::LOG_DUMP& Logger::LOG_DUMP::files(std::set<std::string> files) {
		_files = files;
		return *this;
	}

	Logger::LOG_DUMP& Logger::LOG_DUMP::funcs(std::set<std::string> funcs) {
		_funcs = funcs;
		return *this;
	}
	
	Logger::LOG_DUMP& Logger::LOG_DUMP::lines(std::set<int> lines) {
		_lines = lines;
		return *this;
	}

	Logger::LOG_DUMP& Logger::LOG_DUMP::groups(std::set<std::string> groups) {
		_groups = groups;
		return *this;
	}
	Logger::LOG_DUMP& Logger::LOG_DUMP::log_types(std::set<Logger::LogType> log_types) {
		_log_types = log_types;
		return *this;
	}

	Logger::LOG_DUMP& Logger::LOG_DUMP::logger_ids(std::set<std::string> logger_ids) {
		_logger_ids = logger_ids;
		return *this;
	}
};