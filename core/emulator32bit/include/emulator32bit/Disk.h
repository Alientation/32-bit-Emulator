#pragma once
#ifndef DISK_H
#define DISK_H

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/fbl.h"
#include "util/file.h"

#include <fstream>

/**
 * @def 			AEMU_DISK_CACHE_PSIZE
 * @brief 			The log base 2 of the @ref Disk cache size.
 *
 * @note 			This defines the number of bits necessary to represent a cache location.
 * 					It is used to calculate the total cache size.
 * @see 			AEMU_DISK_CACHE_SIZE
 */
#define AEMU_DISK_CACHE_PSIZE 5

/**
 * @def 			AEMU_DISK_CACHE_SIZE
 * @brief 			The size of @ref Disk cache.
 *
 * @note 			This is calculated as 2 ^ @ref AEMU_DISK_CACHE_PSIZE
 */
#define AEMU_DISK_CACHE_SIZE (1 << AEMU_DISK_CACHE_PSIZE)

/**
 * @brief 			Simulates disk memory with a file to maintain data across utilizations.
 *
 * @details 		Disk memory is stored in terms of pages. Each page is of size
 * 					@ref AEMU_PAGE_SIZE bytes (currently 4096 bytes). Contains a cache that can hold
 * 					@ref AEMU_DISK_CACHE_SIZE pages and features optimized page eviction like
 * 					avoiding writing back clean pages. Disk memory is organized by a memory manager
 * 					that maintains a free block list of page blocks that are not in use. This means
 * 					that proper usage of this class requires that whenever a disk page is not used
 * 					anymore, it needs to be returned back.
 *
 * @todo 			TODO: Allow allocating multiple pages at times. Also add helper to check if the
 * 					disk can allocate such an amount.
 */
class Disk
{
	public:
		/**
		 * @brief 			Construct a new Disk object.
		 *
		 * @param diskfile 	the file the disk memory is saved in.
		 * @param npages 	the number of pages the disk should have.
		 */
		Disk(File diskfile, std::streamsize npages = 4096);
		virtual ~Disk();

		/**
		 * @brief 			Exceptions that occur when reading from disk.
		 *
		 * @note 			If no exception occured, the only valid information would be the type.
		 */
		struct ReadException {
			/**
			 * @brief 			Type of read exception that occured.
			 */
			enum class Type {
				AOK,							///< Good state
				INVALID_ADDRESS,				///< Invalid read address (out of bounds)
			};

			Type type = Type::AOK;				///< Type of read exception, defaults to AOK
			word address = 0;					///< Read address
		};

		/**
		 * @brief 			Exceptions that occur when writing to disk.
		 *
		 * @note 			If no exception occured, the only valid information would be the type.
		 */
		struct WriteException {
			/**
			 * @brief 			Type of write exception that occured.
			 */
			enum class Type {
				AOK,							///< Good state
				INVALID_ADDRESS,				///< Invalid write address (out of bounds)
				INVALID_PAGEDATA_SIZE,			///< Passed in page data was not of size PAGE_SIZE
			};

			Type type = Type::AOK;				///< Type of write exception, defaults to AOK
			word address = 0;					///< Write address
			word data_length = 0;				///< Size of data to write to disk
		};

		/**
		 * @brief 			Get a free disk page that is not currently in use.
		 *
		 * 					The disk page is not guaranteed to be zero initialized.
		 *
		 * @todo 			TODO: The returned exception should be a disk exception to wrap the
		 * 					internal implementation (free block list) and to limit/make more
		 * 					specific what exceptions can actually occur as a result of this request.
		 *
		 * @param exception Exception thrown if the request fails TODO: specify what
		 * 					exceptions can occur.
		 * @return 			Address of the free page (the upper bits of a full 32 bit address).
		 */
		virtual word get_free_page(FreeBlockList::Exception& exception);

		/**
		 * @brief 			Returns a disk page back into the free page list.
		 *
		 * @todo 			TODO: The returned exception should be a disk exception to wrap the
		 * 					internal implementation (free block list) and to limit/make more
		 * 					specific what exceptions can actually occur as a result of this request.
		 *
		 * @param page		Page address of the returned page.
		 * @param exception	Exception thrown if the return fails TODO: specify what exceptions can
		 * 					occur.
		 */
		virtual void return_page(word page, FreeBlockList::Exception& exception);

		/**
		 * @brief 			Returns all disk pages back to the free page list.
		 *
		 * 					This will essentially wipe the disk fully, though the contents of the
		 * 					pages that were in disk will still remain in disk memory.
		 */
		virtual void return_all_pages();

		/**
		 * @brief 			Return all pages in a specific range.
		 *
		 * 					Pages that are in the range page_lo..page_hi, inclusive, but are already
		 * 					a free page will NOT throw an exception when attempting to return back
		 * 					to free list.
		 *
		 * @todo 			TODO: The returned exception should be a disk exception to wrap the
		 * 					internal implementation (free block list) and to limit/make more
		 * 					specific what exceptions can actually occur as a result of this request.
		 *
		 * @param page_lo 	Lowest page address to return back to disk.
		 * @param page_hi 	Highest page address to return back to disk.
		 * @param exception Exception thrown if the return fails TODO: specify what exceptions can
		 * 					occur.
		 */
		virtual void return_pages(word page_lo, word page_hi, FreeBlockList::Exception& exception);

		/**
		 * @brief 			Reads a disk page.
		 *
		 * 					Page data is returned as a vector of @ref PAGE_SIZE bytes,
		 * 					the first element corresponding to the byte at the start of the page.
		 *
		 * @param page 		Disk page address to read.
		 * @param exception ReadException if the read fails. TODO: specify what exceptions can
		 * 					occur.
		 * @return 			Page data corresponding to the page address.
		 */
		virtual std::vector<byte> read_page(word page, ReadException& exception);

		/**
		 * @brief 			Reads a byte from disk.
		 *
		 * @param address 	Full address of byte to read.
		 * @param exception ReadException if the read fails. TODO: specify what exceptions can
		 * 					occur.
		 * @return 			Byte located at the address in disk.
		 */
		virtual byte read_byte(word address, ReadException& exception);

		/**
		 * @brief 			Reads a half word (2 bytes) from disk.
		 *
		 * @param address 	Full address of hword to read.
		 * @param exception ReadException if the read fails. TODO: specify what exceptions can
		 * 					occur.
		 * @return 			Half word located at the address in disk in little endian format.
		 */
		virtual hword read_hword(word address, ReadException& exception);

		/**
		 * @brief 			Reads a word (4 bytes) from disk.
		 *
		 * @param address 	Full address of word to read.
		 * @param exception ReadException if the read fails. TODO: specify what exceptions can
		 * 					occur.
		 * @return 			Word located at the address in disk in little endian format.
		 */
		virtual word read_word(word address, ReadException& exception);

		/**
		 * @brief 			Write a page to disk.
		 *
		 * 					Page data is given as a vector of @ref PAGE_SIZE bytes, where the first
		 * 					byte is written to the start of the page on disk.
		 *
		 * @param page 		Page address to write to.
		 * @param exception WriteException if the write fails. TODO: specify what exceptions can
		 * 					occur.
		 */
		virtual void write_page(word page, std::vector<byte>, WriteException& exception);

		/**
		 * @brief 			Writes a byte to disk.
		 *
		 * @param address 	Full address of the location to write the byte to.
		 * @param data 		Byte to write.
		 * @param exception WriteException if the write fails. TODO: specify what exceptions can
		 * 					occur.
		 */
		virtual void write_byte(word address, byte data, WriteException& exception);

		/**
		 * @brief 			Writes a half word (2 bytes) to disk in little endian format.
		 *
		 * @param address 	Full address of the location to write the half word to.
		 * @param data 		Half word to write.
		 * @param exception WriteException if the write fails. TODO: specify what exceptions can
		 * 					occur.
		 */
		virtual void write_hword(word address, hword data, WriteException& exception);

		/**
		 * @brief 			Writes a word (4 bytes) to disk in little endian format.
		 *
		 * @param address 	Full address of the location to write the word to.
		 * @param data 		Word to write.
		 * @param exception WriteException if the write fails. TODO: specify what exceptions can
		 * 					occur.
		 */
		virtual void write_word(word address, word data, WriteException& exception);

		/**
		 * @brief 			Saves the simulated disk to file.
		 *
		 * 					Saves both the disk file and free page management to file.
		 */
		virtual void save();
	private:
		/**
		 * @brief 			Disk page located in cache
		 */
		struct CachePage {
			/// Disk page address that the cache page refers to
			word page;

			/// Data stored in the cache page
			byte data[PAGE_SIZE];

			/// Whether the data has been written to after bringing into cache
			bool dirty = false;

			/// Whether the cache page refers to an actual disk page or is an empty page
			bool valid = false;

			/// Not yet used, but for caches with multi-page lines can use for page eviction
			long long last_acc;
		};

		File m_diskfile;						///< Where the contents of disk memory are stored at
		File m_diskfile_manager;				///< Where the disk memory manager data is stored at
		std::streamsize m_npages;				///< Number of pages the disk memory contains
		CachePage* m_cache;						///< Disk cache for read/write optimization

		long long n_acc = 0;					///< Used for LRU calculations, number of accesses

		FreeBlockList m_free_list;				///< Disk manager, which pages are free to use

		/**
		 * @brief 			Reads a specified size little endian value from disk.
		 *
		 * 					Interfaced with by the read byte/hword/word public functions. Note,
		 * 					reading anything more than 8 bytes will not produce useful results.
		 *
		 * @todo 			TODO: Have a separate method that reads a stream of bytes so we are not
		 * 					limited by primitive data types.
		 *
		 * @param address 	Address to read from.
		 * @param n_bytes 	Number of bytes to read.
		 * @param exception ReadException thrown if read fails. TODO: Specify what
		 * 					exceptions can occur.
		 * @return 			value read.
		 */
		dword read_val(word address, int n_bytes, ReadException &exception);

		/**
		 * @brief 			Writes a little endian value of specified size to disk.
		 *
		 * 					Interfaced with by the write byte/hword/word public functions. Note,
		 * 					writing anything more than 8 bytes will not be useful.
		 *
		 * @todo 			TODO: Have a separate method that writes a stream of bytes so we are no
		 * 					limited by primitive data types.
		 *
		 * @param address 	Address to write to.
		 * @param val 		Value to write.
		 * @param n_bytes 	Number of bytes to write.
		 * @param exception WriteException thrown if write fails. TODO: Specify what
		 * 					exceptions can occur.
		 */
		void write_val(word address, dword val, int n_bytes, WriteException &exception);

		/**
		 * @brief 			Accesses a cache page.
		 *
		 * 					Fetches the corresponding cache page of the disk page requested,
		 * 					evicting a page from cache to make room when necessary.
		 *
		 * @param addr		Address to fetch the page of.
		 * @return 			Reference to the cache page.
		 */
		CachePage& get_cpage(word addr);

		/**
		 * @brief 			Writes a cache page to disk.
		 *
		 * 					Writes to disk even if the cache page is not valid or dirty.
		 *
		 * @param cpage 	Reference to the cache page to write.
		 */
		void write_cpage(CachePage& cpage);

		/**
		 * @brief			Reads a cache page from disk.
		 *
		 * @param cpage 	Reference to cache page to read to.
		 * @param page 		Page address to read from to fill the cache page.
		 */
		void read_cpage(CachePage& cpage, word page);

		/**
		 * @brief 			Reads and sets up the simulated disk from save files.
		 */
		void read_disk_files();

		/**
		 * @brief 			Reads and sets up the disk free page list from save file.
		 * @note 			Called from @ref Disk::read_disk_files()
		 */
		void read_disk_manager_file();
};

/**
 * @brief 			Mocks @ref Disk but doesn't actually perform any operations
 */
class MockDisk : public Disk
{
	public:
		MockDisk();

		word get_free_page(FreeBlockList::Exception& exception) override;
		void return_page(word page, FreeBlockList::Exception& exception) override;
		void return_all_pages() override;
		void return_pages(word p_addr_lo, word p_addr_hi, FreeBlockList::Exception& exception) override;

		std::vector<byte> read_page(word page, ReadException& exception) override;
		byte read_byte(word address, ReadException &exception) override;
		hword read_hword(word address, ReadException &exception) override;
		word read_word(word address, ReadException &exception) override;

		void write_page(word page, std::vector<byte>, WriteException& exception) override;
		void write_byte(word address, byte data, WriteException &exception) override;
		void write_hword(word address, hword data, WriteException &exception) override;
		void write_word(word address, word data, WriteException &exception) override;

		void save() override;
};

#endif /* DISK_H */