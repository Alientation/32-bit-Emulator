#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <string>
#include <regex>
#include <sstream>
#include <vector>
#include <iostream>
#include <fstream>
#include <set>
#include <map>

#include <../src/util/ConsoleColor.h>
#include <../src/Emulator/AlienCPU.h>


class Assembler;

static std::string validFileNameRegex = "[a-zA-Z0-9_\\-\\(\\)\\,\\.\\*]+";

// labels cannot start with a numeric character
static std::string validLabelNameRegex = "[a-zA-Z_][a-zA-Z0-9_]*";

enum LoggerType {
	LOG,
	LOG_TOKENIZER,
	LOG_PARSER,
	LOG_ASSEMBLER,
	LOG_LINKER
};
enum ErrorType {
	ERROR,
	INVALID_TOKEN_ERROR,
	FILE_ERROR,
	MULTIPLE_DEFINITION_ERROR,
	UNRECOGNIZED_TOKEN_ERROR,
	MISSING_TOKEN_ERROR,
	INTERNAL_ERROR
};
enum WarnType {
	WARN,
};

enum DirectiveType {
	DATA, TEXT,
	END,
	ORG_RELATIVE, ORG_ABSOLUTE,
	DB_LO, D2B_LO, DW_LO, D2W_LO, DB_HI, D2B_HI, DW_HI, D2W_HI,
	ASCII, ASCIZ,
	ADVANCE, FILL, SPACE,
	GLOBAL, EXTERN, EQU,
	CHECKPC, ALIGN,
	INCLUDE, 
	SCOPE, SCEND, 
	MACRO, MACEND, INVOKE
};

static std::map<std::string, DirectiveType> directiveMap = {
	{".data", DATA}, {".text", TEXT},
	{".end", END},
	{".org", ORG_RELATIVE}, {".org*", ORG_ABSOLUTE},
	{".db", DB_LO}, {".d2b", D2B_LO}, {".dw", DW_LO}, {".d2w", D2W_LO}, 
	{".db*", DB_HI}, {".d2b*", D2B_HI}, {".dw*", DW_HI}, {".d2w*", D2W_HI},
	{".ascii", ASCII}, {".asciiz", ASCIZ},
	{".advance", ADVANCE}, {".fill", FILL}, {".space", SPACE},
	{".global", GLOBAL}, {".extern", EXTERN}, {".define", EQU},
	{".checkpc", CHECKPC}, {".align", ALIGN},
	{".include", INCLUDE},
	{".scope", SCOPE}, {".scend", SCEND},
	{".macro", MACRO}, {".macend", MACEND}, {".invoke", INVOKE}
};

struct Token {
	std::string string;
	int location; // number of characters from the first character of the source code
	int lineNumber;

	Token(std::string string, int location, int lineNumber) : 
			string(string), location(location), lineNumber(lineNumber) {}
	
	std::string errorstring() {
		return "\'" + string + "\' at line " + std::to_string(lineNumber);
	}
};

struct MemorySegment {
	Word startAddress;
	std::vector<Byte> bytes;

	MemorySegment(Word startAddress) : startAddress(startAddress) {}
	Word getEndAddress() {
		return startAddress + bytes.size() - 1;
	}

	std::string stringifyStartAddress() {
		return stringifyHex(startAddress);
	}
	
	std::string prettyStringifyStartAddress() {
		return prettyStringifyValue(stringifyHex(startAddress));
	}

	std::string stringifyEndAddress() {
		return stringifyHex(getEndAddress());
	}

	std::string prettyStringifyEndAddress() {
		return prettyStringifyValue(stringifyHex(getEndAddress()));
	}
};

enum SegmentType {
	SEGMENT_TEXT,
	SEGMENT_DATA
};

struct Segment {
	SegmentType type;
	std::string name;
	Word programCounter;

	std::map<Word, MemorySegment*> relativeMemoryMap;
	std::map<Word, MemorySegment*> absoluteMemoryMap;

	Segment(SegmentType type, std::string name) : type(type), name(name), programCounter(0) {}
};


struct Macro {
	std::string name;
	std::map<int, std::pair<std::vector<std::string>, std::vector<Token>>> macros; // map the number of parameters to the macro parameter names and full macro definition

	Macro(std::string name) : name(name) {}
};


enum SymbolStatus {
	SYMBOL_DEFINED_RAW,
	SYMBOL_DEFINED_EVALUATED,
	SYMBOL_UNDEFINED
};

enum SymbolValueType {
	SYMBOL_VALUE_ABSOLUTE,
	SYMBOL_VALUE_RELATIVE
};

struct Symbol {
	std::string symbol;
	SymbolStatus status;
	SymbolValueType valuetype;
	Word value;
	std::string valueString;
	

	/**
	 * Defines a symbol that has a value and is already evaluated
	 */
	Symbol(std::string symbol, Word value, SymbolValueType valuetype) : 
			symbol(symbol), value(value), valueString(std::to_string(value)), valuetype(valuetype), status(SYMBOL_DEFINED_EVALUATED) {}

	/**
	 * Defines a symbol that has a value but is not yet evaluated
	 */
	Symbol(std::string symbol, std::string valueString, SymbolValueType valuetype) : 
			symbol(symbol), valueString(valueString), valuetype(valuetype), status(SYMBOL_DEFINED_RAW) {}

	/**
	 * Defines a symbol that has no value associated with it yet.
	 */
	Symbol(std::string symbol) : 
			symbol(symbol), status(SYMBOL_UNDEFINED) {}
};


struct Scope {
	Scope* parent;

	std::map<std::string, Symbol*> symbols;
	std::map<std::string, Macro*> macros;

	Scope() : parent(nullptr) {}
	Scope(Scope* parent) : parent(parent) {}

	Symbol* getSymbol(std::string symbol) {
		if (symbols.count(symbol) == 0) {
			if (parent == nullptr) {
				return nullptr;
			}
			return parent->getSymbol(symbol);
		}
		return symbols[symbol];
	}

	Macro* getMacro(std::string macro) {
		if (macros.count(macro) == 0) {
			if (parent == nullptr) {
				return nullptr;
			}
			return parent->getMacro(macro);
		}
		return macros[macro];
	}
};


struct ObjectFile {
	std::vector<std::string> includedFiles;
	std::vector<Token> tokens;

	Scope* filescope;
	std::map<int, Scope*> scopeMap;
	std::map<SegmentType, std::map<std::string, Segment*>> segmentMap;

	std::set<std::string> markedGlobalSymbols;
	std::map<std::string,std::set<int>> markedGlobalMacros; // macro name and number of parameters

	std::set<std::string> markedExternSymbols;
	std::map<std::string,std::set<int>> markedExternMacros;

	ObjectFile(std::vector<Token> tokens) : tokens(tokens) {
		filescope = new Scope();

		segmentMap[SEGMENT_DATA] = std::map<std::string, Segment*>();
		segmentMap[SEGMENT_TEXT] = std::map<std::string, Segment*>();

		// create default segment
		segmentMap[SEGMENT_DATA][""] = new Segment(SEGMENT_DATA, "");
		segmentMap[SEGMENT_TEXT][""] = new Segment(SEGMENT_TEXT, "");
	}
};


/**
 * 
 */
static bool debugOn = true;
class Assembler {
	public:
		Assembler(std::vector<std::string> files);

	private:
		enum AssemblerStatus {
			TOKENIZING, PARSING, LINKING, ASSEMBLING
		};
		AssemblerStatus status;


		std::vector<std::string> files;
		std::map<std::string, ObjectFile*> objectFilesMap;

		ObjectFile* currentObjectFile = nullptr;
		Scope* currentScope = nullptr;
		int currentTokenI = 0;
		Segment* currentSegment = nullptr;
		bool isRelativeMemory = true;


		void tokenize(std::string filename);
		void preprocess();
		void parse(std::string filename);
		void linker();
		void assemble();

		void defineLabel(std::string labelname, Word value);
		void startScope();
		void endScope();

		void writeToFile();
        void writeByte(Byte value);
        void writeTwoBytes(u16 value, bool lowEndian = true);
        void writeWord(Word value, bool lowEndian = true);
        void writeTwoWords(u64 value, bool lowEndian = true);
        void writeBytes(u64 value, Byte bytes, bool lowEndian = true);


		// token processing
        u64 EXPECT_PARSEDVALUE(u64 min, u64 max);
        u64 EXPECT_PARSEDVALUE(std::string val, u64 min, u64 max);
        void EXPECT_OPERAND();
        void EXPECT_NO_OPERAND();
        bool HAS_OPERAND(bool requireSameLine = true);


		// assembler directives
        void DIR_DATA();
        void DIR_TEXT();

		void DIR_END();

        void DIR_ORG_RELATIVE();
		void DIR_ORG_ABSOLUTE();

        void DIR_DB_LO();
        void DIR_D2B_LO();
        void DIR_DW_LO();
        void DIR_D2W_LO();
        void DIR_DB_HI();
        void DIR_D2B_HI();
        void DIR_DW_HI();
        void DIR_D2W_HI();

        void DIR_ASCII();
        void DIR_ASCIZ();

        void DIR_ADVANCE();
        void DIR_FILL();
        void DIR_SPACE();

        void DIR_GLOBAL();
        void DIR_EXTERN();
        void DIR_EQU();

        void DIR_CHECKPC();
        void DIR_ALIGN();

        void DIR_INCLUDE();

        void DIR_SCOPE();
        void DIR_SCEND();
        void DIR_MACRO();
        void DIR_MACEND();
        void DIR_INVOKE();

        // helper directive functions
        void defineBytes(std::string token, Byte bytes, bool lowEndian);

        /**
         * Map of directive types to their respective processing functions.
         */
        typedef void (Assembler::*DirectiveFunction)();
        std::map<DirectiveType,DirectiveFunction> processDirective = {
            {DATA, &Assembler::DIR_DATA}, {TEXT, &Assembler::DIR_TEXT},
            {END, &Assembler::DIR_END},
            {ORG_RELATIVE, &Assembler::DIR_ORG_RELATIVE},
            {DB_LO, &Assembler::DIR_DB_LO}, {D2B_LO, &Assembler::DIR_D2B_LO}, 
            {DW_LO, &Assembler::DIR_DW_LO}, {D2W_LO, &Assembler::DIR_D2W_LO},
            {DB_HI, &Assembler::DIR_DB_HI}, {D2B_HI, &Assembler::DIR_D2B_HI}, 
            {DW_HI, &Assembler::DIR_DW_HI}, {D2W_HI, &Assembler::DIR_D2W_HI},
            {ASCII, &Assembler::DIR_ASCII}, {ASCIZ, &Assembler::DIR_ASCIZ},
            {ADVANCE, &Assembler::DIR_ADVANCE}, {FILL, &Assembler::DIR_FILL}, 
            {SPACE, &Assembler::DIR_SPACE},
            {GLOBAL, &Assembler::DIR_GLOBAL}, {EXTERN, &Assembler::DIR_EXTERN},
            {EQU, &Assembler::DIR_EQU},
            {CHECKPC, &Assembler::DIR_CHECKPC}, {ALIGN, &Assembler::DIR_ALIGN},
            {INCLUDE, &Assembler::DIR_INCLUDE}, 
            {SCOPE, &Assembler::DIR_SCOPE}, {SCEND, &Assembler::DIR_SCEND},
            {MACRO, &Assembler::DIR_MACRO}, {MACEND, &Assembler::DIR_MACEND}, 
            {INVOKE, &Assembler::DIR_INVOKE}
        };
};




/**
 * 
 */
static void log(LoggerType type, std::stringstream msg) {
	if (!debugOn) {
        return;
    }

	std::string prefix;
	switch(type) {
        case LOG_TOKENIZER:
            prefix = BOLD_BLUE + "[tokenizing]" + RESET;
            break;
        case LOG_PARSER:
            prefix = BOLD_GREEN + "[parsing]" + RESET;
            break;
        case LOG_ASSEMBLER:
            prefix = BOLD_MAGENTA + "[assembling]" + RESET;
            break;
		case LOG_LINKER:
			prefix = BOLD_YELLOW + "[linking]" + RESET;
        default:
            prefix = BOLD_CYAN + "[log]" + RESET;
    }

    std::cout << prefix << " " << msg.str() << std::endl;
}

/**
 * 
 */
static void error(ErrorType type, std::stringstream msg) {
	std::string name;
    switch(type) {
        case INVALID_TOKEN_ERROR:
            name = BOLD_RED + "[invalid_token]" + RESET;
            break;
        case FILE_ERROR:
            name = BOLD_RED + "[file_error]" + RESET;
            break;
        case MULTIPLE_DEFINITION_ERROR:
            name = BOLD_RED + "[multiple_definition]" + RESET;
            break;
        case UNRECOGNIZED_TOKEN_ERROR:
            name = BOLD_RED + "[unrecognized_token]" + RESET;
            break;
        case MISSING_TOKEN_ERROR:
            name = BOLD_YELLOW + "[missing_token]" + RESET;
        case INTERNAL_ERROR:
            name = BOLD_RED + "[internal_error]" + RESET;
            break;
        default:
            name = BOLD_RED + "[error]" + RESET;
    }

	std::stringstream msgStream;
	msgStream << name << " " << msg.str() << std::endl;
	throw std::runtime_error(msgStream.str());
}

/**
 * 
 */
static void warn(WarnType type, std::stringstream msg) {
	std::string name;
	switch(type) {
		case WARN:
		default:
			name = BOLD_YELLOW + "[warn]" + RESET;
	}

	std::cout << name << " " << msg.str() << std::endl;
}



/**
 * Splits a string into a vector of strings based on the given delimiter.
 * 
 * @param source The string to split.
 * @param delimiter The delimiter to split the string on.
 * @return A vector of strings split from the given string based on the given delimiter.
 */
static std::vector<std::string> split(std::string source, char delimiter) {
    std::vector<std::string> lines;
    std::string line;
    std::stringstream ss(source);

    while (std::getline(ss, line, delimiter)) {
        lines.push_back(line);
    }

    return lines;
}


/**
 * Trim any leading and trailing whitespace from the given string.
 * 
 * @param source The string to trim
 * @return The given string with any leading and trailing whitespace removed.
 */
static std::string trim(std::string& string) {
    std::string result = string;

    // remove leading whitespace
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](int ch) {
        return !std::isspace(ch);
    }));

    // remove trailing whitespace
    result.erase(std::find_if(result.rbegin(), result.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), result.end());

    return result;
}


/**
 * Converts a vector of strings into a single string.
 * 
 * Formats the string in the following way:
 * [string1, string2, string3, ...]
 * 
 * @param strings The vector of strings to convert.
 * @return A single string containing all of the strings in the given vector.
 */
static std::string tostring(std::vector<std::string>& strings) {
    if (strings.size() == 0) {
        return "[]";
    }

    std::string result = "[";

    for (std::string string : strings) {
        result += string + ", ";
    }

    // remove the last comma and space from string
    result.pop_back();
    result.pop_back();

    result += "]";

    return result;
}


/**
 * Converts a vector of tokens into a single string.
 * 
 * Formats the string in the following way:
 * [token1, token2, token3, ...]
 * 
 * @param tokens The vector of tokens to convert.
 * @return A single string containing all of the tokens in the given vector.
 */
static std::string tostring(std::vector<Token>& tokens) {
    if (tokens.size() == 0) {
        return "[]";
    }

    std::string result = "[";

    for (Token token : tokens) {
        result += token.string + ", ";
    }

    // remove the last comma and space from string
    result.pop_back();
    result.pop_back();

    result += "]";

    return result;
}


/**
 * Check if a string contains only digits
 * 
 * @param str string to check
 * @return true if the string contains only digits
 */
static bool isNumber(const std::string& string) {
    std::string::const_iterator it = string.begin();
    while (it != string.end() && std::isdigit(*it)) {
        ++it;
    }
    return !string.empty() && it == string.end();
}


/**
 * Check if a string contains only hexadecimal digits
 * 
 * @param str string to check
 * @return true if the string contains only hexadecimal digits
 */
static bool isHexadecimalNumber(const std::string& string) {
    std::string::const_iterator it = string.begin();
    while (it != string.end() && std::isxdigit(*it)) {
        ++it;
    }
    return !string.empty() && it == string.end();
}

/**
 * Map of addressing modes to how many bytes are needed
 */
static std::map<AddressingMode,u8> addressingModeOperandBytes = {
    {ACCUMULATOR, 0},
    {IMPLIED, 0},
    {IMMEDIATE, 2},
    {RELATIVE, 2},
    {INDIRECT, 4},
    {ABSOLUTE, 4},
    {ABSOLUTE_XINDEXED, 4},
    {ABSOLUTE_YINDEXED, 4},
    {ZEROPAGE, 2},
    {ZEROPAGE_XINDEXED, 2},
    {ZEROPAGE_YINDEXED, 2},
    {XINDEXED_INDIRECT, 2},
    {INDIRECT_YINDEXED, 2}
};


/**
 * Map of addressing modes to their names
 */
static std::map<AddressingMode,std::string> addressingModeNames = {
    {ACCUMULATOR, "ACCUMULATOR"},
    {IMPLIED, "IMPLIED"},
    {IMMEDIATE, "IMMEDIATE"},
    {RELATIVE, "RELATIVE"},
    {INDIRECT, "INDIRECT"},
    {ABSOLUTE, "ABSOLUTE"},
    {ABSOLUTE_XINDEXED, "ABSOLUTE_XINDEXED"},
    {ABSOLUTE_YINDEXED, "ABSOLUTE_YINDEXED"},
    {ZEROPAGE, "ZEROPAGE"},
    {ZEROPAGE_XINDEXED, "ZEROPAGE_XINDEXED"},
    {ZEROPAGE_YINDEXED, "ZEROPAGE_YINDEXED"},
    {XINDEXED_INDIRECT, "XINDEXED_INDIRECT"},
    {INDIRECT_YINDEXED, "INDIRECT_YINDEXED"}
};


/**
 * Map of processor instructions syntactical names and addressing mode to their opcode value
 */
static std::map<std::string, std::map<AddressingMode, u8>> instructionMap = {
    {"LDA", 
        {
            {IMMEDIATE, 0xA9}, 
            {ABSOLUTE, 0xAD}, 
            {ABSOLUTE_XINDEXED, 0xBD}, 
            {ABSOLUTE_YINDEXED, 0xB9},
            {ZEROPAGE, 0xA5},
            {ZEROPAGE_XINDEXED, 0xB5},
            {XINDEXED_INDIRECT, 0xA1},
            {INDIRECT_YINDEXED, 0xB1}
        }
    }, {"LDX",
        {
            {IMMEDIATE, 0xA2},
            {ABSOLUTE, 0xAE},
            {ABSOLUTE_YINDEXED, 0xBE},
            {ZEROPAGE, 0xA6},
            {ZEROPAGE_YINDEXED, 0xB6}
        }
    }, {"LDY", 
        {
            {IMMEDIATE, 0xA0},
            {ABSOLUTE, 0xAC},
            {ABSOLUTE_XINDEXED, 0xBC},
            {ZEROPAGE, 0xA4},
            {ZEROPAGE_XINDEXED, 0xB4}
        }
    }, {"STA",
        {
            {ABSOLUTE, 0x8D},
            {ABSOLUTE_XINDEXED, 0x9D},
            {ABSOLUTE_YINDEXED, 0x99},
            {ZEROPAGE, 0x85},
            {ZEROPAGE_XINDEXED, 0x95},
            {XINDEXED_INDIRECT, 0x81},
            {INDIRECT_YINDEXED, 0x91}
        }
    }, {"STX",
        {
            {ABSOLUTE, 0x8E},
            {ZEROPAGE, 0x86},
            {ZEROPAGE_YINDEXED, 0x96}
        }
    }, {"STY",
        {
            {ABSOLUTE, 0x8C},
            {ZEROPAGE, 0x84},
            {ZEROPAGE_XINDEXED, 0x94}
        }
    }, {"TAX",
        {
            {IMPLIED, 0xAA}
        }
    }, {"TAY",
        {
            {IMPLIED, 0xA8}
        }
    }, {"TSX",
        {
            {IMPLIED, 0xBA}
        }
    }, {"TXA",
        {
            {IMPLIED, 0x8A}
        }
    }, {"TXS",
        {
            {IMPLIED, 0x9A}
        }
    }, {"TYA",
        {
            {IMPLIED, 0x98}
        }
    }, {"PHA",
        {
            {IMPLIED, 0x48}
        }
    }, {"PHP",
        {
            {IMPLIED, 0x08}
        }
    }, {"PLA",
        {
            {IMPLIED, 0x68}
        }
    }, {"PLP",
        {
            {IMPLIED, 0x28}
        }
    }, {"DEC",
        {
            {ABSOLUTE, 0xCE},
            {ABSOLUTE_XINDEXED, 0xDE},
            {ZEROPAGE, 0xC6},
            {ZEROPAGE_XINDEXED, 0xD6}
        }
    }, {"DEX",
        {
            {IMPLIED, 0xCA}
        }
    }, {"DEY",
        {
            {IMPLIED, 0x88}
        }
    }, {"INC",
        {
            {ABSOLUTE, 0xEE},
            {ABSOLUTE_XINDEXED, 0xFE},
            {ZEROPAGE, 0xE6},
            {ZEROPAGE_XINDEXED, 0xF6}
        }
    }, {"INX",
        {
            {IMPLIED, 0xE8}
        }
    }, {"INY",
        {
            {IMPLIED, 0xC8}
        }
    }, {"ADC",
        {
            {IMMEDIATE, 0x69},
            {ABSOLUTE, 0x6D},
            {ABSOLUTE_XINDEXED, 0x7D},
            {ABSOLUTE_YINDEXED, 0x79},
            {ZEROPAGE, 0x65},
            {ZEROPAGE_XINDEXED, 0x75},
            {XINDEXED_INDIRECT, 0x61},
            {INDIRECT_YINDEXED, 0x71}
        }
    }, {"SBC",
        {
            {IMMEDIATE, 0xE9},
            {ABSOLUTE, 0xED},
            {ABSOLUTE_XINDEXED, 0xFD},
            {ABSOLUTE_YINDEXED, 0xF9},
            {ZEROPAGE, 0xE5},
            {ZEROPAGE_XINDEXED, 0xF5},
            {XINDEXED_INDIRECT, 0xE1},
            {INDIRECT_YINDEXED, 0xF1}
        }
    }, {"AND",
        {
            {IMMEDIATE, 0x29},
            {ABSOLUTE, 0x2D},
            {ABSOLUTE_XINDEXED, 0x3D},
            {ABSOLUTE_YINDEXED, 0x39},
            {ZEROPAGE, 0x25},
            {ZEROPAGE_XINDEXED, 0x35},
            {XINDEXED_INDIRECT, 0x21},
            {INDIRECT_YINDEXED, 0x31}
        }
    }, {"EOR",
        {
            {IMMEDIATE, 0x49},
            {ABSOLUTE, 0x4D},
            {ABSOLUTE_XINDEXED, 0x5D},
            {ABSOLUTE_YINDEXED, 0x59},
            {ZEROPAGE, 0x45},
            {ZEROPAGE_XINDEXED, 0x55},
            {XINDEXED_INDIRECT, 0x41},
            {INDIRECT_YINDEXED, 0x51}
        }
    }, {"ORA",
        {
            {IMMEDIATE, 0x09},
            {ABSOLUTE, 0x0D},
            {ABSOLUTE_XINDEXED, 0x1D},
            {ABSOLUTE_YINDEXED, 0x19},
            {ZEROPAGE, 0x05},
            {ZEROPAGE_XINDEXED, 0x15},
            {XINDEXED_INDIRECT, 0x01},
            {INDIRECT_YINDEXED, 0x11}
        }
    }, {"ASL",
        {
            {ACCUMULATOR, 0x0A},
            {ABSOLUTE, 0x0E},
            {ABSOLUTE_XINDEXED, 0x1E},
            {ZEROPAGE, 0x06},
            {ZEROPAGE_XINDEXED, 0x16}
        }
    }, {"LSR",
        {
            {ACCUMULATOR, 0x4A},
            {ABSOLUTE, 0x4E},
            {ABSOLUTE_XINDEXED, 0x5E},
            {ZEROPAGE, 0x46},
            {ZEROPAGE_XINDEXED, 0x56}
        }
    }, {"ROL",
        {
            {ACCUMULATOR, 0x2A},
            {ABSOLUTE, 0x2E},
            {ABSOLUTE_XINDEXED, 0x3E},
            {ZEROPAGE, 0x26},
            {ZEROPAGE_XINDEXED, 0x36}
        }
    }, {"ROR",
        {
            {ACCUMULATOR, 0x6A},
            {ABSOLUTE, 0x6E},
            {ABSOLUTE_XINDEXED, 0x7E},
            {ZEROPAGE, 0x66},
            {ZEROPAGE_XINDEXED, 0x76}
        }
    }, {"CLC",
        {
            {IMPLIED, 0x18}
        }
    }, {"CLD", 
        {
            {IMPLIED, 0xD8}
        }
    }, {"CLI",
        {
            {IMPLIED, 0x58}
        }
    }, {"CLV",
        {
            {IMPLIED, 0xB8}
        }
    }, {"SEC",
        {
            {IMPLIED, 0x38}
        }
    }, {"SED",
        {
            {IMPLIED, 0xF8}
        }
    }, {"SEI",
        {
            {IMPLIED, 0x78}
        }
    }, {"CMP",
        {
            {IMMEDIATE, 0xC9},
            {ABSOLUTE, 0xCD},
            {ABSOLUTE_XINDEXED, 0xDD},
            {ABSOLUTE_YINDEXED, 0xD9},
            {ZEROPAGE, 0xC5},
            {ZEROPAGE_XINDEXED, 0xD5},
            {XINDEXED_INDIRECT, 0xC1},
            {INDIRECT_YINDEXED, 0xD1}
        }
    }, {"CPX",
        {
            {IMMEDIATE, 0xE0},
            {ABSOLUTE, 0xEC},
            {ZEROPAGE, 0xE4}
        }
    }, {"CPY",
        {
            {IMMEDIATE, 0xC0},
            {ABSOLUTE, 0xCC},
            {ZEROPAGE, 0xC4}
        }
    }, {"BCC",
        {
            {RELATIVE, 0x90}
        }
    }, {"BCS",
        {
            {RELATIVE, 0xB0}
        }
    }, {"BEQ",
        {
            {RELATIVE, 0xF0}
        }
    }, {"BPL",
        {
            {RELATIVE, 0x10}
        }
    }, {"BMI",
        {
            {RELATIVE, 0x30}
        }
    }, {"BNE",
        {
            {RELATIVE, 0xD0}
        }
    }, {"BVC", 
        {
            {RELATIVE, 0x50}
        }
    }, {"BVS",
        {
            {RELATIVE, 0x70}
        }
    }, {"JMP",
        {
            {ABSOLUTE, 0x4C},
            {INDIRECT, 0x6C}
        }
    }, {"JSR",
        {
            {ABSOLUTE, 0x20}
        }
    }, {"RTS",
        {
            {IMPLIED, 0x60}
        }
    }, {"BRK",
        {
            {IMPLIED, 0x00}
        }
    }, {"RTI",
        {
            {IMPLIED, 0x40}
        }
    }, {"BIT",
        {
            {ABSOLUTE, 0x2C},
            {ZEROPAGE, 0x24}
        }
    }
};

/**
 * Determines if the given processor instruction is valid for the given addressing mode.
 * 
 * @param instruction The processor instruction to check.
 * @param addressingMode The addressing mode to check.
 * @return True if the processor instruction is valid for the given addressing mode, false otherwise.
 */
static bool validInstruction(std::string& instruction, AddressingMode addressingMode) {
    if (instructionMap.count(instruction) == 0) {
        return false;
    }
    
    return instructionMap.at(instruction).count(addressingMode) > 0;
}


/**
 * Extracts the value of a string operand.
 * 
 * @param operand The operand to extract the value from.
 * @param allowExpressions Whether or not to allow expression evaluation in the operand.
 * @return The value of the string operand.
 */
static u64 parseValue(std::string value, bool allowNonConst = true) {
	if (value.empty()) {
		error(INTERNAL_ERROR, std::stringstream() << "Cannot parse value from empty string");
	}

	// check if the value is a decimal number
	if (std::regex_match(value, std::regex("[1-9][0-9]+"))) {
		return std::stoull(value, nullptr, 10);
	}

	// check if the value is a hexadecimal number
	if (std::regex_match(value, std::regex("$[0-9a-fA-F]+"))) {
		return std::stoull(value.substr(1), nullptr, 16);
	}

	// check if the value is a binary number
	if (std::regex_match(value, std::regex("%[01]+"))) {
		return std::stoull(value.substr(1), nullptr, 2);
	}

	// check if the value is an octal number
	if (std::regex_match(value, std::regex("0[0-7]+"))) {
		return std::stoull(value.substr(1), nullptr, 8);
	}

	// else, this is an expression, evaluate the expression. TODO:
	



	return 0;
}


/**
 * Gets the addressing mode for the given processor instruction and operand.
 * 
 * @param instruction The processor instruction to get the addressing mode for.
 * @param operand The operand to get the addressing mode for.
 * @return The addressing mode for the given processor instruction and operand.
 */
static AddressingMode getAddressingMode(std::string instruction, std::string operand) {
    if (operand.empty()) {
		if (validInstruction(instruction, IMPLIED))
        	return IMPLIED;
		if (validInstruction(instruction, ACCUMULATOR))
			return ACCUMULATOR;
		return NO_ADDRESSING_MODE;
    }

	// check for immediate addressing mode
	// must start with '#'
	if (std::regex_match(operand, std::regex("#.+"))) {
		if (validInstruction(instruction, IMMEDIATE)) {
			return IMMEDIATE;
		}
		return NO_ADDRESSING_MODE;
	}
	

	// check for indirect addressing mode
	// must start with '(' and end with ')'
	if (std::regex_match(operand, std::regex("\\(.+\\)"))) {
		if (validInstruction(instruction, INDIRECT)) {
			return INDIRECT;
		}
		return NO_ADDRESSING_MODE;
	}

	// check for x indexed indirect addressing mode
	// must start with '(' and end with ',x)'
	if (std::regex_match(operand, std::regex("\\(.+,x\\)"))) {
		if (validInstruction(instruction, XINDEXED_INDIRECT)) {
			return XINDEXED_INDIRECT;
		}
		return NO_ADDRESSING_MODE;
	}

	// check for indirect y indexed addressing mode
	// must start with '(' and end with '),y'
	if (std::regex_match(operand, std::regex("\\(.+\\),y"))) {
		if (validInstruction(instruction, INDIRECT_YINDEXED)) {
			return INDIRECT_YINDEXED;
		}
		return NO_ADDRESSING_MODE;
	}

	// check for zeropage or absolute x or y indexed addressing mode
	// must end with ',x' or ',y
	if (std::regex_match(operand, std::regex(".+,x")) || std::regex_match(operand, std::regex(".+,y"))) {
		// check if we can extract a number from the operand,
		// otherwise it is by default absolute
		std::string operandWithoutIndex = operand.substr(0, operand.size() - 2);
		if (isNumber(operandWithoutIndex)) {
			// evaluate the number to ensure it is <= 0xFFFF (the zero page boundary)
			u32 value = parseValue(operandWithoutIndex, false);
			if (value <= 0xFFFF) {
				if (operand.back() == 'x' && validInstruction(instruction, ZEROPAGE_XINDEXED)) {
					return ZEROPAGE_XINDEXED;
				} else if (operand.back() == 'y' && validInstruction(instruction, ZEROPAGE_YINDEXED)) {
					return ZEROPAGE_YINDEXED;
				}
			}
		}

		// if the operand was an expression, there is a chance the value could be out of bounds
		// but that check will happen when we begin assembling.
		if (operand.back() == 'x' && validInstruction(instruction, ABSOLUTE_XINDEXED)) {
			return ABSOLUTE_XINDEXED;
		} else if (operand.back() == 'y' && validInstruction(instruction, ABSOLUTE_YINDEXED)) {
			return ABSOLUTE_YINDEXED;
		}
		return NO_ADDRESSING_MODE;
	}

	// check for zeropage, absolute, or relative addressing mode
	if (isNumber(operand)) {
		// evaluate the number to ensure it is <= 0xFFFF (the zero page boundary)
		u32 value = parseValue(operand, false);
		if (value <= 0xFFFF && validInstruction(instruction, ZEROPAGE)) {
			return ZEROPAGE;
		}
	} else if (validInstruction(instruction, RELATIVE)) {
		return RELATIVE;
	} else if (validInstruction(instruction, ABSOLUTE)) {
		return ABSOLUTE;
	}

	return NO_ADDRESSING_MODE;
}


/**
 * Gets the opcode for the given processor instruction and addressing mode.
 * 
 * @param instruction The processor instruction to get the opcode for.
 * @param addressingMode The addressing mode to get the opcode for.
 * 
 * @return The opcode for the given processor instruction and addressing mode.
 */
static u8 getProcessorInstructionOpcode(std::string& instruction, AddressingMode addressingMode) {
    return instructionMap.at(instruction).at(addressingMode);
}


/**
 * Check to make sure the filename is valid
 * 
 * @param filename The filename to check for validity
 * @return true if the filename is valid, false otherwise
 */
static bool isValidFilename(std::string filename) {
    return filename.size() > 0 && std::regex_match(filename, std::regex(validFileNameRegex));
}

/**
 * Check to make sure the token name is valid
 * 
 * @param tokenname The token name to check for validity
 * @return true if the token name is valid, false otherwise
 */
static bool isValidLabelName(std::string tokenname) {
	return tokenname.size() > 0 && std::regex_match(tokenname, std::regex(validLabelNameRegex));
}

/**
 * Determines whether the provided token is a string operand
 * 
 * @param token The token to check
 * @return true if the token is a string operand, false otherwise
 */
static bool isStringToken(std::string stringToken) {
    return stringToken.size() >= 2 && stringToken[0] == '"' && stringToken[stringToken.size() - 1] == '"';
}

static std::string getStringToken(std::string stringToken) {
	if (!isStringToken(stringToken)) {
		error(INVALID_TOKEN_ERROR, std::stringstream() << "Token is not a string token: " << stringToken);
	}

	return stringToken.substr(1, stringToken.size() - 2);
}

#endif // ASSEMBLER_H