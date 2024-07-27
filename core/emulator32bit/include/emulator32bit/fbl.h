#pragma once
#ifndef FBL_H
#define FBL_H

#include "emulator32bit/emulator32bit_util.h"

#include <vector>

/**
 * @brief			List of sorted free blocks.
 *
 * @details			The free block list spans a specific range which is defined at the creation
 * 					of the list. Provides a standard way of organizing used and free resources
 * 					like used pages in @ref Disk memory.
 *
 * @todo			TODO: an optimzation that could be made to reduce memory allocation would be to
 * 					store allocated memory into a list to then be used when creating new FreeBlock
 * 					nodes.
 */
class FreeBlockList
{
	public:
		/**
		 * @brief 			Construct a new Free Block List object.
		 *
		 * @param begin		Start value of the range this free block list represents.
		 * @param len		Length of the range.
		 * @param init		Whether the list is initialized to be all free upon construction.
		 */
		FreeBlockList(word begin, word len, bool init = true);

		/**
		 * @brief 			Destroy the Free Block List object and cleans up resources used.
		 */
		~FreeBlockList();

		/**
		 * @brief 			Exceptions that occur from operations.
		 *
		 * @note			If no exception occured, the only valid information would be the type.
		 */
		struct Exception {
			/**
			 * @brief 			Type of exception that occured.
			 */
			enum class Type {
				AOK,						///< Good state
				NOT_ENOUGH_SPACE,			///< Free block list didn't have enough space to alloc
				INVALID_ADDR,				///< Out of bounds location given
				DOUBLE_FREE,				///< Tried to return a block that is already in the list
			};

			Type type = Type::AOK;			///< Type of exception, defaults to AOK
			word addr = 0;					///< Block address
			word length = 0;				///< Block length
		};

		/**
		 * @brief 			Get a free block of the specified length.
		 *
		 * @param length	Length of the block needed.
		 * @param exception	Exception if the operation fails. TODO: Specify what
		 * 					exceptions can occur.
		 * @return 			Address of the start of free block.
		 */
		word get_free_block(word length, Exception& exception);

		/**
		 * @brief			Returns a block back to the list.
		 *
		 * @param addr		Address of the start of block.
		 * @param length	Length of the block.
		 * @param exception	Exception if the operation fails. TODO: Specify what
		 * 					exceptions can occur.
		 */
		void return_block(word addr, word length, Exception& exception);

		/**
		 * @brief			Resets the list back to a clean state.
		 *
		 * @details			All blocks will be considered free.
		 */
		void return_all();

		/**
		 * @brief			Check if a block of a certain length can be taken out of the list.
		 *
		 * @param length	Length of the block.
		 * @return 			True if the block can fit.
		 * @return 			False otherwise.
		 */
		bool can_fit(word length);

		/**
		 * @brief 			Get all free blocks in the list.
		 *
		 * @return 			Returns a vector of pairs representing the free blocks in
		 * 					ascending order, the first element is the address, and the second
		 * 					is the length of the block.
		 */
		std::vector<std::pair<word,word>> get_blocks();

		/**
		 * @brief			Prints all free blocks in the list.
		 */
		void print_blocks();
	private:
		/**
		 * @brief			Represents a free block node in the doubly linked list.
		 */
		struct FreeBlock {
			word addr = 0;					///< Address of the start of the block
			word len = 0;					///< Length of the block
			FreeBlock *next = nullptr;		///< Next block in the list
			FreeBlock *prev = nullptr;		///< Previous block in the list
		};

		word m_begin;						///< Start of the range of blocks represented by list
		word m_len;							///< Length of the range of blocks represented by list
		FreeBlock *m_head = nullptr;		///< Start of list

		/**
		 * @brief			Joins the given block with the next block if possible.
		 *
		 * @param first		Block to be joined with it's next neighbor.
		 */
		void coalesce(FreeBlock *first);

		/**
		 * @brief			Returns a block that can fit the requested length.
		 *
		 * @param length	Length of block to query.
		 * @return 			Pointer to the free block that satisfies the request, nullptr if no
		 * 					block can be found.
		 */
		FreeBlock* find(word length);

		/**
		 * @brief			Inserts a new free block into the list in sorted order.
		 * @note			This will insert a new block regardless of whether it is a valid block
		 * 					to insert back into the list to maintain no intersections. It is up to
		 * 					the caller to handle this.
		 *
		 * @param addr		Address of the start of the new block.
		 * @param length	Length of the new block.
		 * @return 			Pointer to the new block.
		 */
		FreeBlock* insert(word addr, word length);

		/**
		 * @brief			Remove a block from the list.
		 *
		 * @param node		The block to remove.
		 */
		void remove(FreeBlock *node);
};

#endif /* FBL_H */