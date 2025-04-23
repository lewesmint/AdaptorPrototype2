# Partial Updates for Shared Memory Synchronization

This document outlines the design and implementation of a partial update system for the Shared Memory Sync application. The goal is to optimize network traffic by sending only the changed portions of shared memory structures rather than the entire structure.

## Current Implementation Limitations

The current implementation sends the entire shared memory structure whenever any part of it changes:

```cpp
// Set the offset and size
// For simplicity, we're syncing the whole structure
message.offset = 0;
message.size = sizeof(MemoryLayout);

// Copy the shared memory data to the message
memcpy(message.data, sharedMem, message.size);
```

While this approach works well for small structures, it becomes inefficient for larger data structures where only small portions might change at a time.

## Proposed Enhancement: Partial Updates

### 1. Core Concepts

The enhanced system will:

1. **Track specific changes** to regions of shared memory
2. **Send only changed data** with offset and size information
3. **Support multi-part updates** for changes that exceed the maximum message size
4. **Ensure update integrity** with start and end markers for multi-part updates

### 2. Message Types

We'll introduce different message types to handle various update scenarios:

```cpp
typedef enum {
    MSG_START_UPDATE,   // Start of an update sequence
    MSG_UPDATE_CHUNK,   // Middle chunk of an update
    MSG_END_UPDATE,     // End of an update sequence
    MSG_SINGLE_UPDATE   // Complete update in a single message
} MessageType;
```

### 3. Enhanced SyncMessage Structure

The `SyncMessage` structure will be enhanced to support partial updates:

```cpp
typedef struct {
    char memoryName[MAX_MEMORY_NAME_LENGTH]; // Name of the shared memory region
    MessageType msgType;                     // Type of message
    uint64_t updateId;                       // Unique ID for this update sequence
    size_t offset;                           // Offset within the shared memory
    size_t size;                             // Size of the data being synchronized
    uint32_t timestamp;                      // Timestamp of when the message was created
    char data[MAX_SYNC_DATA_SIZE];           // Data to be synchronized
} SyncMessage;
```

### 4. Change Tracking

To track which parts of the shared memory have changed:

```cpp
// Structure to track a change to a region of shared memory
struct MemoryChange {
    size_t offset;      // Offset from the start of the shared memory
    size_t size;        // Size of the changed data
    bool inProgress;    // Flag indicating if this change is part of a larger update
};

// Vector to track pending changes
std::vector<MemoryChange> pendingChanges;
```

### 5. Sending Partial Updates

When changes are detected, only the changed regions will be sent:

```cpp
// For each pending change
for (size_t i = 0; i < pendingChanges.size(); i++) {
    // Create a synchronization message
    SyncMessage message;
    
    // Set message type (START, CHUNK, END, or SINGLE)
    // ...
    
    // Set the offset and size for this chunk
    message.offset = pendingChanges[i].offset;
    message.size = pendingChanges[i].size;
    
    // Copy just the changed data
    char* source = static_cast<char*>(sharedMem) + message.offset;
    memcpy(message.data, source, message.size);
    
    // Send the message to all remote nodes
    // ...
}
```

### 6. Receiving and Reassembling Updates

The receiving end will handle different message types and reassemble multi-part updates:

```cpp
switch (message.msgType) {
    case MSG_SINGLE_UPDATE:
        // Apply the change immediately
        applyUpdate(message);
        break;
        
    case MSG_START_UPDATE:
        // Start tracking a new multi-part update
        inProgressUpdates[message.updateId].push_back(message);
        break;
        
    case MSG_UPDATE_CHUNK:
        // Add to an existing update
        inProgressUpdates[message.updateId].push_back(message);
        break;
        
    case MSG_END_UPDATE:
        // Add the final chunk and apply the complete update
        inProgressUpdates[message.updateId].push_back(message);
        applyMultipartUpdate(message.updateId);
        inProgressUpdates.erase(message.updateId);
        break;
}
```

### 7. Application API for Change Tracking

The application will need to mark which regions have changed:

```cpp
// Function to mark a specific field as changed
template<typename T>
void markFieldChanged(const char* memoryName, size_t fieldOffset) {
    void* sharedMem = getSharedMemory(memoryName);
    if (sharedMem) {
        markRegionChanged(sharedMem, fieldOffset, sizeof(T));
    }
}

// Example usage
void updateDataField(int newValue) {
    MemoryLayout* memory = static_cast<MemoryLayout*>(getSharedMemory(primary_memory_name.c_str()));
    if (memory) {
        // Update the data
        memory->data = newValue;
        memory->last_modified = GetTickCount();
        
        // Mark only the changed fields
        markFieldChanged<int>(primary_memory_name.c_str(), offsetof(MemoryLayout, data));
        markFieldChanged<uint64_t>(primary_memory_name.c_str(), offsetof(MemoryLayout, last_modified));
    }
}
```

## Implementation Details

### 1. Change Detection

Two approaches for change detection:

1. **Manual Tracking**: The application explicitly marks regions as changed
   - Pros: Precise control, potentially more efficient
   - Cons: Requires discipline from developers

2. **Automatic Tracking**: The system automatically detects changes
   - Pros: Easier to use, less error-prone
   - Cons: More overhead, may be less precise

Our implementation will support both approaches, with a preference for manual tracking for better performance.

### 2. Handling Large Changes

For changes larger than `MAX_SYNC_DATA_SIZE`:

1. Split the change into multiple chunks
2. Send a `MSG_START_UPDATE` message with the first chunk
3. Send one or more `MSG_UPDATE_CHUNK` messages with middle chunks
4. Send a `MSG_END_UPDATE` message with the final chunk

### 3. Timeout Handling

To handle cases where some parts of a multi-part update are lost:

```cpp
// Check for updates that have been in progress too long
for (auto& pair : inProgressUpdates) {
    if (currentTime - pair.second.startTime > UPDATE_TIMEOUT_MS) {
        // Discard the incomplete update
        inProgressUpdates.erase(pair.first);
    }
}
```

### 4. Thread Safety

All change tracking and update processing will be protected by mutexes:

```cpp
// Mutex for protecting the pendingChanges vector
HANDLE g_changesMutex = NULL;

// Mutex for protecting the inProgressUpdates map
HANDLE g_updatesMutex = NULL;

// Initialize mutexes
void initUpdateMutexes() {
    if (g_changesMutex == NULL) {
        g_changesMutex = CreateMutex(NULL, FALSE, NULL);
    }
    if (g_updatesMutex == NULL) {
        g_updatesMutex = CreateMutex(NULL, FALSE, NULL);
    }
}

// Lock/unlock functions
void lockChangesMutex() { WaitForSingleObject(g_changesMutex, INFINITE); }
void unlockChangesMutex() { ReleaseMutex(g_changesMutex); }
void lockUpdatesMutex() { WaitForSingleObject(g_updatesMutex, INFINITE); }
void unlockUpdatesMutex() { ReleaseMutex(g_updatesMutex); }
```

## Benefits

1. **Reduced Network Traffic**: Only sending changed data significantly reduces bandwidth usage
2. **Improved Scalability**: The system can handle much larger shared memory structures efficiently
3. **Better Performance**: Less data to transmit means lower latency and CPU usage
4. **Enhanced Reliability**: Multi-part update handling improves reliability for large changes

## Implementation Plan

1. **Update Message Structures**: Enhance `SyncMessage` with new fields
2. **Implement Change Tracking**: Add structures and functions for tracking changes
3. **Modify Sending Logic**: Update to send only changed regions
4. **Modify Receiving Logic**: Add support for reassembling multi-part updates
5. **Add Application API**: Provide functions for marking regions as changed
6. **Add Timeout Handling**: Implement detection and cleanup for incomplete updates
7. **Testing**: Verify with various change patterns and simulated network conditions

## Example Scenarios

### Scenario 1: Small Change to a Large Structure

A 4-byte integer is changed in a 1MB structure:

- **Current Implementation**: Sends the entire 1MB structure
- **Enhanced Implementation**: Sends only the 4-byte integer plus offset information

### Scenario 2: Multiple Small Changes

Several small fields are changed in different parts of the structure:

- **Current Implementation**: Sends the entire structure multiple times
- **Enhanced Implementation**: Sends only the changed fields with their offsets

### Scenario 3: Large Change Exceeding Message Size

A change larger than the maximum message size:

- **Current Implementation**: Fails or requires increasing the message size limit
- **Enhanced Implementation**: Splits into multiple messages with start/end markers

## Conclusion

The partial update system will significantly improve the efficiency and scalability of the Shared Memory Sync application, especially for large data structures. By sending only what has changed, with proper support for multi-part updates, the system can handle much larger shared memory regions while minimizing network traffic.
