// Make sure winsock2.h is included before windows.h to avoid conflicts
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#include "change_tracking.h"
#include "sync_message.h"
#include "network_sync.h"
#include <iostream>
#include <algorithm>

// Initialize global variables
std::map<std::string, std::vector<MemoryChange> > g_pendingChanges;
std::map<uint64_t, UpdateInfo> g_inProgressUpdates;
HANDLE g_changesMutex = NULL;
HANDLE g_updatesMutex = NULL;

void initChangeTracking() {
    // Initialize mutexes if they haven't been already
    if (g_changesMutex == NULL) {
        g_changesMutex = CreateMutex(NULL, FALSE, NULL);
        if (g_changesMutex == NULL) {
            std::cerr << "Failed to create changes mutex: " << GetLastError() << std::endl;
        }
    }

    if (g_updatesMutex == NULL) {
        g_updatesMutex = CreateMutex(NULL, FALSE, NULL);
        if (g_updatesMutex == NULL) {
            std::cerr << "Failed to create updates mutex: " << GetLastError() << std::endl;
        }
    }
}

void cleanupChangeTracking() {
    // Clean up mutexes
    if (g_changesMutex) {
        CloseHandle(g_changesMutex);
        g_changesMutex = NULL;
    }

    if (g_updatesMutex) {
        CloseHandle(g_updatesMutex);
        g_updatesMutex = NULL;
    }

    // Clear the maps
    lockChangesMutex();
    g_pendingChanges.clear();
    unlockChangesMutex();

    lockUpdatesMutex();
    g_inProgressUpdates.clear();
    unlockUpdatesMutex();
}

void markRegionChanged(const char* memoryName, size_t offset, size_t size) {
    // Get a pointer to the shared memory
    void* sharedMem = getSharedMemory(memoryName);
    if (!sharedMem) {
        std::cerr << "Failed to get shared memory for marking change" << std::endl;
        return;
    }

    // Create a change record
    MemoryChange change;
    change.offset = offset;
    change.size = size;
    change.inProgress = false;

    // Add the change to our pending changes list
    lockChangesMutex();
    g_pendingChanges[memoryName].push_back(change);
    unlockChangesMutex();

    // Mark the memory as dirty and increment the version
    // This assumes the memory layout has version and dirty fields at the beginning
    MemoryLayout* layout = static_cast<MemoryLayout*>(sharedMem);
    layout->version++;
    layout->dirty = true;
}

void markFieldChanged(const char* memoryName, size_t fieldOffset, size_t fieldSize) {
    markRegionChanged(memoryName, fieldOffset, fieldSize);
}

uint64_t generateUniqueId() {
    // Generate a unique ID based on time and a random component
    static uint64_t lastId = 0;
    uint64_t newId = (uint64_t)GetTickCount64() << 32 | (rand() & 0xFFFFFFFF);

    // Ensure it's different from the last one
    if (newId == lastId) {
        newId++;
    }

    lastId = newId;
    return newId;
}

void checkUpdateTimeouts() {
    uint64_t currentTime = GetTickCount64();
    std::vector<uint64_t> timeoutIds;

    // Find updates that have timed out
    lockUpdatesMutex();
    std::map<uint64_t, UpdateInfo>::iterator it;
    for (it = g_inProgressUpdates.begin(); it != g_inProgressUpdates.end(); ++it) {
        if (currentTime - it->second.startTime > UPDATE_TIMEOUT_MS) {
            timeoutIds.push_back(it->first);
        }
    }

    // Remove timed-out updates
    for (size_t i = 0; i < timeoutIds.size(); i++) {
        std::cerr << "Update " << timeoutIds[i] << " timed out and was discarded" << std::endl;
        g_inProgressUpdates.erase(timeoutIds[i]);
    }
    unlockUpdatesMutex();
}

void applyUpdate(const SyncMessage& message) {
    // Get the shared memory
    void* sharedMem = getSharedMemory(message.memoryName);
    if (sharedMem) {
        // Calculate the target address
        char* target = static_cast<char*>(sharedMem) + message.offset;

        // Copy the data
        memcpy(target, message.data, message.size);

        // Invoke the callback if registered
        if (g_networkCallback) {
            g_networkCallback(message.memoryName, message.offset, message.size);
        }
    }
}

void applyMultipartUpdate(uint64_t updateId) {
    lockUpdatesMutex();

    // Get the chunks for this update
    std::map<uint64_t, UpdateInfo>::iterator it = g_inProgressUpdates.find(updateId);
    if (it != g_inProgressUpdates.end()) {
        std::vector<SyncMessage>& chunks = it->second.chunks;

        // Sort chunks by offset
        std::sort(chunks.begin(), chunks.end(),
                  [](const SyncMessage& a, const SyncMessage& b) {
                      return a.offset < b.offset;
                  });

        // Apply each chunk
        for (size_t i = 0; i < chunks.size(); i++) {
            applyUpdate(chunks[i]);
        }
    }

    unlockUpdatesMutex();
}

void lockChangesMutex() {
    if (g_changesMutex != NULL) {
        WaitForSingleObject(g_changesMutex, INFINITE);
    }
}

void unlockChangesMutex() {
    if (g_changesMutex != NULL) {
        ReleaseMutex(g_changesMutex);
    }
}

void lockUpdatesMutex() {
    if (g_updatesMutex != NULL) {
        WaitForSingleObject(g_updatesMutex, INFINITE);
    }
}

void unlockUpdatesMutex() {
    if (g_updatesMutex != NULL) {
        ReleaseMutex(g_updatesMutex);
    }
}
