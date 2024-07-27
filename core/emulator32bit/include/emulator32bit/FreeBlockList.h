#pragma once
#ifndef FREEBLOCKLIST_H
#define FREEBLOCKLIST_H

#include "emulator32bit/Emulator32bitUtil.h"

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
class FreeBlockList {
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
		 * @brief Get the free block object
		 *
		 * @param length
		 * @param exception
		 * @return word
		 */
		word get_free_block(word length, Exception& exception);

		/**
		 * @brief
		 *
		 * @param addr
		 * @param length
		 * @param exception
		 */
		void return_block(word addr, word length, Exception& exception);

		/**
		 * @brief
		 *
		 */
		void return_all();

		/**
		 * @brief
		 *
		 * @param length
		 * @return true
		 * @return false
		 */
		bool can_fit(word length);

		/**
		 * @brief Get the blocks object
		 *
		 * @return std::vector<std::pair<word,word>>
		 */
		std::vector<std::pair<word,word>> get_blocks();

		/**
		 * @brief
		 *
		 */
		void print_blocks();
	private:
		/**
		 * @brief
		 *
		 */
		struct FreeBlock {
			word addr = 0;					///<
			word len = 0;					///<
			FreeBlock *next = nullptr;		///<
			FreeBlock *prev = nullptr;		///<
		};

		word m_begin;						///<
		word m_len;							///<
		FreeBlock *m_head = nullptr;		///<

		/**
		 * @brief
		 *
		 * @param first
		 */
		void coalesce(FreeBlock *first);

		/**
		 * @brief
		 *
		 * @param length
		 * @return FreeBlock*
		 */
		FreeBlock* find(word length);

		/**
		 * @brief
		 *
		 * @param addr
		 * @param length
		 * @return FreeBlock*
		 */
		FreeBlock* insert(word addr, word length);

		/**
		 * @brief
		 *
		 * @param node
		 */
		void remove(FreeBlock *node);
};




#endif /* FREEBLOCKLIST_H */