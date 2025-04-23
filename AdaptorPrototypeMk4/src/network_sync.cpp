/**
 * @file network_sync.cpp
 * @brief Implementation of network synchronization functions
 *
 * This file contains the implementation of functions for synchronizing shared memory
 * across multiple processes or machines using network communication. It provides
 * functionality for sending and receiving synchronization messages, connecting to
 * remote nodes, and monitoring shared memory for changes.
 */

// Make sure winsock2.h is included before windows.h to avoid conflicts
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "network_sync.h"
#include "sync_message.h"
#include "shared_memory.h"
#include "memory_layout.h"
#include "change_tracking.h"
#include <iostream>
#include <map>
#include <string>
#include <process.h>  // For _beginthreadex

/**
 * @brief Global variables for network synchronization
 *
 * These variables maintain the state of the network synchronization system.
 * They are used by various functions to track connections, threads, and callbacks.
 */

/// Socket used for sending and receiving synchronization messages
static SOCKET g_socket = INVALID_SOCKET;

/// IP address of the local node
static std::string g_localIp;

/// Port number of the local node
static int g_localPort;

/// Map of remote nodes (key: "ip:port", value: "ip:port")
static std::map<std::string, std::string> g_remoteNodes;

/// Mutex to protect access to the g_remoteNodes map
static HANDLE g_remoteNodesMutex = NULL;

/// Thread handle that receives synchronization messages from the network
static HANDLE g_receiveThread = NULL;

/// Flag indicating whether the synchronization system is running
static volatile bool g_running = false;

/// Callback function to invoke when a network update is received
NetworkUpdateCallback g_networkCallback = NULL;

/// Map of threads that monitor shared memory regions for changes (key: memory name, value: thread handle)
static std::map<std::string, HANDLE> g_syncThreads;

/// Mutex to protect access to the g_syncThreads map
static HANDLE g_syncThreadsMutex = NULL;

/**
 * @brief Initialize the mutexes for thread safety
 *
 * This function initializes the mutexes used to protect the g_remoteNodes and g_syncThreads maps.
 * It should be called before any other network synchronization functions.
 */
void initNetworkMutexes() {
    if (g_remoteNodesMutex == NULL) {
        g_remoteNodesMutex = CreateMutex(NULL, FALSE, NULL);
        if (g_remoteNodesMutex == NULL) {
            std::cerr << "Failed to create remote nodes mutex: " << GetLastError() << std::endl;
        }
    }

    if (g_syncThreadsMutex == NULL) {
        g_syncThreadsMutex = CreateMutex(NULL, FALSE, NULL);
        if (g_syncThreadsMutex == NULL) {
            std::cerr << "Failed to create sync threads mutex: " << GetLastError() << std::endl;
        }
    }
}

/**
 * @brief Acquire the g_remoteNodes mutex
 *
 * This function acquires the mutex used to protect the g_remoteNodes map.
 * It should be called before accessing the map.
 */
void lockRemoteNodesMutex() {
    if (g_remoteNodesMutex != NULL) {
        WaitForSingleObject(g_remoteNodesMutex, INFINITE);
    }
}

/**
 * @brief Release the g_remoteNodes mutex
 *
 * This function releases the mutex used to protect the g_remoteNodes map.
 * It should be called after accessing the map.
 */
void unlockRemoteNodesMutex() {
    if (g_remoteNodesMutex != NULL) {
        ReleaseMutex(g_remoteNodesMutex);
    }
}

/**
 * @brief Acquire the g_syncThreads mutex
 *
 * This function acquires the mutex used to protect the g_syncThreads map.
 * It should be called before accessing the map.
 */
void lockSyncThreadsMutex() {
    if (g_syncThreadsMutex != NULL) {
        WaitForSingleObject(g_syncThreadsMutex, INFINITE);
    }
}

/**
 * @brief Release the g_syncThreads mutex
 *
 * This function releases the mutex used to protect the g_syncThreads map.
 * It should be called after accessing the map.
 */
void unlockSyncThreadsMutex() {
    if (g_syncThreadsMutex != NULL) {
        ReleaseMutex(g_syncThreadsMutex);
    }
}

/**
 * @brief Helper functions for network communication
 *
 * These functions provide low-level support for network operations such as
 * initializing Winsock, creating sockets, and sending/receiving messages.
 */

/**
 * @brief Initializes the Windows Socket API (Winsock)
 *
 * This function initializes Winsock version 2.2, which is required for
 * network communication in Windows.
 *
 * @return true if initialization was successful, false otherwise
 */
bool initializeWinsock() {
    WSADATA wsaData;
    // Initialize Winsock version 2.2
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

/**
 * @brief Creates a UDP socket for network communication
 *
 * This function creates a UDP socket that can be used to send and receive
 * synchronization messages over the network.
 *
 * @return A new socket handle, or INVALID_SOCKET if creation failed
 */
SOCKET createSocket() {
    // Create a UDP socket (SOCK_DGRAM) using IPv4 (AF_INET) and the UDP protocol (IPPROTO_UDP)
    return socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

/**
 * @brief Binds a socket to a local IP address and port
 *
 * This function binds a socket to a specific local IP address and port number,
 * allowing it to receive messages sent to that address and port.
 *
 * @param sock The socket to bind
 * @param ipAddress The local IP address to bind to
 * @param port The local port number to bind to
 * @return true if binding was successful, false otherwise
 */
bool bindSocket(SOCKET sock, const char* ipAddress, int port) {
    // Create a sockaddr_in structure with the local address information
    sockaddr_in localAddr;
    localAddr.sin_family = AF_INET;  // IPv4 address family
    localAddr.sin_port = htons(port);  // Convert port to network byte order

    // Convert IP address string to binary form
    inet_pton(AF_INET, ipAddress, &localAddr.sin_addr);

    // Bind the socket to the local address
    // Return true if binding succeeded (bind() returns 0 on success, SOCKET_ERROR on failure)
    return bind(sock, reinterpret_cast<sockaddr*>(&localAddr), sizeof(localAddr)) != SOCKET_ERROR;
}

/**
 * @brief Sends a synchronization message to a remote node
 *
 * This function sends a SyncMessage structure to a specific IP address and port
 * using a UDP socket.
 *
 * @param sock The socket to send from
 * @param ipAddress The destination IP address
 * @param port The destination port number
 * @param message The synchronization message to send
 * @return true if sending was successful, false otherwise
 */
bool sendSyncMessage(SOCKET sock, const char* ipAddress, int port, const SyncMessage& message) {
    // Create a sockaddr_in structure with the destination address information
    sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;  // IPv4 address family
    destAddr.sin_port = htons(port);  // Convert port to network byte order

    // Convert IP address string to binary form
    inet_pton(AF_INET, ipAddress, &destAddr.sin_addr);

    // Send the message to the destination address
    // We need to cast the message to a char* for the sendto() function
    int result = sendto(sock, reinterpret_cast<const char*>(&message), sizeof(message), 0,
                        reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr));

    // Return true if sending succeeded (sendto() returns the number of bytes sent on success, SOCKET_ERROR on failure)
    return result != SOCKET_ERROR;
}

/**
 * @brief Receives a synchronization message from the network
 *
 * This function waits for a synchronization message to arrive on a socket and
 * returns the message along with the source IP address and port.
 *
 * @param sock The socket to receive on
 * @param message Reference to a SyncMessage structure to store the received message
 * @param sourceIp Reference to a string to store the source IP address
 * @param sourcePort Reference to an int to store the source port number
 * @return true if receiving was successful, false otherwise
 */
bool receiveSyncMessage(SOCKET sock, SyncMessage& message, std::string& sourceIp, int& sourcePort) {
    // Create a sockaddr_in structure to store the source address information
    sockaddr_in srcAddr;
    int addrLen = sizeof(srcAddr);

    // Receive a message from the socket
    // We need to cast the message to a char* for the recvfrom() function
    int result = recvfrom(sock, reinterpret_cast<char*>(&message), sizeof(message), 0,
                          reinterpret_cast<sockaddr*>(&srcAddr), &addrLen);

    // If receiving succeeded (recvfrom() returns the number of bytes received on success, SOCKET_ERROR on failure)
    if (result != SOCKET_ERROR) {
        // Convert the source IP address from binary to string form
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(srcAddr.sin_addr), ipStr, INET_ADDRSTRLEN);

        // Store the source IP address and port
        sourceIp = ipStr;
        sourcePort = ntohs(srcAddr.sin_port);  // Convert port from network byte order
        return true;
    }
    return false;
}

/**
 * @brief Cleans up the Windows Socket API (Winsock)
 *
 * This function cleans up Winsock resources when they are no longer needed.
 * It should be called when the application is shutting down.
 */
void cleanupWinsock() {
    // Clean up Winsock resources
    WSACleanup();
}

/**
 * @brief Thread function for receiving synchronization messages
 *
 * This function runs in a separate thread and continuously listens for
 * synchronization messages from the network. When a message is received,
 * it updates the corresponding shared memory region and invokes the
 * registered callback function.
 *
 * The thread continues running until the g_running flag is set to false.
 *
 * @param arg Thread argument (not used)
 * @return Thread exit code
 */
unsigned int __stdcall receiveThreadFunc(void* arg) {
    // Variables to store the received message and source address
    SyncMessage message;
    std::string sourceIp;
    int sourcePort;

    // Continue receiving messages until the g_running flag is set to false
    while (g_running) {
        // Try to receive a synchronization message
        if (receiveSyncMessage(g_socket, message, sourceIp, sourcePort)) {
            // We received a message, process it based on message type
            switch (message.msgType) {
                case MSG_SINGLE_UPDATE:
                    // Apply the change immediately
                    applyUpdate(message);
                    break;

                case MSG_START_UPDATE:
                    // Start tracking a new multi-part update
                    lockUpdatesMutex();
                    {
                        UpdateInfo& info = g_inProgressUpdates[message.updateId];
                        info.updateId = message.updateId;
                        info.chunks.push_back(message);
                        info.startTime = GetTickCount64();
                    }
                    unlockUpdatesMutex();
                    break;

                case MSG_UPDATE_CHUNK:
                    // Add to an existing update
                    lockUpdatesMutex();
                    {
                        std::map<uint64_t, UpdateInfo>::iterator it =
                            g_inProgressUpdates.find(message.updateId);
                        if (it != g_inProgressUpdates.end()) {
                            it->second.chunks.push_back(message);
                        } else {
                            // We missed the start message, discard this chunk
                            std::cerr << "Received chunk for unknown update ID: "
                                      << message.updateId << std::endl;
                        }
                    }
                    unlockUpdatesMutex();
                    break;

                case MSG_END_UPDATE:
                    // Add the final chunk and apply the complete update
                    lockUpdatesMutex();
                    {
                        std::map<uint64_t, UpdateInfo>::iterator it =
                            g_inProgressUpdates.find(message.updateId);
                        if (it != g_inProgressUpdates.end()) {
                            it->second.chunks.push_back(message);
                            applyMultipartUpdate(message.updateId);
                            g_inProgressUpdates.erase(it);
                        } else {
                            // We missed the start message, try to apply just this chunk
                            std::cerr << "Received end for unknown update ID: "
                                      << message.updateId << std::endl;
                            applyUpdate(message);
                        }
                    }
                    unlockUpdatesMutex();
                    break;
            }

            // Check for timed-out updates
            checkUpdateTimeouts();
        }

        // Sleep briefly to avoid consuming too much CPU
        // This determines how quickly we respond to incoming messages (10ms latency here)
        Sleep(10);
    }
    // When g_running is set to false, this thread will exit
    return 0;
}

// Thread data structure for memory sync thread
struct MemorySyncThreadData {
    std::string memoryName;
};

/**
 * @brief Thread function for monitoring and synchronizing shared memory
 *
 * This function runs in a separate thread and continuously monitors a shared
 * memory region for changes. When it detects a change (version number increase
 * and dirty flag set), it creates synchronization messages for the changed regions
 * and sends them to all connected remote nodes.
 *
 * The thread continues running until the g_running flag is set to false.
 *
 * @param arg Pointer to a MemorySyncThreadData structure containing the memory name
 * @return Thread exit code
 */
unsigned int __stdcall memorySyncThreadFunc(void* arg) {
    // Extract the memory name from the thread data
    MemorySyncThreadData* data = static_cast<MemorySyncThreadData*>(arg);
    std::string memoryName = data->memoryName;
    delete data;  // Free the thread data

    // Get a pointer to the shared memory region
    void* sharedMem = getSharedMemory(memoryName.c_str());
    if (!sharedMem) {
        // If we can't access the shared memory, exit the thread
        return 0;
    }

    // Cast the memory pointer to our expected structure type
    MemoryLayout* layout = static_cast<MemoryLayout*>(sharedMem);

    // Remember the current version to detect changes
    uint64_t lastVersion = layout->version;

    // Continue monitoring until the g_running flag is set to false
    while (g_running) {
        // Check if the memory has changed (version increased) and is marked as dirty
        if (layout->version > lastVersion && layout->dirty) {
            // Check if there are pending changes to send
            lockChangesMutex();
            std::map<std::string, std::vector<MemoryChange> >::iterator changeIt =
                g_pendingChanges.find(memoryName);

            if (changeIt != g_pendingChanges.end() && !changeIt->second.empty()) {
                // We have specific changes to send
                std::vector<MemoryChange>& changes = changeIt->second;

                // Generate a unique update ID for this batch
                uint64_t updateId = generateUniqueId();

                // Determine if we need multiple messages
                bool multipleMessages = (changes.size() > 1);

                for (size_t i = 0; i < changes.size(); i++) {
                    // Create a synchronization message
                    SyncMessage message;

                    // Set the message type based on position in sequence
                    if (multipleMessages) {
                        if (i == 0) {
                            message.msgType = MSG_START_UPDATE;
                        } else if (i == changes.size() - 1) {
                            message.msgType = MSG_END_UPDATE;
                        } else {
                            message.msgType = MSG_UPDATE_CHUNK;
                        }
                    } else {
                        message.msgType = MSG_SINGLE_UPDATE;
                    }

                    // Set the update ID
                    message.updateId = updateId;

                    // Copy the memory name
                    strncpy(message.memoryName, memoryName.c_str(), sizeof(message.memoryName) - 1);
                    message.memoryName[sizeof(message.memoryName) - 1] = '\0';

                    // Set the offset and size for this chunk
                    message.offset = changes[i].offset;
                    message.size = changes[i].size;

                    // Set the timestamp
                    message.timestamp = GetTickCount();

                    // Copy just the changed data
                    char* source = static_cast<char*>(sharedMem) + message.offset;
                    memcpy(message.data, source, message.size);

                    // Send the message to all remote nodes
                    lockRemoteNodesMutex();
                    std::map<std::string, std::string>::iterator it;
                    for (it = g_remoteNodes.begin(); it != g_remoteNodes.end(); ++it) {
                        // Parse the IP address and port from the node value
                        // The format is "ip:port"
                        size_t colonPos = it->second.find(':');
                        if (colonPos != std::string::npos) {
                            // Extract the IP address (everything before the colon)
                            std::string ip = it->second.substr(0, colonPos);

                            // Extract the port number (everything after the colon)
                            int port = atoi(it->second.substr(colonPos + 1).c_str());

                            // Send the synchronization message to this node
                            sendSyncMessage(g_socket, ip.c_str(), port, message);
                        }
                    }
                    unlockRemoteNodesMutex();
                }

                // Clear the pending changes
                changes.clear();
            } else {
                // No specific changes tracked, send the whole structure (fallback)
                SyncMessage message;

                // Set message type and update ID
                message.msgType = MSG_SINGLE_UPDATE;
                message.updateId = generateUniqueId();

                // Copy the memory name to the message, ensuring it's null-terminated
                strncpy(message.memoryName, memoryName.c_str(), sizeof(message.memoryName) - 1);
                message.memoryName[sizeof(message.memoryName) - 1] = '\0';

                // Set the offset and size for the whole structure
                message.offset = 0;
                message.size = sizeof(MemoryLayout);

                // Set the timestamp
                message.timestamp = GetTickCount();

                // Copy the shared memory data to the message
                memcpy(message.data, sharedMem, message.size);

                // Send the message to all remote nodes
                lockRemoteNodesMutex();
                std::map<std::string, std::string>::iterator it;
                for (it = g_remoteNodes.begin(); it != g_remoteNodes.end(); ++it) {
                    // Parse the IP address and port from the node value
                    // The format is "ip:port"
                    size_t colonPos = it->second.find(':');
                    if (colonPos != std::string::npos) {
                        // Extract the IP address (everything before the colon)
                        std::string ip = it->second.substr(0, colonPos);

                        // Extract the port number (everything after the colon)
                        int port = atoi(it->second.substr(colonPos + 1).c_str());

                        // Send the synchronization message to this node
                        sendSyncMessage(g_socket, ip.c_str(), port, message);
                    }
                }
                unlockRemoteNodesMutex();
            }
            unlockChangesMutex();

            // Update our last known version
            lastVersion = layout->version;

            // Clear the dirty flag since we've synchronized the changes
            layout->dirty = false;
        }

        // Sleep briefly to avoid consuming too much CPU
        // This determines how quickly we detect and synchronize changes (10ms latency here)
        Sleep(10);
    }
    // When g_running is set to false, this thread will exit
    return 0;
}

/**
 * @brief Public API implementation
 *
 * The following functions provide the public API for network synchronization.
 * They allow the application to initialize the network, connect to remote nodes,
 * start and stop synchronization for shared memory regions, and register callbacks.
 */

/**
 * @brief Initializes the network synchronization system
 *
 * This function initializes Winsock, creates a socket, binds it to the specified
 * IP address and port, and starts a thread to receive synchronization messages.
 * It must be called before any other network synchronization functions.
 *
 * @param ip_address The local IP address to bind to (e.g., "127.0.0.1" for localhost)
 * @param port The local port number to bind to (e.g., 8080)
 * @return true if initialization was successful, false otherwise
 */
bool initNetworkSync(const char* ip_address, int port) {
    // Initialize the mutexes
    initNetworkMutexes();

    // Initialize change tracking
    initChangeTracking();

    // Step 1: Initialize the Windows Socket API (Winsock)
    if (!initializeWinsock()) {
        std::cerr << "Failed to initialize Winsock" << std::endl;
        return false;
    }

    // Step 2: Create a UDP socket for sending and receiving messages
    g_socket = createSocket();
    if (g_socket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        cleanupWinsock();
        return false;
    }

    // Step 3: Bind the socket to the specified local IP address and port
    if (!bindSocket(g_socket, ip_address, port)) {
        std::cerr << "Failed to bind socket" << std::endl;
        closesocket(g_socket);
        cleanupWinsock();
        return false;
    }

    // Step 4: Store the local address information for later use
    g_localIp = ip_address;
    g_localPort = port;

    // Step 5: Start the receive thread to listen for incoming messages
    g_running = true;  // Set the running flag to true
    unsigned int threadId;
    g_receiveThread = (HANDLE)_beginthreadex(
        NULL,                   // Default security attributes
        0,                      // Default stack size
        receiveThreadFunc,      // Thread function
        NULL,                   // Thread argument
        0,                      // Default creation flags
        &threadId               // Thread identifier
    );

    if (g_receiveThread == NULL) {
        std::cerr << "Failed to create receive thread: " << GetLastError() << std::endl;
        closesocket(g_socket);
        cleanupWinsock();
        g_running = false;
        return false;
    }

    return true;
}

/**
 * @brief Connects to a remote node for synchronization
 *
 * This function adds a remote node to the list of nodes that will receive
 * synchronization messages when shared memory changes. It also sends a test
 * message to verify that the connection works.
 *
 * @param ip_address The IP address of the remote node
 * @param port The port number of the remote node
 * @return true if the connection was successful, false otherwise
 */
// Helper function to convert int to string (C++03 compatible)
std::string to_string(int value) {
    char buffer[32];
    sprintf(buffer, "%d", value);
    return std::string(buffer);
}

bool connectToRemoteNode(const char* ip_address, int port) {
    // Create a key and value for the remote node
    // Both are in the format "ip:port"
    std::string nodeKey = std::string(ip_address) + ":" + to_string(port);
    std::string nodeValue = std::string(ip_address) + ":" + to_string(port);

    // Add the remote node to our map
    lockRemoteNodesMutex();
    g_remoteNodes[nodeKey] = nodeValue;
    unlockRemoteNodesMutex();

    // Send a test message to verify that we can reach the remote node
    SyncMessage testMsg;
    strcpy(testMsg.memoryName, "TEST");  // Special name for test messages
    testMsg.offset = 0;
    testMsg.size = 0;

    // Return true if the test message was sent successfully
    return sendSyncMessage(g_socket, ip_address, port, testMsg);
}

/**
 * @brief Starts synchronization for a shared memory region
 *
 * This function starts a thread that monitors a shared memory region for changes
 * and sends synchronization messages to remote nodes when changes are detected.
 *
 * @param memory_name The name of the shared memory region to synchronize
 * @return true if synchronization was started successfully, false otherwise
 */
bool startSharedMemorySync(const char* memory_name) {
    // Convert the memory name to a string for easier handling
    std::string memName(memory_name);

    // Check if we're already synchronizing this memory region
    lockSyncThreadsMutex();
    std::map<std::string, HANDLE>::iterator it = g_syncThreads.find(memName);
    if (it != g_syncThreads.end()) {
        // Already syncing this memory, nothing to do
        unlockSyncThreadsMutex();
        return true;
    }

    // Create thread data
    MemorySyncThreadData* data = new MemorySyncThreadData();
    data->memoryName = memName;

    // Start a new thread to monitor and synchronize this memory region
    unsigned int threadId;
    HANDLE threadHandle = (HANDLE)_beginthreadex(
        NULL,                   // Default security attributes
        0,                      // Default stack size
        memorySyncThreadFunc,   // Thread function
        data,                   // Thread argument
        0,                      // Default creation flags
        &threadId               // Thread identifier
    );

    if (threadHandle == NULL) {
        std::cerr << "Failed to create memory sync thread: " << GetLastError() << std::endl;
        delete data;
        unlockSyncThreadsMutex();
        return false;
    }

    // Store the thread handle in our map
    g_syncThreads[memName] = threadHandle;
    unlockSyncThreadsMutex();
    return true;
}

/**
 * @brief Stops synchronization for a shared memory region
 *
 * This function stops the thread that monitors a shared memory region for changes.
 * The region will no longer be synchronized with remote nodes.
 *
 * @param memory_name The name of the shared memory region to stop synchronizing
 */
void stopSharedMemorySync(const char* memory_name) {
    // Convert the memory name to a string for easier handling
    std::string memName(memory_name);

    // Find the synchronization thread for this memory region
    lockSyncThreadsMutex();
    std::map<std::string, HANDLE>::iterator it = g_syncThreads.find(memName);
    if (it != g_syncThreads.end()) {
        // We found the thread, now we need to stop and clean it up
        // Note: The thread will exit when g_running is false, but we don't want to
        // stop all threads, so we'll just terminate this one specifically

        // Terminate the thread (not ideal, but we don't have a better way in this implementation)
        TerminateThread(it->second, 0);

        // Clean up the thread handle
        CloseHandle(it->second);

        // Remove the thread from our map
        g_syncThreads.erase(it);
    }
    unlockSyncThreadsMutex();
    // If the memory region isn't being synchronized, there's nothing to do
}

/**
 * @brief Shuts down the network synchronization system
 *
 * This function stops all synchronization threads, closes the socket, and
 * cleans up Winsock resources. It should be called when the application is
 * shutting down or when network synchronization is no longer needed.
 */
void shutdownNetworkSync() {
    // Step 1: Stop all threads by setting the running flag to false
    g_running = false;

    // Clean up change tracking
    cleanupChangeTracking();

    // Step 2: Wait for the receive thread to finish and clean it up
    if (g_receiveThread) {
        WaitForSingleObject(g_receiveThread, INFINITE);
        CloseHandle(g_receiveThread);
        g_receiveThread = NULL;
    }

    // Step 3: Wait for all synchronization threads to finish and clean them up
    lockSyncThreadsMutex();
    std::map<std::string, HANDLE>::iterator it;
    for (it = g_syncThreads.begin(); it != g_syncThreads.end(); ++it) {
        WaitForSingleObject(it->second, INFINITE);
        CloseHandle(it->second);
    }
    g_syncThreads.clear();
    unlockSyncThreadsMutex();

    // Step 4: Close the socket if it's open
    if (g_socket != INVALID_SOCKET) {
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
    }

    // Step 5: Clean up Winsock resources
    cleanupWinsock();

    // Step 6: Clean up mutexes
    if (g_remoteNodesMutex) {
        CloseHandle(g_remoteNodesMutex);
        g_remoteNodesMutex = NULL;
    }

    if (g_syncThreadsMutex) {
        CloseHandle(g_syncThreadsMutex);
        g_syncThreadsMutex = NULL;
    }
}

/**
 * @brief Registers a callback function for network updates
 *
 * This function sets up a callback function that will be called whenever a
 * synchronization message is received from the network. The callback can be
 * used to perform additional processing or logging when updates occur.
 *
 * @param callback The function to call when a network update is received
 * @return true if the callback was registered successfully, false otherwise
 */
bool registerNetworkUpdateCallback(NetworkUpdateCallback callback) {
    // Set the global callback function
    g_networkCallback = callback;
    return true;
}