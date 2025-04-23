#include <gtest/gtest.h>
#include "../src/shared_memory.h"
#include "../src/structs/memory_layout.h"

class SharedMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize shared memory for testing
        ASSERT_TRUE(initializeSharedMemory("TestSharedMemory", sizeof(MemoryLayout)));
    }

    void TearDown() override {
        // Clean up shared memory after tests
        cleanupSharedMemory("TestSharedMemory");
    }
};

TEST_F(SharedMemoryTest, TestWriteRead) {
    MemoryLayout* mem = static_cast<MemoryLayout*>(getSharedMemory("TestSharedMemory"));
    ASSERT_NE(mem, nullptr);

    // Write to shared memory
    mem->data = 42;
    
    // Read from shared memory
    ASSERT_EQ(mem->data, 42);
}

TEST_F(SharedMemoryTest, TestMultipleAccess) {
    MemoryLayout* mem1 = static_cast<MemoryLayout*>(getSharedMemory("TestSharedMemory"));
    MemoryLayout* mem2 = static_cast<MemoryLayout*>(getSharedMemory("TestSharedMemory"));
    
    ASSERT_NE(mem1, nullptr);
    ASSERT_NE(mem2, nullptr);

    // Write to shared memory from first instance
    mem1->data = 100;

    // Read from shared memory from second instance
    ASSERT_EQ(mem2->data, 100);
}