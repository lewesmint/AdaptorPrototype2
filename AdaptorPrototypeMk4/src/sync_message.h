#ifndef SYNC_MESSAGE_H
#define SYNC_MESSAGE_H

#include <stdint.h>

#define MAX_MEMORY_NAME_LENGTH 64
#define MAX_SYNC_DATA_SIZE 1024

/**
 * @brief Message types for synchronization
 *
 * These types indicate the purpose of a synchronization message
 * and help with reassembling multi-part updates.
 */
typedef enum {
    MSG_SINGLE_UPDATE,   // Complete update in a single message
    MSG_START_UPDATE,    // Start of an update sequence
    MSG_UPDATE_CHUNK,    // Middle chunk of an update
    MSG_END_UPDATE       // End of an update sequence
} MessageType;

/**
 * @brief Synchronization message structure
 *
 * This structure contains all the information needed to synchronize
 * a portion of shared memory across the network.
 */
typedef struct {
    char memoryName[MAX_MEMORY_NAME_LENGTH]; // Name of the shared memory region
    MessageType msgType;                     // Type of message (single, start, chunk, end)
    uint64_t updateId;                       // Unique ID for multi-part updates
    size_t offset;                           // Offset within the shared memory
    size_t size;                             // Size of the data being synchronized
    uint32_t timestamp;                      // Timestamp of when the message was created
    char data[MAX_SYNC_DATA_SIZE];           // Data to be synchronized
} SyncMessage;

#endif // SYNC_MESSAGE_H