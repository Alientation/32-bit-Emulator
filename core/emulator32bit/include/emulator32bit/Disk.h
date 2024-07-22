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
		Disk(File diskfile, std::streamsize npages = 4096);
		virtual ~Disk();

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

		// todo read_page()
		virtual byte read_byte(word address, ReadException &exception);
		virtual hword read_hword(word address, ReadException &exception);
		virtual word read_word(word address, ReadException &exception);

		// todo write_page()
		virtual void write_byte(word address, byte data, WriteException &exception);
		virtual void write_hword(word address, hword data, WriteException &exception);
		virtual void write_word(word address, word data, WriteException &exception);

		virtual void write_all();
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
};

class MockDisk : public Disk {
	public:
		MockDisk();

		// todo read_page()
		byte read_byte(word address, ReadException &exception) override;
		hword read_hword(word address, ReadException &exception) override;
		word read_word(word address, ReadException &exception) override;

		// todo write_page()
		void write_byte(word address, byte data, WriteException &exception) override;
		void write_hword(word address, hword data, WriteException &exception) override;
		void write_word(word address, word data, WriteException &exception) override;

		void write_all() override;
};

#endif /* DISK_H */