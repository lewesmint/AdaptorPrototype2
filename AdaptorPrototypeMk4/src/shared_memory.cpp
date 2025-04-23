/**
 * @file shared_memory.cpp
 * @brief Implementation of shared memory management functions
 *
 * This file contains the implementation of functions for creating, accessing,
 * and monitoring shared memory regions in Windows. It provides both low-level
 * Windows API wrappers and higher-level functions for easier use.
 */

#include "shared_memory.h"
#include "memory_layout.h"
#include <windows.h>
#include <iostream>
#include <map>
#include <string>
#include <process.h>  // For _beginthreadex

/**
 * @struct SharedMemoryInfo
 * @brief Structure to hold information about a shared memory region
 *
 * This structure maintains all the necessary information about a shared memory region,
 * including its handle, mapped address, size, and monitoring state. It also provides
 * proper copy semantics to allow storing in STL containers.
 */
struct SharedMemoryInfo {
    HANDLE handle;              ///< Windows handle to the file mapping object
    void* data;                 ///< Pointer to the mapped memory region
    SIZE_T size;                ///< Size of the shared memory region in bytes
    HANDLE monitor_thread;      ///< Thread handle that monitors for changes in the memory
    volatile bool monitoring;   ///< Flag indicating if monitoring is active
    MemoryChangeCallback callback; ///< Callback function to invoke when memory changes

    /**
     * @brief Default constructor
     *
     * Initializes all members to safe default values.
     */
    SharedMemoryInfo() : handle(NULL), data(NULL), size(0), monitor_thread(NULL), monitoring(false), callback(NULL) {}

    /**
     * @brief Copy constructor
     *
     * Creates a new SharedMemoryInfo that shares the same memory region as the original.
     * Note: This does not create a new shared memory region, just a new reference to it.
     *
     * @param other The SharedMemoryInfo to copy from
     */
    SharedMemoryInfo(const SharedMemoryInfo& other) :
        handle(other.handle),
        data(other.data),
        size(other.size),
        monitor_thread(other.monitor_thread),
        monitoring(other.monitoring),
        callback(other.callback) {}

    /**
     * @brief Assignment operator
     *
     * Assigns this SharedMemoryInfo to refer to the same memory region as another.
     * Note: This does not create a new shared memory region, just a new reference to it.
     *
     * @param other The SharedMemoryInfo to assign from
     * @return Reference to this object after assignment
     */
    SharedMemoryInfo& operator=(const SharedMemoryInfo& other) {
        handle = other.handle;
        data = other.data;
        size = other.size;
        monitor_thread = other.monitor_thread;
        monitoring = other.monitoring;
        callback = other.callback;
        return *this;
    }
};

/**
 * @brief Global map of all shared memory regions managed by this process
 *
 * This map stores all the shared memory regions that have been created or opened
 * by this process, indexed by their name. This allows us to keep track of all
 * shared memory regions and properly clean them up when they're no longer needed.
 */
static std::map<std::string, SharedMemoryInfo> shared_memories;

/**
 * @brief Mutex to protect access to the shared_memories map
 *
 * This mutex ensures thread-safe access to the shared_memories map, preventing
 * race conditions when multiple threads try to create, access, or clean up
 * shared memory regions simultaneously.
 */
static HANDLE shared_memories_mutex = NULL;

/**
 * @brief Initialize the mutex for thread safety
 *
 * This function initializes the mutex used to protect the shared_memories map.
 * It should be called before any other shared memory functions.
 */
void initSharedMemoryMutex() {
    if (shared_memories_mutex == NULL) {
        shared_memories_mutex = CreateMutex(NULL, FALSE, NULL);
        if (shared_memories_mutex == NULL) {
            std::cerr << "Failed to create mutex: " << GetLastError() << std::endl;
        }
    }
}

/**
 * @brief Acquire the shared_memories mutex
 *
 * This function acquires the mutex used to protect the shared_memories map.
 * It should be called before accessing the map.
 */
void lockSharedMemoriesMutex() {
    if (shared_memories_mutex != NULL) {
        WaitForSingleObject(shared_memories_mutex, INFINITE);
    }
}

/**
 * @brief Release the shared_memories mutex
 *
 * This function releases the mutex used to protect the shared_memories map.
 * It should be called after accessing the map.
 */
void unlockSharedMemoriesMutex() {
    if (shared_memories_mutex != NULL) {
        ReleaseMutex(shared_memories_mutex);
    }
}

/**
 * @brief Helper function to convert a char* string to LPCSTR (ANSI string)
 *
 * This function is a no-op since we're using the ANSI versions of the Windows API
 * functions. It's included for clarity and to maintain the code structure.
 *
 * @param charString The input string
 * @return The same input string
 */
LPCSTR CharToLPCSTR(const char* charString) {
    // No conversion needed for ANSI functions
    return charString;
}

/**
 * @brief Creates a new shared memory region
 *
 * This function creates a new shared memory region using the Windows CreateFileMapping API.
 * It creates a memory-mapped file backed by the system paging file (not a real file on disk).
 * The shared memory can be accessed by other processes that know its name.
 *
 * @param name The name of the shared memory region (must be unique system-wide)
 * @param size The size of the shared memory region in bytes
 * @return HANDLE to the shared memory region, or NULL if creation failed
 */
HANDLE CreateSharedMemory(const char* name, SIZE_T size) {
    // Convert name to ANSI string (for Windows API)
    LPCSTR ansiName = CharToLPCSTR(name);

    // Create the file mapping object
    // Parameters:
    // - INVALID_HANDLE_VALUE: Use the system paging file instead of a real file
    // - NULL: Default security attributes
    // - PAGE_READWRITE: Allow read and write access to the memory
    // - 0: High 32 bits of the maximum size (0 for sizes < 4GB)
    // - size: Low 32 bits of the maximum size
    // - ansiName: Name of the shared memory object
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,  // Use system paging file
        NULL,                  // Default security attributes
        PAGE_READWRITE,        // Read/write access
        0,                     // High 32 bits of size (0 for sizes < 4GB)
        static_cast<DWORD>(size),  // Low 32 bits of size (cast to avoid warning)
        ansiName               // Name of the shared memory object
    );

    // Check if creation was successful
    if (hMapFile == NULL) {
        std::cerr << "Could not create file mapping object: " << GetLastError() << std::endl;
    }

    return hMapFile;
}

/**
 * @brief Opens an existing shared memory region
 *
 * This function opens an existing shared memory region that was created by another
 * process or by this process earlier. It uses the Windows OpenFileMapping API to
 * get a handle to the shared memory region.
 *
 * @param name The name of the shared memory region to open
 * @return HANDLE to the shared memory region, or NULL if opening failed
 */
HANDLE OpenSharedMemory(const char* name) {
    // Convert name to ANSI string (for Windows API)
    LPCSTR ansiName = CharToLPCSTR(name);

    // Open the file mapping object
    // Parameters:
    // - FILE_MAP_ALL_ACCESS: Request full access to the memory
    // - FALSE: Do not allow inheritance by child processes
    // - ansiName: Name of the shared memory object to open
    HANDLE hMapFile = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,   // Request full access
        FALSE,                 // Do not inherit handle
        ansiName               // Name of the shared memory object
    );

    // Check if opening was successful
    if (hMapFile == NULL) {
        std::cerr << "Could not open file mapping object: " << GetLastError() << std::endl;
    }

    return hMapFile;
}

/**
 * @brief Maps a shared memory region into the process's address space
 *
 * This function maps a shared memory region (identified by a handle) into the
 * process's address space, making it accessible for reading and writing. It uses
 * the Windows MapViewOfFile API to create the mapping.
 *
 * @param hMapFile Handle to the shared memory region
 * @param size Size of the region to map (can be smaller than the total size)
 * @return Pointer to the mapped memory, or NULL if mapping failed
 */
void* MapSharedMemory(HANDLE hMapFile, SIZE_T size) {
    if (hMapFile == NULL) {
        std::cerr << "Invalid handle passed to MapSharedMemory" << std::endl;
        return NULL;
    }

    // Map the shared memory into the process's address space
    // Parameters:
    // - hMapFile: Handle to the file mapping object
    // - FILE_MAP_ALL_ACCESS: Request full access to the memory
    // - 0: High 32 bits of the file offset (0 to start at beginning)
    // - 0: Low 32 bits of the file offset (0 to start at beginning)
    // - size: Number of bytes to map (0 maps the entire file)
    void* pBuf = MapViewOfFile(
        hMapFile,               // Handle to the file mapping object
        FILE_MAP_ALL_ACCESS,   // Request full access
        0,                     // High 32 bits of offset (0 = start at beginning)
        0,                     // Low 32 bits of offset (0 = start at beginning)
        size                   // Number of bytes to map
    );

    // Check if mapping was successful
    if (pBuf == NULL) {
        std::cerr << "Could not map view of file: " << GetLastError() << std::endl;
        return NULL;
    }

    // Check memory alignment for MemoryLayout structure
    // In C++03, we don't have alignof, so we'll use a different approach
    if ((reinterpret_cast<size_t>(pBuf) % 8) != 0) {  // Assuming 8-byte alignment for MemoryLayout
        std::cerr << "Warning: Mapped memory is not properly aligned for MemoryLayout" << std::endl;
        // We continue anyway, but this could cause issues on some architectures
    }

    return pBuf;
}

/**
 * @brief Unmaps a shared memory region from the process's address space
 *
 * This function unmaps a previously mapped shared memory region, making it
 * inaccessible to this process. It uses the Windows UnmapViewOfFile API.
 *
 * @param pData Pointer to the mapped memory region to unmap
 * @return true if unmapping was successful, false otherwise
 */
bool UnmapSharedMemory(void* pData) {
    if (!pData) {
        // Nothing to unmap
        return true;
    }

    // Unmap the view of the file from the process's address space
    BOOL result = UnmapViewOfFile(pData);
    if (!result) {
        DWORD error = GetLastError();
        std::cerr << "Failed to unmap view of file: " << error << std::endl;
        return false;
    }

    return true;
}

/**
 * @brief Closes a handle to a shared memory region
 *
 * This function closes a handle to a shared memory region, releasing the
 * associated resources. It uses the Windows CloseHandle API.
 *
 * @param hMapFile Handle to the shared memory region to close
 * @return true if closing was successful, false otherwise
 */
bool CloseSharedMemory(HANDLE hMapFile) {
    if (!hMapFile) {
        // Nothing to close
        return true;
    }

    // Close the handle to the file mapping object
    BOOL result = CloseHandle(hMapFile);
    if (!result) {
        DWORD error = GetLastError();
        std::cerr << "Failed to close handle: " << error << std::endl;
        return false;
    }

    return true;
}

/**
 * @brief Monitors a shared memory region for changes
 *
 * This function is a placeholder for a more sophisticated monitoring mechanism.
 * In a real implementation, it would set up a way to detect changes in the shared
 * memory region and invoke the callback when changes occur.
 *
 * @param hMapFile Handle to the shared memory region to monitor
 * @param callback Function to call when the memory changes
 */
void MonitorSharedMemory(HANDLE hMapFile, void (*callback)(void*)) {
    // This is a placeholder for a more sophisticated monitoring mechanism
    // In a real implementation, you might use events or other synchronization primitives
    // For now, we just print a warning that this function is not fully implemented
    std::cerr << "MonitorSharedMemory is not fully implemented" << std::endl;

    // Note: Our actual monitoring is implemented in the monitorThreadFunc function,
    // which is used by registerMemoryChangeCallback to poll for changes
}

/**
 * @brief Higher-level wrapper functions for shared memory management
 *
 * The following functions provide a higher-level interface for working with shared memory.
 * They handle the details of creating, mapping, and tracking shared memory regions,
 * making it easier for the application to use shared memory without dealing with
 * the low-level Windows API directly.
 */

/**
 * @brief Initializes a shared memory region with the given name and size
 *
 * This function creates a new shared memory region or opens an existing one with
 * the specified name and size. It handles all the details of creating the file
 * mapping object, mapping it into memory, and initializing it to zeros.
 *
 * The shared memory region is tracked in the global shared_memories map, so it can
 * be accessed later using getSharedMemory() and cleaned up using cleanupSharedMemory().
 *
 * @param name The name of the shared memory region (must be unique system-wide)
 * @param size The size of the shared memory region in bytes
 * @return true if initialization was successful, false otherwise
 */
bool initializeSharedMemory(const char* name, size_t size) {
    // Initialize the mutex if needed
    initSharedMemoryMutex();

    // Lock the shared_memories map to ensure thread safety
    lockSharedMemoriesMutex();

    // Check if this shared memory region is already initialized
    std::map<std::string, SharedMemoryInfo>::iterator it = shared_memories.find(name);
    if (it != shared_memories.end()) {
        // Already initialized, nothing to do
        unlockSharedMemoriesMutex();
        return true;
    }

    // Create the shared memory region using the low-level function
    HANDLE hMapFile = CreateSharedMemory(name, size);
    if (hMapFile == NULL) {
        // Creation failed, return false
        unlockSharedMemoriesMutex();
        return false;
    }

    // Map the shared memory into our address space
    void* pBuf = MapSharedMemory(hMapFile, size);
    if (pBuf == NULL) {
        // Mapping failed, clean up and return false
        CloseSharedMemory(hMapFile);
        unlockSharedMemoriesMutex();
        return false;
    }

    // Initialize the memory to zeros
    // This ensures that the memory starts in a known state
    memset(pBuf, 0, size);

    // Create a new SharedMemoryInfo object to track this shared memory region
    SharedMemoryInfo info;
    info.handle = hMapFile;      // Windows handle to the file mapping object
    info.data = pBuf;           // Pointer to the mapped memory
    info.size = size;           // Size of the shared memory region
    info.monitor_thread = NULL;  // No monitoring thread yet
    info.monitoring = false;    // Not monitoring yet
    info.callback = NULL;       // No callback function yet

    // Add the shared memory info to our map for future reference
    shared_memories[name] = info;

    unlockSharedMemoriesMutex();
    return true;
}

/**
 * @brief Gets a pointer to a shared memory region
 *
 * This function returns a pointer to a shared memory region with the given name.
 * If the region has already been initialized or opened by this process, it returns
 * the existing pointer. Otherwise, it tries to open an existing shared memory region
 * created by another process.
 *
 * The returned pointer can be cast to the appropriate type (e.g., MemoryLayout*)
 * to access the shared memory contents.
 *
 * @param name The name of the shared memory region to access
 * @return Pointer to the shared memory region, or nullptr if it doesn't exist or can't be opened
 */
void* getSharedMemory(const char* name) {
    // Initialize the mutex if needed
    initSharedMemoryMutex();

    // Lock the shared_memories map to ensure thread safety
    lockSharedMemoriesMutex();

    // Check if we already have this shared memory region in our map
    std::map<std::string, SharedMemoryInfo>::iterator it = shared_memories.find(name);
    if (it != shared_memories.end()) {
        // We already have it, return the pointer to the mapped memory
        void* result = it->second.data;
        unlockSharedMemoriesMutex();
        return result;
    }

    // We don't have it yet, try to open an existing shared memory region
    HANDLE hMapFile = OpenSharedMemory(name);
    if (hMapFile == NULL) {
        // The shared memory region doesn't exist or can't be opened
        unlockSharedMemoriesMutex();
        return NULL;
    }

    // We don't know the exact size of the shared memory region when opening it,
    // so we'll use a reasonable default based on our expected structure
    SIZE_T size = sizeof(MemoryLayout);

    // Map the shared memory into our address space
    void* pBuf = MapSharedMemory(hMapFile, size);
    if (pBuf == NULL) {
        // Mapping failed, clean up and return NULL
        CloseSharedMemory(hMapFile);
        unlockSharedMemoriesMutex();
        return NULL;
    }

    // Create a new SharedMemoryInfo object to track this shared memory region
    SharedMemoryInfo info;
    info.handle = hMapFile;      // Windows handle to the file mapping object
    info.data = pBuf;           // Pointer to the mapped memory
    info.size = size;           // Size of the shared memory region
    info.monitor_thread = NULL;  // No monitoring thread yet
    info.monitoring = false;    // Not monitoring yet
    info.callback = NULL;       // No callback function yet

    // Add the shared memory info to our map for future reference
    shared_memories[name] = info;

    unlockSharedMemoriesMutex();
    return pBuf;
}

/**
 * @brief Cleans up a shared memory region
 *
 * This function releases all resources associated with a shared memory region,
 * including stopping any monitoring threads, unmapping the memory, and closing
 * the handle. It also removes the region from the shared_memories map.
 *
 * After calling this function, any pointers to the shared memory region obtained
 * from getSharedMemory() will be invalid and should not be used.
 *
 * @param name The name of the shared memory region to clean up
 * @return true if cleanup was successful, false if there were errors
 */
bool cleanupSharedMemory(const char* name) {
    if (!name) {
        std::cerr << "Null name passed to cleanupSharedMemory" << std::endl;
        return false;
    }

    bool success = true;

    // Initialize the mutex if needed
    initSharedMemoryMutex();

    // Lock the shared_memories map to ensure thread safety
    lockSharedMemoriesMutex();

    // Find the shared memory region in our map
    std::map<std::string, SharedMemoryInfo>::iterator it = shared_memories.find(name);
    if (it != shared_memories.end()) {
        // Stop monitoring thread if it's active
        if (it->second.monitoring && it->second.monitor_thread) {
            // Signal the thread to stop by setting monitoring to false
            it->second.monitoring = false;

            // Wait for the thread to finish
            WaitForSingleObject(it->second.monitor_thread, INFINITE);

            // Clean up the thread object
            CloseHandle(it->second.monitor_thread);
            it->second.monitor_thread = NULL;
        }

        // Unmap the shared memory from our address space
        if (!UnmapSharedMemory(it->second.data)) {
            success = false;
            // Continue cleanup despite the error
        }
        it->second.data = NULL; // Prevent double-unmap

        // Close the handle to the file mapping object
        if (!CloseSharedMemory(it->second.handle)) {
            success = false;
            // Continue cleanup despite the error
        }
        it->second.handle = NULL; // Prevent double-close

        // Remove the shared memory info from our map
        shared_memories.erase(it);
    }
    // If the shared memory region isn't in our map, there's nothing to clean up

    unlockSharedMemoriesMutex();
    return success;
}

/**
 * @brief Checks if a shared memory region has changed since a given version
 *
 * This function checks if the version number in a shared memory region has
 * increased since the last known version. This can be used to detect changes
 * made by other processes.
 *
 * @param name The name of the shared memory region to check
 * @param last_known_version The last known version number
 * @return true if the memory has changed (version increased), false otherwise
 */
bool hasMemoryChanged(const char* name, uint64_t last_known_version) {
    // Initialize the mutex if needed
    initSharedMemoryMutex();

    // Lock the shared_memories map to ensure thread safety
    lockSharedMemoriesMutex();

    // Find the shared memory region in our map
    std::map<std::string, SharedMemoryInfo>::iterator it = shared_memories.find(name);
    if (it != shared_memories.end()) {
        // Cast the memory pointer to our expected structure type
        MemoryLayout* layout = static_cast<MemoryLayout*>(it->second.data);

        // Check if the version has increased
        bool result = layout->version > last_known_version;
        unlockSharedMemoriesMutex();
        return result;
    }

    // If the shared memory region isn't in our map, it hasn't changed
    unlockSharedMemoriesMutex();
    return false;
}

// Thread data structure for monitoring thread
struct MonitorThreadData {
    std::string name;
    SharedMemoryInfo* info;
};

/**
 * @brief Thread function for monitoring shared memory changes
 *
 * This function runs in a separate thread and continuously monitors a shared memory
 * region for changes. When it detects a change (version number increase), it invokes
 * the registered callback function.
 *
 * The thread continues running until the monitoring flag is set to false.
 *
 * @param arg Pointer to a MonitorThreadData structure containing thread information
 * @return Thread exit code
 */
unsigned int __stdcall monitorThreadFunc(void* arg) {
    MonitorThreadData* data = static_cast<MonitorThreadData*>(arg);
    SharedMemoryInfo* info = data->info;
    std::string name = data->name;
    delete data;  // Free the thread data

    // Cast the memory pointer to our expected structure type
    MemoryLayout* layout = static_cast<MemoryLayout*>(info->data);

    // Remember the current version to detect changes
    uint64_t lastVersion = layout->version;

    // Continue monitoring until the monitoring flag is set to false
    while (info->monitoring) {
        // Check if the version has increased since we last checked
        if (layout->version > lastVersion) {
            // Version has increased, memory has changed

            // Call the callback function if one is registered
            if (info->callback) {
                info->callback(info->data);
            }

            // Update our last known version
            lastVersion = layout->version;
        }

        // Sleep briefly to avoid consuming too much CPU
        // This determines how quickly we detect changes (10ms latency here)
        Sleep(10);
    }

    return 0;
}

/**
 * @brief Registers a callback function for shared memory changes
 *
 * This function sets up a callback function that will be called whenever the
 * shared memory region changes (when its version number increases). It also
 * starts a monitoring thread if one isn't already running.
 *
 * The callback function will be called from the monitoring thread, not the
 * main thread, so it should be thread-safe.
 *
 * @param name The name of the shared memory region to monitor
 * @param callback The function to call when the memory changes
 * @return true if the callback was registered successfully, false otherwise
 */
bool registerMemoryChangeCallback(const char* name, MemoryChangeCallback callback) {
    // Initialize the mutex if needed
    initSharedMemoryMutex();

    // Lock the shared_memories map to ensure thread safety
    lockSharedMemoriesMutex();

    // Find the shared memory region in our map
    std::map<std::string, SharedMemoryInfo>::iterator it = shared_memories.find(name);
    if (it == shared_memories.end()) {
        // The shared memory region doesn't exist in our map
        unlockSharedMemoriesMutex();
        return false;
    }

    // Set the callback function
    it->second.callback = callback;

    // Start a monitoring thread if one isn't already running
    if (!it->second.monitoring) {
        // Set the monitoring flag to true
        it->second.monitoring = true;

        // Create thread data
        MonitorThreadData* data = new MonitorThreadData();
        data->name = name;
        data->info = &(it->second);

        // Create a new thread that runs the monitorThreadFunc function
        unsigned int threadId;
        it->second.monitor_thread = (HANDLE)_beginthreadex(
            NULL,                   // Default security attributes
            0,                      // Default stack size
            monitorThreadFunc,      // Thread function
            data,                   // Thread argument
            0,                      // Default creation flags
            &threadId               // Thread identifier
        );

        if (it->second.monitor_thread == NULL) {
            std::cerr << "Failed to create monitoring thread: " << GetLastError() << std::endl;
            delete data;
            it->second.monitoring = false;
            unlockSharedMemoriesMutex();
            return false;
        }
    }

    unlockSharedMemoriesMutex();
    return true;
}