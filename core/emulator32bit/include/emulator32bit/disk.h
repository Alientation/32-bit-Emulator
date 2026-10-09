#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/fbl.h"
#include "emulator32bit/memory.h"
#include "util/file.h"

#include <fstream>
#include <unordered_map>
#include <vector>

/**
 * @brief             The number of pages the cache of a @ref Disk holds. A power of 2, a page is
 *                     in the cache slot of its number modulo this.
 */
inline constexpr word kDiskCacheSize = 32;
static_assert((kDiskCacheSize & (kDiskCacheSize - 1)) == 0, "The cache size is a power of 2");

/**
 * @brief             Simulates disk memory with a file to maintain data across utilizations.
 *
 * @details         Disk memory is stored in terms of pages. Each page is of size
 *                     @ref kPageSize bytes (currently 4096 bytes). Contains a cache that can hold
 *                     @ref kDiskCacheSize pages and features optimized page eviction like
 *                     avoiding writing back clean pages. Disk memory is organized by a memory manager
 *                     that maintains a free block list of page blocks that are not in use. This means
 *                     that proper usage of this class requires that whenever a disk page is not used
 *                     anymore, it needs to be returned back.
 *
 *                     A disk without a file is a @ref MockDisk.
 *
 * @todo             TODO: Allow allocating multiple pages at times. Also add helper to check if the
 *                     disk can allocate such an amount.
 */
class Disk : public BaseMemory
{
  public:
    /**
         * @brief             Construct a new Disk object.
         *
         * @param diskfile     the file the disk memory is saved in.
         * @param npages     the number of pages the disk should have.
         */
    Disk(File diskfile, word npages, word lo_page);
    virtual ~Disk();

    Disk(const Disk &) = delete;
    Disk &operator=(const Disk &) = delete;

    class DiskReadException : public std::exception
    {
      private:
        std::string message;

      public:
        DiskReadException(const std::string &msg);

        const char *what() const noexcept override;
    };

    class DiskWriteException : public std::exception
    {
      private:
        std::string message;

      public:
        DiskWriteException(const std::string &msg);

        const char *what() const noexcept override;
    };

    /**
         * @brief             Get a free disk page that is not currently in use.
         *
         *                     The disk page is not guaranteed to be zero initialized.
         *
         * @todo             TODO: The returned exception should be a disk exception to wrap the
         *                     internal implementation (free block list) and to limit/make more
         *                     specific what exceptions can actually occur as a result of this request.
         *
         * @return             Address of the free page (the upper bits of a full 32 bit address).
         */
    virtual word get_free_page();

    /**
         * @brief             Returns a disk page back into the free page list.
         *
         * @todo             TODO: The returned exception should be a disk exception to wrap the
         *                     internal implementation (free block list) and to limit/make more
         *                     specific what exceptions can actually occur as a result of this request.
         *
         * @param page        Page address of the returned page.
         * @param exception    Exception thrown if the return fails TODO: specify what exceptions can
         *                     occur.
         */
    virtual void return_page(word page);

    /**
         * @brief             Returns all disk pages back to the free page list.
         *
         *                     This will essentially wipe the disk fully, though the contents of the
         *                     pages that were in disk will still remain in disk memory.
         */
    virtual void return_all_pages();

    /**
         * @brief             Return all pages in a specific range.
         *
         *                     Pages that are in the range page_lo..page_hi, inclusive, but are already
         *                     a free page will NOT throw an exception when attempting to return back
         *                     to free list.
         *
         * @todo             TODO: The returned exception should be a disk exception to wrap the
         *                     internal implementation (free block list) and to limit/make more
         *                     specific what exceptions can actually occur as a result of this request.
         *
         * @param page_lo     Lowest page address to return back to disk.
         * @param page_hi     Highest page address to return back to disk.
         */
    virtual void return_pages(word page_lo, word page_hi);

    /**
         * @brief             Reads a disk page.
         *
         *                     Page data is returned as a vector of @ref PAGE_SIZE bytes,
         *                     the first element corresponding to the byte at the start of the page.
         *
         * @param page         Disk page address to read.
         * @param exception ReadException if the read fails. TODO: specify what exceptions can
         *                     occur.
         * @return             Page data corresponding to the page address.
         */
    std::vector<byte> read_page(word page);

    /**
         * @brief             Reads a disk page into a buffer, which the swapping uses so that it
         *                     does not allocate a vector for every page.
         *
         * @param page         Disk page address to read.
         * @param out          Room for kPageSize bytes.
         */
    virtual void read_page(word page, byte *out);

    /**
         * @brief             Reads a byte from disk.
         *
         * @param address     Full address of byte to read.
         * @param exception ReadException if the read fails. TODO: specify what exceptions can
         *                     occur.
         * @return             Byte located at the address in disk.
         */
    byte read_byte(word address) override;

    /**
         * @brief             Reads a half word (2 bytes) from disk.
         *
         * @param address     Full address of hword to read.
         * @param exception ReadException if the read fails. TODO: specify what exceptions can
         *                     occur.
         * @return             Half word located at the address in disk in little endian format.
         */
    hword read_hword(word address) override;

    /**
         * @brief             Reads a word (4 bytes) from disk.
         *
         * @param address     Full address of word to read.
         * @param exception ReadException if the read fails. TODO: specify what exceptions can
         *                     occur.
         * @return             Word located at the address in disk in little endian format.
         */
    word read_word(word address) override;

    /**
         * @brief             Write a page to disk.
         *
         *                     Page data is given as a vector of @ref PAGE_SIZE bytes, where the first
         *                     byte is written to the start of the page on disk.
         *
         * @param page         Page address to write to.
         * @param exception WriteException if the write fails. TODO: specify what exceptions can
         *                     occur.
         */
    void write_page(word page, const std::vector<byte> &data);

    /**
         * @brief             Writes kPageSize bytes to a disk page (the vector version checks the
         *                     size and calls this).
         *
         * @param page         Page address to write to.
         * @param data         kPageSize bytes.
         */
    virtual void write_page(word page, const byte *data);

    /**
         * @brief             Writes a byte to disk.
         *
         * @param address     Full address of the location to write the byte to.
         * @param data         Byte to write.
         * @param exception WriteException if the write fails. TODO: specify what exceptions can
         *                     occur.
         */
    void write_byte(word address, byte data) override;

    /**
         * @brief             Writes a half word (2 bytes) to disk in little endian format.
         *
         * @param address     Full address of the location to write the half word to.
         * @param data         Half word to write.
         * @param exception WriteException if the write fails. TODO: specify what exceptions can
         *                     occur.
         */
    void write_hword(word address, hword data) override;

    /**
         * @brief             Writes a word (4 bytes) to disk in little endian format.
         *
         * @param address     Full address of the location to write the word to.
         * @param data         Word to write.
         * @param exception WriteException if the write fails. TODO: specify what exceptions can
         *                     occur.
         */
    void write_word(word address, word data) override;

    /**
         * @brief             Saves the simulated disk to file.
         *
         *                     Saves both the disk file and free page management to file.
         */
    virtual void save();

  protected:
    /// A disk with no file and no addresses, for a disk that keeps its pages some other way.
    Disk();

  private:
    /**
         * @brief             Disk page located in cache
         */
    struct CachePage
    {
        /// Disk page address that the cache page refers to
        word page;

        /// Data stored in the cache page
        byte data[kPageSize];

        /// Whether the data has been written to after bringing into cache
        bool dirty = false;

        /// Whether the cache page refers to an actual disk page or is an empty page
        bool valid = false;
    };

    File m_diskfile;                ///< Where the contents of disk memory are stored at
    File m_diskfile_manager;        ///< Where the disk memory manager data is stored at
    std::fstream m_stream;          ///< The disk file, open for as long as the disk exists
    std::streamsize m_npages = 0;   ///< Number of pages the disk memory contains
    std::vector<CachePage> m_cache; ///< Disk cache for read/write optimization

    FreeBlockList m_free_list;      ///< Disk manager, which pages are free to use

    /**
         * @brief             Reads a specified size little endian value from disk.
         *
         *                     Interfaced with by the read byte/hword/word public functions. Note,
         *                     reading anything more than 8 bytes will not produce useful results.
         *
         * @todo             TODO: Have a separate method that reads a stream of bytes so we are not
         *                     limited by primitive data types.
         *
         * @param address     Address to read from.
         * @param n_bytes     Number of bytes to read.
         * @param exception ReadException thrown if read fails. TODO: Specify what
         *                     exceptions can occur.
         * @return             value read.
         */
    dword read_val(word address, int n_bytes);

    /**
         * @brief             Writes a little endian value of specified size to disk.
         *
         *                     Interfaced with by the write byte/hword/word public functions. Note,
         *                     writing anything more than 8 bytes will not be useful.
         *
         * @todo             TODO: Have a separate method that writes a stream of bytes so we are no
         *                     limited by primitive data types.
         *
         * @param address     Address to write to.
         * @param val         Value to write.
         * @param n_bytes     Number of bytes to write.
         * @param exception WriteException thrown if write fails. TODO: Specify what
         *                     exceptions can occur.
         */
    void write_val(word address, dword val, int n_bytes);

    /**
         * @brief             Accesses a cache page.
         *
         *                     Fetches the corresponding cache page of the disk page requested,
         *                     evicting a page from cache to make room when necessary.
         *
         * @param addr        Page to fetch.
         * @param load         Whether the contents of the page are read from the disk file. Not
         *                     needed when the whole page is about to be overwritten.
         * @return             Reference to the cache page.
         * @throws             DiskReadException if the page is not on the disk.
         */
    CachePage &get_cpage(word addr, bool load = true);

    /**
         * @brief             Writes a cache page to disk.
         *
         *                     Writes to disk even if the cache page is not valid or dirty.
         *
         * @param cpage     Reference to the cache page to write.
         */
    void write_cpage(CachePage &cpage);

    /**
         * @brief            Reads a cache page from disk.
         *
         * @param cpage     Reference to cache page to read to.
         */
    void read_cpage(CachePage &cpage);

    /**
         * @brief             Reads and sets up the simulated disk from save files.
         */
    void read_disk_files();

    /**
         * @brief             Reads and sets up the disk free page list from save file.
         * @note             Called from @ref Disk::read_disk_files()
         */
    void read_disk_manager_file();
};

/**
 * @brief             A @ref Disk in memory, without a file. It keeps the pages that are written
 *                    to it for as long as it exists, and has as many pages as are needed. It is
 *                    only a store of pages, it has no addresses (the byte, hword and word accesses
 *                    do nothing).
 */
class MockDisk : public Disk
{
  public:
    MockDisk();

    word get_free_page() override;
    void return_page(word page) override;
    void return_all_pages() override;
    void return_pages(word p_addr_lo, word p_addr_hi) override;

    using Disk::read_page;
    using Disk::write_page;
    void read_page(word page, byte *out) override;
    byte read_byte(word address) override;
    hword read_hword(word addressn) override;
    word read_word(word address) override;

    void write_page(word page, const byte *data) override;
    void write_byte(word address, byte data) override;
    void write_hword(word address, hword data) override;
    void write_word(word address, word data) override;

    void save() override;

  private:
    /// The contents of the pages, by page number. A page that has none (an empty vector, or a
    /// number past the end) is zeros.
    std::vector<std::vector<byte>> m_pages;

    /// Contents that were returned, so that writing a page does not allocate one.
    std::vector<std::vector<byte>> m_spare;

    /// Pages that were returned, and are handed out again before new ones.
    std::vector<word> m_returned;

    /// Whether a page that was handed out is in m_returned (by page number).
    std::vector<bool> m_is_returned;

    /// The next page that was never handed out.
    word m_next_page = 0;
};
