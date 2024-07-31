#include "util/loggerv2.h"

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
		const char* file;
		int line;
		const char* func;
		std::time_t time;
		bool start;
	};

	std::vector<Log> logs;
};

static std::time_t master_total_time = 0;
static ProfileLog master_profile_log = (ProfileLog) {
	.tag = "MASTER",
};

static std::unordered_map<std::string, ProfileLog> profile_logs_map;
static std::stack<ProfileLog&> current_clocks;

void logger::clock_start_master(const char* file, int line, const char* func) {

}

void logger::clock_end_master(const char* file, int line, const char* func) {

}

void logger::clock_start(const std::string& tag, const char* file, int line, const char* func) {

}

void logger::clock_end(const char* file, int line, const char* func) {

}