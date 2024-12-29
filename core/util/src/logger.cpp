#include "util/logger.h"

#include <chrono>
#include <ctime>
#include <cstdarg>
#include <unordered_map>
#include <stack>
#include <vector>

#define UNUSED(x) (void)(x)
void logger::track(const std::string& type, const char* format, const char* file, int line,
		   const char* func, ...)
{
	UNUSED(type);
	UNUSED(format);
	UNUSED(file);
	UNUSED(line);
	UNUSED(func);
	// char* buf = 0;
	// size_t buf_size = 0;
	// va_list args;
	// va_start(args, func);
	// size_t size = vsnprintf(buf, buf_size, format, args);
	// va_end(args);

	// buf = (char*) malloc(size + 1);
	// if (!buf)
	// {
	// 	return;
	// }

	// buf_size = size + 1;
	// buf[buf_size-1] = '\0';
	// va_start(args, func);
	// vsnprintf(buf, buf_size, format, args);
	// va_end(args);
	// std::string str(buf);

	// str = "[" + type + "] [" + std::string(file) + ":" +
	// 		std::to_string(line) + "]" + str;

	// /* TODO: complete tracking */


	// free(buf);
}


struct ProfileLog
{
	const std::string tag;

	struct Log
	{
		std::string file;
		int line;
		std::string func;
		std::chrono::high_resolution_clock::time_point start_time;
		std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
		bool ended  = false;
	};

	long long total_elapsed = 0;
	std::vector<Log> logs = std::vector<Log>();
};

static long long master_total_time = 0;
static ProfileLog master_profile_log = (ProfileLog)
{
	.tag = "MASTER",
};

static std::unordered_map<std::string, ProfileLog> profile_logs_map;
static std::stack<std::string> current_clocks;

template <typename... Args>
static inline void log_profile(const char* format, const char* file, int line, const char* func,
							   Args&&... args)
{
	if (AEMU_LOG_ENABLED)
	{
		if (AEMU_PRINT_ENABLED && AEMU_LOG_LEVEL >= AEMU_LOG_DEBUG)
		{
			logger::print_header(ccolor::GREEN + "PRF", file, line, func);
			printf(format, args...);
			std::cout << "\n";
		}

		logger::track("PRF", format, file, line, func, args...);
	}
}

void logger::clock_start_master(const char* file, int line, const char* func)
{
	if (AEMU_PROFILER_ENABLED)
	{
		ProfileLog::Log log = (ProfileLog::Log)
		{
			.file = file,
			.line = line,
			.func = func,
			.start_time = std::chrono::high_resolution_clock::now(),
		};

		master_profile_log.logs.push_back(log);
	}
}

void logger::clock_stop_master()
{
	if (AEMU_PROFILER_ENABLED)
	{
		if (master_profile_log.logs.empty() || master_profile_log.logs.back().ended)
		{
			ERROR("Could not stop the master clock that has not yet started");
			return;
		}
		using namespace std::chrono;

		ProfileLog::Log& log = master_profile_log.logs.back();
		log.end_time = high_resolution_clock::now();
		log.ended = true;

		master_total_time += duration_cast<nanoseconds>(log.end_time - log.start_time).count();
	}
}

void logger::clock_start(const std::string& tag, const char* file, int line, const char* func)
{
	if (AEMU_PROFILER_ENABLED)
	{
		if (master_profile_log.logs.empty())
		{
			clock_start_master(file, line, func);
		}

		ProfileLog::Log log = (ProfileLog::Log)
		{
			.file = file,
			.line = line,
			.func = func,
			.start_time = std::chrono::high_resolution_clock::now(),
		};

		if (!current_clocks.empty() && current_clocks.top() == tag)
		{
			profile_logs_map.at(tag).logs.push_back(log);
			return;
		}

		if (profile_logs_map.find(tag) != profile_logs_map.end())
		{
			current_clocks.push(tag);
			profile_logs_map.at(tag).logs.push_back(log);
			return;
		}

		ProfileLog profile_log = (ProfileLog)
		{
			.tag = tag,
		};
		profile_log.logs.push_back(log);
		profile_logs_map.emplace(tag, profile_log);
		current_clocks.push(tag);
	}
}

static std::tuple<double,std::string> simplify_clocktime(long long time)
{
	std::tuple<double,std::string> simplified;

	if (time <= 10'000)
	{
		simplified = std::tuple<double,std::string>(time, "ns");
	}
	else if (time <= 10'000'000)
	{
		simplified = std::tuple<double,std::string>(time / 1'000.0, "us");
	}
	else if (time <= 10'000'000'000)
	{
		simplified = std::tuple<double,std::string>(time / 1'000'000.0, "ms");
	}
	else
	{
		simplified = std::tuple<double,std::string>(time / 1'000'000'000.0, "s");
	}

	return simplified;
}

void logger::clock_stop()
{
	if (AEMU_PROFILER_ENABLED)
	{
		if (current_clocks.empty() || profile_logs_map.at(current_clocks.top()).logs.back().ended)
		{
			ERROR("Could not stop a clock that has not yet started");
			return;
		}
		using namespace std::chrono;

		ProfileLog& profile_log = profile_logs_map.at(current_clocks.top());
		ProfileLog::Log& log = profile_log.logs.back();
		log.ended = true;
		log.end_time = high_resolution_clock::now();
		auto elapsed = duration_cast<nanoseconds>(log.end_time - log.start_time).count();
		profile_log.total_elapsed += elapsed;

		if (AEMU_PROFILER_LOG_ENABLED)
		{
			auto elapsed_simpl = simplify_clocktime(elapsed);
			auto tot_elapsed_simpl = simplify_clocktime(profile_log.total_elapsed);

			log_profile("%s took %.2f%s, total %.2f%s", log.file.c_str(), log.line,
					log.func.c_str(), current_clocks.top().c_str(),
					std::get<0>(elapsed_simpl), std::get<1>(elapsed_simpl).c_str(),
					std::get<0>(tot_elapsed_simpl), std::get<1>(tot_elapsed_simpl).c_str());
		}
	}
}

void logger::clock_end()
{
	if (AEMU_PROFILER_ENABLED)
	{
		clock_stop();
		current_clocks.pop();
	}
}