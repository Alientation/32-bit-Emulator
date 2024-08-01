#include "util/loggerv2.h"

#include <chrono>
#include <ctime>
#include <cstdarg>
#include <unordered_map>
#include <stack>
#include <vector>

void logger::track(const std::string& type, const char* format, const char* file, int line,
		   const char* func, ...)
{
	char* buf = 0;
	size_t buf_size = 0;
	va_list args;
	va_start(args, func);
	size_t size = vsnprintf(buf, buf_size, format, args);
	va_end(args);

	buf = (char*) malloc(size + 1);
	if (!buf) {
		return;
	}

	buf_size = size + 1;
	buf[buf_size-1] = '\0';
	va_start(args, func);
	vsnprintf(buf, buf_size, format, args);
	va_end(args);
	std::string str(buf);

	/* TODO: complete tracking */

	free(buf);
}


struct ProfileLog {
	const std::string tag;

	struct Log {
		std::string file;
		int line;
		std::string func;
		std::chrono::high_resolution_clock::time_point start_time;
		std::chrono::high_resolution_clock::time_point end_time;
		bool ended  = false;
	};

	long long total_elapsed;
	std::vector<Log> logs;
};

static long long master_total_time = 0;
static ProfileLog master_profile_log = (ProfileLog) {
	.tag = "MASTER",
};

static std::unordered_map<std::string, ProfileLog> profile_logs_map;
static std::stack<std::string> current_clocks;

void logger::clock_start_master(const char* file, int line, const char* func) {
	if (AEMU_PROFILER_ENABLED) {
		auto start = std::clock();

		ProfileLog::Log log = (ProfileLog::Log) {
			.file = file,
			.line = line,
			.func = func,
			.start_time = std::chrono::high_resolution_clock::now(),
		};

		master_profile_log.logs.push_back(log);
	}
}

void logger::clock_stop_master() {
	if (AEMU_PROFILER_ENABLED) {
		if (master_profile_log.logs.empty() || master_profile_log.logs.back().ended) {
			ERROR("Could not stop the master clock that has not yet started");
			return;
		}

		ProfileLog::Log& log = master_profile_log.logs.back();
		log.end_time = std::chrono::high_resolution_clock::now();
		log.ended = true;

		master_total_time += std::chrono::duration_cast<std::chrono::nanoseconds>(log.end_time - log.start_time).count();
	}
}

void logger::clock_start(const std::string& tag, const char* file, int line, const char* func) {
	if (AEMU_PROFILER_ENABLED) {
		if (master_profile_log.logs.empty()) {
			clock_start_master(file, line, func);
		}

		ProfileLog::Log log = (ProfileLog::Log) {
			.file = file,
			.line = line,
			.func = func,
			.start_time = std::chrono::high_resolution_clock::now(),
		};

		if (!current_clocks.empty() && current_clocks.top() == tag) {
			profile_logs_map.at(tag).logs.push_back(log);
			return;
		}

		if (profile_logs_map.find(tag) != profile_logs_map.end()) {
			ERROR_SS(std::stringstream() << "Duplicate clock tag: " + tag);
			return;
		}

		ProfileLog profile_log = (ProfileLog) {
			.tag = tag,
		};
		profile_log.logs.push_back(log);
		profile_logs_map.emplace(tag, profile_log);
		current_clocks.push(tag);
	}
}

void logger::clock_stop() {
	if (AEMU_PROFILER_ENABLED) {
		if (current_clocks.empty() || profile_logs_map.at(current_clocks.top()).logs.back().ended) {
			ERROR("Could not stop a clock that has not yet started");
			return;
		}

		ProfileLog profile_log = profile_logs_map.at(current_clocks.top());
		ProfileLog::Log& log = profile_log.logs.back();
		log.ended = true;
		log.end_time = std::chrono::high_resolution_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(log.end_time - log.start_time).count();
		profile_log.total_elapsed += elapsed;

		if (AEMU_PROFILER_LOG_ENABLED) {
			double elapsed_simplified = 0;
			std::string elapsed_unit = "";

			double total_elapsed_simplified = 0;
			std::string total_elapsed_unit = "";

			/*TODO: refactor in future. use a lambda instead */
			if (elapsed <= 10'000) {
				elapsed_simplified = elapsed;
				elapsed_unit = "ns";
			} else if (elapsed <= 10'000'000) {
				elapsed_simplified = elapsed / 1'000.0;
				elapsed_unit = "us";
			} else if (elapsed <= 10'000'000'000) {
				elapsed_simplified = elapsed / 1'000'000.0;
				elapsed_unit = "ms";
			} else {
				elapsed_simplified = elapsed / 1'000'000'000.0;
				elapsed_unit = "s";
			}

			if (profile_log.total_elapsed <= 10'000) {
				total_elapsed_simplified = profile_log.total_elapsed;
				total_elapsed_unit = "ns";
			} else if (profile_log.total_elapsed <= 10'000'000) {
				total_elapsed_simplified = profile_log.total_elapsed / 1'000.0;
				total_elapsed_unit = "us";
			} else if (profile_log.total_elapsed <= 10'000'000'000) {
				total_elapsed_simplified = profile_log.total_elapsed / 1'000'000.0;
				total_elapsed_unit = "ms";
			} else {
				total_elapsed_simplified = profile_log.total_elapsed / 1'000'000'000.0;
				total_elapsed_unit = "s";
			}

			INFO("PROFILER: %s took %.2f%s, total %.2f%s.", current_clocks.top().c_str(),
					elapsed_simplified, elapsed_unit.c_str(), total_elapsed_simplified,
					total_elapsed_unit.c_str());
		}
	}
}

void logger::clock_end() {
	if (AEMU_PROFILER_ENABLED) {
		clock_stop();
		current_clocks.pop();
	}
}