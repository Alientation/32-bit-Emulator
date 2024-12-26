#define AEMU_ONLY_CRITICAL_LOG

#include "emulator32bit/emulator32bit.h"
#include "util/logger.h"

#include <iostream>

#define UNUSED(x) (void)(x)

void Emulator32bit::_emu_print()
{
	print();
}

void Emulator32bit::_emu_printr(byte reg_id)
{
	printf("REG: %d = %x\n", reg_id, read_reg(reg_id));
}

void Emulator32bit::_emu_printm(word mem_addr, byte size, bool little_endian)
{
	word val = 0;
	if (little_endian) {
		for (byte i = 0; i < size; i++) {
			val <<= 8;
			val += system_bus.read_byte(mem_addr + i);
		}
	} else {
		for (int i = size - 1; i >= 0; i--) {
			val <<= 8;
			val += system_bus.read_byte(mem_addr + i);
		}
	}

	printf("MEM: %x = %.2x", mem_addr, val);
}

void Emulator32bit::_emu_printp()
{
	printf("PSTATE: N=%lli,Z=%lli,C=%lli,V=%lli", test_bit(_pstate, N_FLAG), test_bit(_pstate, Z_FLAG),
		   test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
}

void Emulator32bit::_emu_assertr(byte reg_id, word min_value, word max_value) {
	word val = read_reg(reg_id);

	if (val >= min_value && val <= max_value) {

	} else {
		throw Exception(FAILED_ASSERT, "Failed system call assertion. Expected register " +
				std::to_string(reg_id) + " to contain a value between " +
				std::to_string(min_value) + " and " + std::to_string(max_value) +
				" but it contains " + std::to_string(val) + ".");
	}
}

void Emulator32bit::_emu_assertm(word mem_addr, byte size, bool little_endian, word min_value,
								 word max_value)
{
	word val = 0;
	if (little_endian) {
		for (byte i = 0; i < size; i++) {
			val <<= 8;
			val += system_bus.read_byte(mem_addr + i);
		}
	} else {
		for (int i = size - 1; i >= 0; i--) {
			val <<= 8;
			val += system_bus.read_byte(mem_addr + i);
		}
	}

	if (val < min_value || val > max_value) {
		throw Exception(FAILED_ASSERT, "Expected value at memory address " + std::to_string(mem_addr) +
				" to be between " + std::to_string(min_value) + " and " +
				std::to_string(max_value) + ". Got " + std::to_string(val) + ".");
	}
}

void Emulator32bit::_emu_assertp(byte p_state_id, bool expected_value)
{
	bool val = test_bit(_pstate, p_state_id);

	if (val != expected_value) {
		throw Exception(FAILED_ASSERT, "Failed system call assertion. Expected PSTATE " +
				std::to_string(p_state_id) + " to be " + std::to_string(expected_value) +
				". Got " + std::to_string(val) + ".");
	}
}

void Emulator32bit::_emu_log(word str)
{
	std::string msg;
	while (system_bus.read_byte(str) != '\0') {
		msg += (char) system_bus.read_byte(str);
		str++;
	}

	std::cout << msg << "\n";
}

// todo, raise interrupt so kernel can handle
void Emulator32bit::_emu_err(word err)
{
	std::string msg;
	while (system_bus.read_byte(err) != '\0') {
		msg += (char) system_bus.read_byte(err);
		err++;
	}

	std::cerr << msg << "\n";
}

/**
 * @brief					System Calls
 * 							https://chromium.googlesource.com/chromiumos/docs/+/master/constants/syscalls.md#arm64-64_bit
 * 							Would prefer to, in the future, implement these system calls directly in the kernel rather than abstracting it away
 * 							here. For now this would be fine until a higher level language is implemented for basm
 *
 * 							In future, we need a vector table that contains various jump instructions to handle various exceptions.
 * 							The kernel's vector table needs to be loaded into a fixed address location, which means we need a linker script
 * 							to specify memory layout of an executable. This also means the executable file format would have to slightly differ
 * 							and contain information about the physical/virtual address locations. We also would want to be able to access memory directly
 * 							instead of using virtual memory. We could store that information in the PSTATE variable of the processor.
 *
 * 							File management would be simulated through creating a large file to represent a hard drive (something along the lines of ~16 MiB)
 * ______________________________________________________________________________________________________________________________________________________________________________________________
 * | ID |		NAME	   |		arg x0		   |		arg x1		   |		arg x2		   |		arg x3			   |				arg x4				   |		arg x5			|
 * |____|__________________|_______________________|_______________________|_______________________|___________________________|_______________________________________|________________________|
 * |
 * |======================= Emulator Specific =======================
 **|1000: emu_print			-						-						-						-							-										-
 * |
 * |	prints emulator state to console
 * |
 **|1001: emu_printr		byte reg_id				-						-						-							-										-
 * |
 * |	prints a register to console
 * |
 **|1002: emu_printm		word mem_addr			byte size				bool little_endian		-							-										-
 * |
 * |	prints a specific value in memory to console
 * |
 **|1003: emu_printp		-						-						-						-							-										-
 * |
 * |	prints the pstate of the processor
 * |
 **|1010: emu_assertr		byte reg_id				word min_value			word max_value			-							-										-
 * |
 * |	halts execution if reg val is not within bounds
 * |
 **|1011: emu_assertm		word mem_addr			byte size				bool little_endian		word min_value				word max_value							-
 * |
 * |	halts execution if mem val is not within bounds
 * |
 **|1012: emu_assertp		byte p_state_id			bool expected_val		-						-							-										-
 * |
 * |	halts execution if the specified p_state is not the expected val
 * |
 **|1020: emu_log			char* str				-						-						-							-										-
 * |
 * |	prints message to console
 * |
 **|1021: emu_error			char* err				-						-						-							-										-
 * |
 * |	prints error to console and halts program
 * |
 * |
 * |
 * |======================= I/O Operations ==========================
 * |
 **|0000: io_setup			unsigned nr_reqs		aio_context_t *ctx
 * |
 * | 	creates the context information for the I/O operation with space for #nr requests
 * |
 **|0001: io_destroy			aio_context_t ctx
 * |
 * | 	invalidates the previously created context information
 * |
 **|0002: io_submit			aio_context_t			long					struct iocb * *
 * |
 * | 	with the file descriptor (some unique number that identifies a specific file), begins an operation
 * |
 **|0003: io_cancel			aio_context_t ctx_id	struct iocb *iocb		struct io_event *result
 * |
 * |	cancels a specific I/O operation
 * |
 **|0004: io_getevents		aio_context_t ctx_id	long min_nr				long nr					struct io_event *events		struct __kernel_timespec *timeout
 * |
 * | 	waits for when a specific I/O operation finishes or timesout
 * |
 * |
 * |
 * |======================= File Operations =========================
 **|0005: setxattr			const char *path		const char *name		const void *value		size_t size					int flags								-
 * |
 * |
 * |
 **|0006: lsetxattr			const char *path		const char *name		const void *value		size_t size					int flags								-
 * |
 * |
 * |
 **|0007: fsetxattr			int fd					const char *name		const void *value		size_t size					int flags								-
 * |
 * |
 * |
 **|0008: getxattr			const char *path		const char *name		void *value				size_t size					-										-
 * |
 * |
 * |
 **|0009: lgetxattr			const char *path		const char *name		void *value				size_t size					-										-
 * |
 * |
 * |
 **|0010: fgetxattr			int fd					const char *name		void *value				size_t size					-										-
 * |
 * |
 * |
 **|0011: listxattr			const char *path		char *list				size_t size				-							-										-
 * |
 * |
 * |
 **|0012: llistxattr		const char *path		char *list				size_t size				-							-										-
 * |
 * |
 * |
 **|0013: flistxattr		int fd					char *list				size_t size				-							-										-
 * |
 * |
 * |
 **|0014: removexattr		const char *path		const char *name		-						-							-										-
 * |
 * |
 * |
 **|0015: lremovexattr		const char *path		const char *name		-						-							-										-
 * |
 * |
 * |
 **|0016: fremovexattr		int fd					const char *name		-						-							-										-
 * |
 * |
 * L____________________________________________________________________________________________________________________________________________________________________________________________|
 * @param instr
 * @param exception
 */
void Emulator32bit::_swi(word instr)
{
	byte cond = bitfield_u32(instr, 22, 4);
	DEBUG_SS(std::stringstream() << "swi " << std::to_string(cond));

	if (!check_cond(_pstate, cond)) {
		return;
	}

	// software interrupts.. perfect to add functionality to this like console print,
	// file operations, ports, etc
	word id = read_reg(NR);
	word arg0 = read_reg(0);
	word arg1 = read_reg(1);
	word arg2 = read_reg(2);
	word arg3 = read_reg(3);
	word arg4 = read_reg(4);
	word arg5 = read_reg(5);
	UNUSED(arg5); // temporary
	switch(id) {
		case 1000:
			_emu_print();
			break;
		case 1001:
			_emu_printr(arg0);
			break;
		case 1002:
			_emu_printm(arg0, arg1, arg2);
			break;
		case 1003:
			_emu_printp();
			break;

		case 1010:
			_emu_assertr(arg0, arg1, arg2);
			break;
		case 1011:
			_emu_assertm(arg0, arg1, arg2, arg3, arg4);
			break;
		case 1012:
			_emu_assertp(arg0, arg1);
			break;
		default:
			throw Exception(BAD_INSTR, "Invalid syscall number " + std::to_string(id));
	}
}