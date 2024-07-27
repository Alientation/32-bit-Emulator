#pragma once
#ifndef CONSOLE_COLOR_H
#define CONSOLE_COLOR_H

#include <string>

namespace ccolor {
	static const std::string RESET = "\033[0m";
	static const std::string BOLD = "\033[1m";
	static const std::string DARK = "\033[2m";
	static const std::string ITALICS = "\033[3m";
	static const std::string UNDERLINE = "\033[4m";
	static const std::string HIGHLIGHT = "\033[7m";
	static const std::string STRIKETHROUGH = "\033[9m";
	static const std::string GRAY = "\033[30m";
	static const std::string RED = "\033[31m";
	static const std::string GREEN = "\033[32m";
	static const std::string YELLOW = "\033[33m";
	static const std::string BLUE = "\033[34m";
	static const std::string MAGENTA = "\033[35m";
	static const std::string CYAN = "\033[36m";
	static const std::string WHITE = "\033[37m";
	static const std::string HIGHLIGHT_RED = "\033[41m";
	static const std::string HIGHLIGHT_GREEN = "\033[42m";
	static const std::string HIGHLIGHT_YELLOW = "\033[43m";
	static const std::string HIGHLIGHT_BLUE = "\033[44m";
	static const std::string HIGHLIGHT_MAGENTA = "\033[45m";
	static const std::string HIGHLIGHT_CYAN = "\033[46m";
	static const std::string HIGHLIGHT_WHITE = "\033[47m";
	static const std::string BOLD_GRAY = "\033[90m";
	static const std::string BOLD_RED = "\033[91m";
	static const std::string BOLD_GREEN = "\033[92m";
	static const std::string BOLD_YELLOW = "\033[93m";
	static const std::string BOLD_BLUE = "\033[94m";
	static const std::string BOLD_MAGENTA = "\033[95m";
	static const std::string BOLD_CYAN = "\033[96m";
	static const std::string BOLD_WHITE = "\033[97m";
}

#endif // CONSOLE_COLOR_H