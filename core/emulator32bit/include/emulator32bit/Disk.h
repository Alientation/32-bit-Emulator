#pragma once
#ifndef DISK_H
#define DISK_H

#include "emulator32bit/Emulator32bitUtil.h"
#include "util/File.h"

#include <fstream>

#define DISK_CACHE_PSIZE 5
#define DISK_CACHE_SIZE (1 << DISK_CACHE_PSIZE)
#define DISK_PAGE_PSIZE 12
#define DISK_PAGE_SIZE (1 << DISK_PAGE_PSIZE)

class Disk {
	public:
		Disk(File diskfile, std::streamsize npages = 128);
		~Disk();

		struct ReadException {
			enum class Type {
				AOK, INVALID_ADDRESS,
			} type = Type::AOK;
			word address = 0;
		};

		struct WriteException {
			enum class Type {
				AOK, INVALID_ADDRESS,
			} type = Type::AOK;
			word address = 0;
		};
		struct CachePage {
			word p_addr;
			byte data[DISK_PAGE_SIZE];
			bool dirty = false;
			bool valid = false;
			long long last_acc;
		};

		byte read_byte(word address, ReadException &exception);
		hword read_hword(word address, ReadException &exception);
		word read_word(word address, ReadException &exception);

		void write_byte(word address, byte data, WriteException &exception);
		void write_hword(word address, hword data, WriteException &exception);
		void write_word(word address, word data, WriteException &exception);

	private:
		File m_diskfile;
		std::streamsize m_npages;
		CachePage* m_cache;

		long long n_acc = 0;

		dword read_val(word address, int n_bytes, ReadException &exception);
		void write_val(word address, dword val, int n_bytes, WriteException &exception);

		CachePage& get_cpage(word p_addr);
		void write_cpage(CachePage& cpage);
		void read_cpage(CachePage& cpage, word p_addr);
		void write_all();
};

#endif /* DISK_H */