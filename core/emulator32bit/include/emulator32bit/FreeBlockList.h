#pragma once
#ifndef FREEBLOCKLIST_H
#define FREEBLOCKLIST_H

#include "emulator32bit/Emulator32bitUtil.h"

#include <vector>
class FreeBlockList {
	public:
		FreeBlockList(word begin, word len, bool init = true);
		~FreeBlockList();

		struct Exception {
			enum class Type {
				AOK, NOT_ENOUGH_SPACE, INVALID_ADDR, DOUBLE_FREE,
			} type = Type::AOK;
			word addr = 0;
			word length = 0;
		};

		word get_free_block(word length, Exception& exception);
		void return_block(word addr, word length, Exception& exception);
		void return_all();

		bool can_fit(word length);
		std::vector<std::pair<word,word>> get_blocks();
	private:
		struct FreeBlock {
			word addr = 0;
			word len = 0;
			FreeBlock *next = nullptr;
			FreeBlock *prev = nullptr;
		};

		word m_begin;
		word m_len;
		FreeBlock *m_head = nullptr;

		void coalesce(FreeBlock *first);
};




#endif /* FREEBLOCKLIST_H */