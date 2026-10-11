#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/fbl.h"
#include "emulator32bit/memory.h"
#include "util/file.h"

#include <fstream>
#include <unordered_map>
#include <vector>

/// The number of pages the cache of a @ref Disk holds. A power of 2, a page is
/// in the cache slot of its number modulo this.
inline constexpr word kDiskCacheSize = 32;
static_assert((kDiskCacheSize & (kDiskCacheSize - 1)) == 0, "The cache size is a power of 2");

/// Simulates disk memory with a file to maintain data across utilizations.
///
/// Disk memory is stored in terms of pages. Each page is @ref kPageSize bytes (currently 4096
/// bytes). Contains a cache that can hold
/// @ref kDiskCacheSize pages and features optimized page eviction like
/// avoiding writing back clean pages. Disk memory is organized by a memory manager
/// that maintains a free block list of page blocks that are not in use. This means
/// that proper usage of this class requires that whenever a disk page is not used
/// anymore, it needs to be returned back.
///
/// A disk without a file is a @ref MockDisk.
///
/// @todo Allow allocating multiple pages at times. Also add helper to check if the
///     disk can allocate such an amount.
class Disk : public BaseMemory
{
  public:
    /// Constructs a disk, reading its contents from the file if it exists.
    ///
    /// @param diskfile The file the disk memory is saved in.
    /// @param npages The number of pages the disk should have.
    /// @param lo_page The page number of the first page, so the first address is
    ///     `lo_page << kNumPageOffsetBits`.
    Disk(const File &diskfile, word npages, word lo_page);
    virtual ~Disk();

    Disk(const Disk &) = delete;
    Disk &operator=(const Disk &) = delete;

    /// Thrown when a page cannot be read from the disk.
    class ReadError : public std::exception
    {
      private:
        std::string message;

      public:
        /// @param msg What is wrong.
        ReadError(const std::string &msg);

        /// @return The message.
        const char *what() const noexcept override;
    };

    /// Thrown when a page cannot be written to the disk.
    class WriteError : public std::exception
    {
      private:
        std::string message;

      public:
        /// @param msg What is wrong.
        WriteError(const std::string &msg);

        /// @return The message.
        const char *what() const noexcept override;
    };

    /// Takes a free page of the disk. The page is zeros, whatever it held before.
    ///
    /// @return The number of the page (the upper bits of a full 32 bit address).
    /// @throws FreeBlockList::Error if every page is in use.
    virtual word get_free_page();

    /// Gives a page back to the free pages.
    ///
    /// @param page The number of a page that get_free_page () gave.
    /// @throws FreeBlockList::Error if the page is not one that is in use.
    virtual void return_page(word page);

    /// Reads a disk page.
    ///
    /// @param page The number of the page.
    /// @return The kPageSize bytes of the page, the first one at the start of the page.
    /// @throws ReadError if the page is not on the disk.
    std::vector<byte> read_page(word page);

    /// Reads a disk page into a buffer, which the swapping uses so that it does not allocate a
    /// vector for every page.
    ///
    /// @param out Room for kPageSize bytes.
    /// @param page The number of the page.
    /// @throws ReadError if the page is not on the disk.
    virtual void read_page(byte *out, word page);

    /// Reads a byte from the disk.
    ///
    /// @param address The full address of the byte.
    /// @return The byte.
    /// @throws ReadError if the address is not on the disk.
    byte read_byte(word address) override;

    /// Reads a half word (2 bytes) from the disk, little endian.
    ///
    /// @param address The full address of the half word.
    /// @return The half word.
    /// @throws ReadError if the address is not on the disk.
    hword read_hword(word address) override;

    /// Reads a word (4 bytes) from the disk, little endian.
    ///
    /// @param address The full address of the word.
    /// @return The word.
    /// @throws ReadError if the address is not on the disk.
    word read_word(word address) override;

    /// Writes a page to the disk.
    ///
    /// @param page The number of the page.
    /// @param data The new contents: kPageSize bytes, the first one at the start of the page.
    /// @throws WriteError if `data` is not kPageSize bytes.
    /// @throws ReadError if the page is not on the disk.
    void write_page(word page, const std::vector<byte> &data);

    /// Writes kPageSize bytes to a disk page (the vector version checks the size and calls this).
    ///
    /// @param page The number of the page.
    /// @param data kPageSize bytes.
    /// @throws ReadError if the page is not on the disk.
    virtual void write_page(word page, const byte *data);

    /// Writes a byte to the disk.
    ///
    /// @param address The full address of the byte.
    /// @param data The byte to write.
    /// @throws ReadError if the address is not on the disk.
    void write_byte(word address, byte data) override;

    /// Writes a half word (2 bytes) to the disk, little endian.
    ///
    /// @param address The full address of the half word.
    /// @param data The half word to write.
    /// @throws ReadError if the address is not on the disk.
    void write_hword(word address, hword data) override;

    /// Writes a word (4 bytes) to the disk, little endian.
    ///
    /// @param address The full address of the word.
    /// @param data The word to write.
    /// @throws ReadError if the address is not on the disk.
    void write_word(word address, word data) override;

    /// Saves the simulated disk to file.
    ///
    /// Saves both the disk file and free page management to file.
    virtual void save();

  protected:
    /// A disk with no file and no addresses, for a disk that keeps its pages some other way.
    Disk();

  private:
    /// Disk page located in cache.
    struct CachePage
    {
        /// Disk page address that the cache page refers to.
        word page;

        /// Data stored in the cache page.
        byte data[kPageSize];

        /// Whether the data has been written to after bringing into cache.
        bool dirty = false;

        /// Whether the cache page refers to an actual disk page or is an empty page.
        bool valid = false;
    };

    File m_diskfile;                ///< Where the contents of disk memory are stored at.
    File m_diskfile_manager;        ///< Where the disk memory manager data is stored at.
    std::fstream m_stream;          ///< The disk file, open for as long as the disk exists.
    std::streamsize m_npages = 0;   ///< Number of pages the disk memory contains.
    std::vector<CachePage> m_cache; ///< Disk cache for read/write optimization.

    FreeBlockList m_free_list;      ///< Disk manager, which pages are free to use.

    /// Reads a specified size little endian value from disk.
    ///
    /// Interfaced with by the read byte/hword/word public functions. Note,
    /// reading anything more than 8 bytes will not produce useful results.
    ///
    /// @param address Address to read from.
    /// @param n_bytes Number of bytes to read.
    /// @return Value read.
    /// @throws ReadError if the address is not on the disk.
    dword read_val(word address, int n_bytes);

    /// Writes a little endian value of specified size to disk.
    ///
    /// Interfaced with by the write byte/hword/word public functions. Note,
    /// writing anything more than 8 bytes will not be useful.
    ///
    /// @param address Address to write to.
    /// @param val Value to write.
    /// @param n_bytes Number of bytes to write.
    /// @throws ReadError if the address is not on the disk.
    void write_val(word address, dword val, int n_bytes);

    /// Accesses a cache page.
    ///
    /// Fetches the corresponding cache page of the disk page requested,
    /// evicting a page from cache to make room when necessary.
    ///
    /// @param addr Page to fetch.
    /// @param load Whether the contents of the page are read from the disk file. Not
    ///     needed when the whole page is about to be overwritten.
    /// @return Reference to the cache page.
    /// @throws ReadError If the page is not on the disk.
    CachePage &get_cpage(word addr, bool load = true);

    /// Writes a cache page to disk.
    ///
    /// Writes to disk even if the cache page is not valid or dirty.
    ///
    /// @param cpage Reference to the cache page to write.
    void write_cpage(CachePage &cpage);

    /// Reads a cache page from disk.
    ///
    /// @param cpage Reference to cache page to read to.
    void read_cpage(CachePage &cpage);

    /// Reads and sets up the simulated disk from save files.
    void read_disk_files();

    /// Reads and sets up the disk free page list from save file.
    /// @note Called from @ref Disk::read_disk_files()
    void read_disk_manager_file();
};

/// A @ref Disk in memory, without a file. It keeps the pages that are written
/// to it for as long as it exists, and has as many pages as are needed. It is
/// only a store of pages, it has no addresses (the byte, hword and word accesses
/// do nothing).
class MockDisk : public Disk
{
  public:
    MockDisk();

    // The same operations as Disk, on the pages in memory (see Disk for what each one does).
    word get_free_page() override;
    void return_page(word page) override;

    using Disk::read_page;
    using Disk::write_page;
    void read_page(byte *out, word page) override;
    byte read_byte(word address) override;
    hword read_hword(word address) override;
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
