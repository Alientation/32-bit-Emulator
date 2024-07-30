#include "util/loggerv2.h"

#include <cstdarg>

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