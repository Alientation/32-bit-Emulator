#include "emulator32bit_test/emulator32bit_test.h"

#include "emulator32bit/fbl_inmemory.h"


TEST(fbl_inmemory, in_order)
{
    const int MEM_SIZE = 4096;
    const int BLOCK_SIZE = 1024;
    byte mem[MEM_SIZE];
    FBL_InMemory fbl(mem, 0, MEM_SIZE, BLOCK_SIZE);

    ASSERT_FALSE(fbl.empty());
    
    ASSERT_EQ(fbl.nfree(), 4);
    word b1 = fbl.get_free_block();
    ASSERT_EQ(fbl.nfree(), 3);
    word b2 = fbl.get_free_block();
    ASSERT_EQ(fbl.nfree(), 2);
    word b3 = fbl.get_free_block();
    ASSERT_EQ(fbl.nfree(), 1);
    word b4 = fbl.get_free_block();
    ASSERT_EQ(fbl.nfree(), 0);

    ASSERT_NE(b1, b2);
    ASSERT_NE(b1, b3);
    ASSERT_NE(b1, b4);
    ASSERT_NE(b2, b3);
    ASSERT_NE(b2, b4);
    ASSERT_NE(b3, b4);

    ASSERT_EQ(b1 % BLOCK_SIZE, 0);
    ASSERT_EQ(b2 % BLOCK_SIZE, 0);
    ASSERT_EQ(b3 % BLOCK_SIZE, 0);
    ASSERT_EQ(b4 % BLOCK_SIZE, 0);

    ASSERT_TRUE(b1 >= 0 && b1 < MEM_SIZE);
    ASSERT_TRUE(b2 >= 0 && b2 < MEM_SIZE);
    ASSERT_TRUE(b3 >= 0 && b3 < MEM_SIZE);
    ASSERT_TRUE(b4 >= 0 && b4 < MEM_SIZE);

    ASSERT_TRUE(fbl.empty());

    fbl.return_block(b1);
    ASSERT_FALSE(fbl.empty());
    ASSERT_EQ(b1, fbl.get_free_block());

    fbl.return_block(b1);
    ASSERT_EQ(fbl.nfree(), 1);
    fbl.return_block(b2);
    ASSERT_EQ(fbl.nfree(), 2);
    fbl.return_block(b3);
    ASSERT_EQ(fbl.nfree(), 3);
    fbl.return_block(b4);
    ASSERT_EQ(fbl.nfree(), 4);
}

TEST(fbl_inmemory, out_of_order)
{
    const int MEM_SIZE = 4096;
    const int BLOCK_SIZE = 1024;
    byte mem[MEM_SIZE];
    FBL_InMemory fbl(mem, 0, MEM_SIZE, BLOCK_SIZE);

    ASSERT_FALSE(fbl.empty());

    word b1 = fbl.get_free_block();
    word b2 = fbl.get_free_block();
    word b3 = fbl.get_free_block();
    word b4 = fbl.get_free_block();

    ASSERT_TRUE(fbl.empty());

    fbl.return_block(b2);
    ASSERT_EQ(fbl.nfree(), 1);
    ASSERT_EQ(fbl.nnodes(), 1);
    fbl.return_block(b1);
    ASSERT_EQ(fbl.nfree(), 2);
    ASSERT_EQ(fbl.nnodes(), 1);

    fbl.return_block(b4);
    ASSERT_EQ(fbl.nfree(), 3);
    ASSERT_EQ(fbl.nnodes(), 2);

    fbl.return_block(b3);
    ASSERT_EQ(fbl.nfree(), 4);
    ASSERT_EQ(fbl.nnodes(), 1);
}