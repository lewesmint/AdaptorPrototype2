#ifndef MEMORY_LAYOUT_H
#define MEMORY_LAYOUT_H

#include <stdint.h>

// Define the layout of the shared memory as used in main.cpp
typedef struct {
    uint64_t version;           // Version number that increments with each change
    int data;                   // Example data field
    uint64_t last_modified;     // Timestamp of last modification
    bool dirty;                 // Flag indicating if data has been modified
} MemoryLayout;

#endif // MEMORY_LAYOUT_H