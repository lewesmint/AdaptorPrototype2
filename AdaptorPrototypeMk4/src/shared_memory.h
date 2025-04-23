#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <Windows.h>
#include <string>
#include <map>

// Function to create shared memory
HANDLE CreateSharedMemory(const char* name, SIZE_T size);

// Function to open existing shared memory
HANDLE OpenSharedMemory(const char* name);

// Function to map shared memory to the process's address space
void* MapSharedMemory(HANDLE hMapFile, SIZE_T size);

// Function to unmap shared memory
bool UnmapSharedMemory(void* pData);

// Function to close shared memory handle
bool CloseSharedMemory(HANDLE hMapFile);

// Function to monitor shared memory for changes
void MonitorSharedMemory(HANDLE hMapFile, void (*callback)(void*));

// Initialize shared memory with given name and size
bool initializeSharedMemory(const char* name, size_t size);

// Get a pointer to the shared memory region
void* getSharedMemory(const char* name);

// Clean up shared memory resources
bool cleanupSharedMemory(const char* name);

// Check if memory has changed since last check
bool hasMemoryChanged(const char* name, uint64_t last_known_version);

// Register a callback for when memory changes
typedef void (*MemoryChangeCallback)(void* memory_ptr);
bool registerMemoryChangeCallback(const char* name, MemoryChangeCallback callback);

#endif // SHARED_MEMORY_H