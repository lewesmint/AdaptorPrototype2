#include <gtest/gtest.h>
#include "../src/change_tracking.h"
#include "../src/sync_message.h"
#include "../src/memory_layout.h"
#include <string>

class PartialUpdatesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize change tracking
        initChangeTracking();
    }

    void TearDown() override {
        // Clean up change tracking
        cleanupChangeTracking();
    }
};

TEST_F(PartialUpdatesTest, MarkRegionChanged) {
    // Create a test memory region
    const char* memoryName = "TestMemory";
    
    // Mark a region as changed
    markRegionChanged(memoryName, 10, 20);
    
    // Check that the change was recorded
    lockChangesMutex();
    ASSERT_TRUE(g_pendingChanges.find(memoryName) != g_pendingChanges.end());
    ASSERT_EQ(g_pendingChanges[memoryName].size(), 1);
    ASSERT_EQ(g_pendingChanges[memoryName][0].offset, 10);
    ASSERT_EQ(g_pendingChanges[memoryName][0].size, 20);
    unlockChangesMutex();
}

TEST_F(PartialUpdatesTest, MarkFieldChanged) {
    // Create a test memory region
    const char* memoryName = "TestMemory";
    
    // Mark a field as changed
    markFieldChanged(memoryName, 4, 8);
    
    // Check that the change was recorded
    lockChangesMutex();
    ASSERT_TRUE(g_pendingChanges.find(memoryName) != g_pendingChanges.end());
    ASSERT_EQ(g_pendingChanges[memoryName].size(), 1);
    ASSERT_EQ(g_pendingChanges[memoryName][0].offset, 4);
    ASSERT_EQ(g_pendingChanges[memoryName][0].size, 8);
    unlockChangesMutex();
}

TEST_F(PartialUpdatesTest, GenerateUniqueId) {
    // Generate multiple IDs and check that they're unique
    uint64_t id1 = generateUniqueId();
    uint64_t id2 = generateUniqueId();
    uint64_t id3 = generateUniqueId();
    
    ASSERT_NE(id1, id2);
    ASSERT_NE(id2, id3);
    ASSERT_NE(id1, id3);
}

TEST_F(PartialUpdatesTest, UpdateTimeouts) {
    // Add some in-progress updates
    lockUpdatesMutex();
    
    UpdateInfo info1;
    info1.updateId = 1;
    info1.startTime = GetTickCount64() - (UPDATE_TIMEOUT_MS + 1000); // Expired
    g_inProgressUpdates[1] = info1;
    
    UpdateInfo info2;
    info2.updateId = 2;
    info2.startTime = GetTickCount64() - 1000; // Not expired
    g_inProgressUpdates[2] = info2;
    
    unlockUpdatesMutex();
    
    // Check timeouts
    checkUpdateTimeouts();
    
    // Verify that the expired update was removed
    lockUpdatesMutex();
    ASSERT_TRUE(g_inProgressUpdates.find(1) == g_inProgressUpdates.end());
    ASSERT_TRUE(g_inProgressUpdates.find(2) != g_inProgressUpdates.end());
    unlockUpdatesMutex();
}

TEST_F(PartialUpdatesTest, MessageTypes) {
    // Create messages of different types
    SyncMessage msg1;
    msg1.msgType = MSG_SINGLE_UPDATE;
    
    SyncMessage msg2;
    msg2.msgType = MSG_START_UPDATE;
    
    SyncMessage msg3;
    msg3.msgType = MSG_UPDATE_CHUNK;
    
    SyncMessage msg4;
    msg4.msgType = MSG_END_UPDATE;
    
    // Verify the types
    ASSERT_EQ(msg1.msgType, MSG_SINGLE_UPDATE);
    ASSERT_EQ(msg2.msgType, MSG_START_UPDATE);
    ASSERT_EQ(msg3.msgType, MSG_UPDATE_CHUNK);
    ASSERT_EQ(msg4.msgType, MSG_END_UPDATE);
}

TEST_F(PartialUpdatesTest, MultipleChanges) {
    // Create a test memory region
    const char* memoryName = "TestMemory";
    
    // Mark multiple regions as changed
    markRegionChanged(memoryName, 0, 4);   // First 4 bytes
    markRegionChanged(memoryName, 8, 4);   // 4 bytes at offset 8
    markRegionChanged(memoryName, 16, 8);  // 8 bytes at offset 16
    
    // Check that all changes were recorded
    lockChangesMutex();
    ASSERT_TRUE(g_pendingChanges.find(memoryName) != g_pendingChanges.end());
    ASSERT_EQ(g_pendingChanges[memoryName].size(), 3);
    
    // Check first change
    ASSERT_EQ(g_pendingChanges[memoryName][0].offset, 0);
    ASSERT_EQ(g_pendingChanges[memoryName][0].size, 4);
    
    // Check second change
    ASSERT_EQ(g_pendingChanges[memoryName][1].offset, 8);
    ASSERT_EQ(g_pendingChanges[memoryName][1].size, 4);
    
    // Check third change
    ASSERT_EQ(g_pendingChanges[memoryName][2].offset, 16);
    ASSERT_EQ(g_pendingChanges[memoryName][2].size, 8);
    
    unlockChangesMutex();
}
