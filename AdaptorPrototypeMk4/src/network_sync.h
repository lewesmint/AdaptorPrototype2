#ifndef NETWORK_SYNC_H
#define NETWORK_SYNC_H

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include "sync_message.h"

// Function to initialize network synchronization
bool initNetworkSync(const char* ip_address, int port);

// Function to connect to a remote node
bool connectToRemoteNode(const char* ip_address, int port);

// Function to start shared memory synchronization
bool startSharedMemorySync(const char* memory_name);

// Function to stop shared memory synchronization
void stopSharedMemorySync(const char* memory_name);

// Function to shutdown network synchronization
void shutdownNetworkSync();

// Callback for network updates
typedef void (*NetworkUpdateCallback)(const char* memory_name, size_t offset, size_t size);

// Function to register a callback for network updates
bool registerNetworkUpdateCallback(NetworkUpdateCallback callback);

// Global callback function for network updates
extern NetworkUpdateCallback g_networkCallback;

// Function to send a synchronization message
bool sendSyncMessage(SOCKET sock, const char* ipAddress, int port, const SyncMessage& message);

#endif // NETWORK_SYNC_H