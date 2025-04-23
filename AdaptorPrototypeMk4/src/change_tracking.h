#ifndef CHANGE_TRACKING_H
#define CHANGE_TRACKING_H

#include <windows.h>
#include <vector>
#include <map>
#include <string>
#include "shared_memory.h"
#include "sync_message.h"
#include "memory_layout.h"

/**
 * @brief Structure to track a change to a region of shared memory
 */
struct MemoryChange {
    size_t offset;      // Offset from the start of the shared memory
    size_t size;        // Size of the changed data
    bool inProgress;    // Flag indicating if this change is part of a larger update
};

/**
 * @brief Structure to track an in-progress multi-part update
 */
struct UpdateInfo {
    uint64_t updateId;                  // Unique ID for this update
    std::vector<SyncMessage> chunks;    // Chunks received so far
    uint64_t startTime;                 // Time when first chunk was received
};

// Vector to track pending changes for each memory region
extern std::map<std::string, std::vector<MemoryChange> > g_pendingChanges;

// Map of in-progress updates with timing information
extern std::map<uint64_t, UpdateInfo> g_inProgressUpdates;

// Mutex for protecting the pendingChanges map
extern HANDLE g_changesMutex;

// Mutex for protecting the inProgressUpdates map
extern HANDLE g_updatesMutex;

// Timeout for multi-part updates (milliseconds)
#define UPDATE_TIMEOUT_MS 5000

/**
 * @brief Initialize the change tracking system
 *
 * This function initializes the mutexes used for thread safety.
 */
void initChangeTracking();

/**
 * @brief Clean up the change tracking system
 *
 * This function cleans up resources used by the change tracking system.
 */
void cleanupChangeTracking();

/**
 * @brief Mark a region of shared memory as changed
 *
 * This function adds a change to the pending changes list for a memory region.
 *
 * @param memoryName Name of the shared memory region
 * @param offset Offset within the shared memory
 * @param size Size of the changed data
 */
void markRegionChanged(const char* memoryName, size_t offset, size_t size);

/**
 * @brief Mark a specific field as changed
 *
 * This function marks a specific field in a structure as changed.
 *
 * @param memoryName Name of the shared memory region
 * @param fieldOffset Offset of the field within the structure
 * @param fieldSize Size of the field
 */
void markFieldChanged(const char* memoryName, size_t fieldOffset, size_t fieldSize);

/**
 * @brief Generate a unique update ID
 *
 * This function generates a unique ID for a multi-part update.
 *
 * @return A unique update ID
 */
uint64_t generateUniqueId();

/**
 * @brief Check for timed-out updates
 *
 * This function checks for updates that have been in progress too long
 * and removes them from the in-progress updates map.
 */
void checkUpdateTimeouts();

/**
 * @brief Apply a single update to shared memory
 *
 * This function applies a single update to shared memory.
 *
 * @param message The message containing the update
 */
void applyUpdate(const SyncMessage& message);

/**
 * @brief Apply a multi-part update to shared memory
 *
 * This function applies all chunks of a multi-part update to shared memory.
 *
 * @param updateId The ID of the update to apply
 */
void applyMultipartUpdate(uint64_t updateId);

/**
 * @brief Lock the changes mutex
 */
void lockChangesMutex();

/**
 * @brief Unlock the changes mutex
 */
void unlockChangesMutex();

/**
 * @brief Lock the updates mutex
 */
void lockUpdatesMutex();

/**
 * @brief Unlock the updates mutex
 */
void unlockUpdatesMutex();

#endif // CHANGE_TRACKING_H
