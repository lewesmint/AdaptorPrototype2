#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <csignal>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include "shared_memory.h"
#include "network_sync.h"
#include "memory_layout.h"
#include "config.h"
#include "change_tracking.h"

// Global variables
bool running = true;
int instance_id = 0;
std::string primary_memory_name;
std::map<int, std::string> secondary_memory_names;
HANDLE memory_names_mutex = NULL;

/**
 * Initialize the mutex for thread safety
 */
void initMemoryNamesMutex() {
    if (memory_names_mutex == NULL) {
        memory_names_mutex = CreateMutex(NULL, FALSE, NULL);
        if (memory_names_mutex == NULL) {
            std::cerr << "Failed to create memory names mutex: " << GetLastError() << std::endl;
        }
    }
}

/**
 * Acquire the memory_names mutex
 */
void lockMemoryNamesMutex() {
    if (memory_names_mutex != NULL) {
        WaitForSingleObject(memory_names_mutex, INFINITE);
    }
}

/**
 * Release the memory_names mutex
 */
void unlockMemoryNamesMutex() {
    if (memory_names_mutex != NULL) {
        ReleaseMutex(memory_names_mutex);
    }
}

/**
 * Signal handler for graceful shutdown
 *
 * @param signal The signal received (e.g., SIGINT)
 */
void signalHandler(int signal) {
    std::cout << "Signal " << signal << " received. Shutting down..." << std::endl;
    running = false;
}

/**
 * Callback function for memory updates
 * This is called when a shared memory region is modified
 *
 * @param memory_ptr Pointer to the memory that was updated
 */
void memoryUpdateCallback(void* memory_ptr) {
    MemoryLayout* layout = static_cast<MemoryLayout*>(memory_ptr);
    std::cout << "[MEMORY UPDATE] Memory updated: version=" << layout->version
              << ", data=" << layout->data << std::endl;
}

/**
 * Callback function for network updates
 * This is called when a network message is received
 *
 * @param memory_name Name of the shared memory region to update
 * @param offset Offset within the shared memory region
 * @param size Size of the data being updated
 */
void networkUpdateCallback(const char* memory_name, size_t offset, size_t size) {
    std::cout << "[NETWORK UPDATE] Received update for " << memory_name
              << " at offset " << offset
              << " with size " << size << std::endl;

    // After receiving a network update, we can check the updated memory
    void* mem = getSharedMemory(memory_name);
    if (mem) {
        MemoryLayout* layout = static_cast<MemoryLayout*>(mem);
        std::cout << "[NETWORK UPDATE] New values: version=" << layout->version
                  << ", data=" << layout->data << std::endl;
    }
}

/**
 * Creates a shared memory name for a specific instance
 *
 * @param id Instance ID
 * @return Shared memory name string
 */
std::string createMemoryName(int id) {
    std::ostringstream oss;
    oss << "AdaptorPrototypeMk4_" << id;
    return oss.str();
}

/**
 * Initializes a primary shared memory region for this instance
 *
 * @return true if successful, false otherwise
 */
bool initializePrimaryMemory() {
    primary_memory_name = createMemoryName(instance_id);
    std::cout << "[INIT] Creating primary shared memory: " << primary_memory_name << std::endl;

    if (!initializeSharedMemory(primary_memory_name.c_str(), sizeof(MemoryLayout))) {
        std::cerr << "[ERROR] Failed to initialize primary shared memory" << std::endl;
        return false;
    }

    // Get pointer to shared memory
    MemoryLayout* memory = static_cast<MemoryLayout*>(getSharedMemory(primary_memory_name.c_str()));
    if (!memory) {
        std::cerr << "[ERROR] Failed to get primary shared memory" << std::endl;
        return false;
    }

    // Initialize the memory
    memory->version = 1;
    memory->data = instance_id * 1000;  // Start with a value based on instance ID
    memory->last_modified = GetTickCount();  // Use GetTickCount instead of chrono
    memory->dirty = false;

    // Register memory change callback
    registerMemoryChangeCallback(primary_memory_name.c_str(), memoryUpdateCallback);

    // Start shared memory sync
    if (!startSharedMemorySync(primary_memory_name.c_str())) {
        std::cerr << "[ERROR] Failed to start shared memory sync" << std::endl;
        return false;
    }

    std::cout << "[INIT] Primary shared memory initialized successfully" << std::endl;
    return true;
}

/**
 * Initializes a secondary shared memory region for another instance
 *
 * @param other_id ID of the other instance
 * @return true if successful, false otherwise
 */
bool initializeSecondaryMemory(int other_id) {
    std::string memory_name = createMemoryName(other_id);

    // Initialize the mutex if needed
    initMemoryNamesMutex();

    // Lock the memory_names map to ensure thread safety
    lockMemoryNamesMutex();

    // Check if already initialized
    std::map<int, std::string>::iterator it = secondary_memory_names.find(other_id);
    if (it != secondary_memory_names.end()) {
        // Already initialized
        unlockMemoryNamesMutex();
        return true;
    }

    std::cout << "[INIT] Creating secondary shared memory: " << memory_name << std::endl;

    if (!initializeSharedMemory(memory_name.c_str(), sizeof(MemoryLayout))) {
        std::cerr << "[ERROR] Failed to initialize secondary shared memory for instance " << other_id << std::endl;
        unlockMemoryNamesMutex();
        return false;
    }

    // Get pointer to shared memory
    MemoryLayout* memory = static_cast<MemoryLayout*>(getSharedMemory(memory_name.c_str()));
    if (!memory) {
        std::cerr << "[ERROR] Failed to get secondary shared memory for instance " << other_id << std::endl;
        unlockMemoryNamesMutex();
        return false;
    }

    // Register memory change callback
    registerMemoryChangeCallback(memory_name.c_str(), memoryUpdateCallback);

    // Start shared memory sync
    if (!startSharedMemorySync(memory_name.c_str())) {
        std::cerr << "[ERROR] Failed to start shared memory sync for instance " << other_id << std::endl;
        unlockMemoryNamesMutex();
        return false;
    }

    secondary_memory_names[other_id] = memory_name;
    std::cout << "[INIT] Secondary shared memory for instance " << other_id << " initialized successfully" << std::endl;

    unlockMemoryNamesMutex();
    return true;
}

/**
 * Displays the current state of all shared memory regions
 */
void displayMemoryState() {
    std::cout << "\n===== SHARED MEMORY STATE =====" << std::endl;

    // Display primary memory
    MemoryLayout* primary = static_cast<MemoryLayout*>(getSharedMemory(primary_memory_name.c_str()));
    if (primary) {
        std::cout << "PRIMARY (" << primary_memory_name << "):" << std::endl;
        std::cout << "  Version: " << primary->version << std::endl;
        std::cout << "  Data: " << primary->data << std::endl;
        std::cout << "  Last Modified: " << primary->last_modified << std::endl;
        std::cout << "  Dirty: " << (primary->dirty ? "true" : "false") << std::endl;
    }

    // Display secondary memories
    // Initialize the mutex if needed
    initMemoryNamesMutex();

    // Lock the memory_names map to ensure thread safety
    lockMemoryNamesMutex();

    std::map<int, std::string>::iterator it;
    for (it = secondary_memory_names.begin(); it != secondary_memory_names.end(); ++it) {
        MemoryLayout* secondary = static_cast<MemoryLayout*>(getSharedMemory(it->second.c_str()));
        if (secondary) {
            std::cout << "SECONDARY (" << it->second << ") for instance " << it->first << ":" << std::endl;
            std::cout << "  Version: " << secondary->version << std::endl;
            std::cout << "  Data: " << secondary->data << std::endl;
            std::cout << "  Last Modified: " << secondary->last_modified << std::endl;
            std::cout << "  Dirty: " << (secondary->dirty ? "true" : "false") << std::endl;
        }
    }

    unlockMemoryNamesMutex();

    std::cout << "================================\n" << std::endl;
}

/**
 * Updates the primary shared memory with a new data value
 *
 * @param new_data The new data value to set
 */
void updatePrimaryMemory(int new_data) {
    MemoryLayout* memory = static_cast<MemoryLayout*>(getSharedMemory(primary_memory_name.c_str()));
    if (!memory) {
        std::cerr << "[ERROR] Failed to get primary shared memory for update" << std::endl;
        return;
    }

    // Update the memory
    memory->data = new_data;
    memory->last_modified = GetTickCount();  // Use GetTickCount instead of chrono

    // Mark the specific fields that changed
    markFieldChanged(primary_memory_name.c_str(), offsetof(MemoryLayout, data), sizeof(int));
    markFieldChanged(primary_memory_name.c_str(), offsetof(MemoryLayout, last_modified), sizeof(uint64_t));

    // Note: We don't need to manually increment version or set dirty flag
    // as markFieldChanged does this for us

    std::cout << "[UPDATE] Primary memory updated: version=" << memory->version
              << ", data=" << memory->data << std::endl;
}

/**
 * Displays the menu of available commands
 */
void displayMenu() {
    std::cout << "\nAVAILABLE COMMANDS:" << std::endl;
    std::cout << "  1. Update primary memory" << std::endl;
    std::cout << "  2. Display memory state" << std::endl;
    std::cout << "  3. Connect to another instance" << std::endl;
    std::cout << "  4. Exit" << std::endl;
    std::cout << "Enter command number: ";
}

/**
 * @brief Parse command-line arguments
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @param configPath Output parameter for the configuration file path
 * @return true if parsing was successful, false otherwise
 */
bool parseCommandLine(int argc, char* argv[], std::string& configPath) {
    // Default config file path
    configPath = "sm_config.ini";

    // Check for -c or --config option
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-c" || arg == "--config") {
            // Next argument should be the config file path
            if (i + 1 < argc) {
                configPath = argv[i + 1];
                i++; // Skip the next argument
            } else {
                std::cerr << "[ERROR] Missing config file path after " << arg << std::endl;
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Display usage information
 *
 * @param programName Name of the program executable
 */
void displayUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -c, --config <file>  Specify configuration file (default: sm_config.ini)" << std::endl;
    std::cout << std::endl;
    std::cout << "Configuration file format:" << std::endl;
    std::cout << "  local_ip = <ip>                  Local IP address" << std::endl;
    std::cout << "  local_port = <port>              Local port number" << std::endl;
    std::cout << "  instance_id = <id>                Instance ID" << std::endl;
    std::cout << "  remote_node = <ip>:<port>:<id>   Remote node to connect to" << std::endl;
    std::cout << std::endl;
    std::cout << "Example configuration file:" << std::endl;
    std::cout << "  local_ip = 127.0.0.1" << std::endl;
    std::cout << "  local_port = 8080" << std::endl;
    std::cout << "  instance_id = 1" << std::endl;
    std::cout << "  remote_node = 127.0.0.1:8081:2" << std::endl;
}

/**
 * Main function
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @return Exit code
 */
int main(int argc, char* argv[]) {
    // Register signal handler
    signal(SIGINT, signalHandler);

    // Parse command-line arguments
    std::string configPath;
    if (!parseCommandLine(argc, argv, configPath)) {
        displayUsage(argv[0]);
        return 1;
    }

    // Load configuration
    Config config;
    bool configLoaded = false;

    // Check if the config file exists
    FILE* file = fopen(configPath.c_str(), "r");
    if (file) {
        fclose(file);
        // Load configuration from file
        configLoaded = config.loadFromFile(configPath);
    } else {
        std::cerr << "[WARNING] Configuration file not found: " << configPath << std::endl;

        // If it's the default config file, we can continue with default values
        if (configPath != "sm_config.ini") {
            std::cerr << "[ERROR] Specified configuration file not found" << std::endl;
            displayUsage(argv[0]);
            return 1;
        }

        std::cout << "[INFO] Using default configuration" << std::endl;
        configLoaded = true; // Use default values in the Config constructor
    }

    if (!configLoaded || !config.isValid()) {
        std::cerr << "[ERROR] Invalid configuration" << std::endl;
        displayUsage(argv[0]);
        return 1;
    }

    // Display the configuration
    std::cout << config.toString();

    // Set global variables from configuration
    instance_id = config.getInstanceId();
    const std::string& local_ip = config.getLocalIp();
    int local_port = config.getLocalPort();

    std::cout << "[INIT] Starting instance " << instance_id << " on " << local_ip << ":" << local_port << std::endl;

    // Initialize primary shared memory
    if (!initializePrimaryMemory()) {
        std::cerr << "[ERROR] Failed to initialize primary shared memory" << std::endl;
        return 1;
    }

    // Initialize network sync
    if (!initNetworkSync(local_ip.c_str(), local_port)) {
        std::cerr << "[ERROR] Failed to initialize network sync" << std::endl;
        cleanupSharedMemory(primary_memory_name.c_str());
        return 1;
    }

    // Register network update callback
    registerNetworkUpdateCallback(networkUpdateCallback);

    // Connect to remote nodes from configuration
    const std::vector<Config::RemoteNode>& remoteNodes = config.getRemoteNodes();
    for (size_t i = 0; i < remoteNodes.size(); ++i) {
        const Config::RemoteNode& node = remoteNodes[i];
        const std::string& remote_ip = node.ip;
        int remote_port = node.port;
        int remote_instance_id = node.instanceId;

        std::cout << "[INIT] Connecting to remote instance " << remote_instance_id
                  << " at " << remote_ip << ":" << remote_port << std::endl;

        // Initialize secondary memory for the remote instance
        if (!initializeSecondaryMemory(remote_instance_id)) {
            std::cerr << "[ERROR] Failed to initialize secondary memory for remote instance" << std::endl;
            // Continue anyway, as this is not critical
        }

        // Connect to remote node
        if (!connectToRemoteNode(remote_ip.c_str(), remote_port)) {
            std::cerr << "[WARNING] Failed to connect to remote node" << std::endl;
            // Continue anyway, they might connect to us later
        }
    }

    std::cout << "[INIT] Initialization complete. Starting interactive mode." << std::endl;

    // Main interactive loop
    std::string input;
    int command;

    while (running) {
        displayMenu();
        std::getline(std::cin, input);

        // Convert input to integer
        command = atoi(input.c_str());
        if (command == 0 && input != "0") {
            std::cout << "Invalid command. Please enter a number." << std::endl;
            continue;
        }

        switch (command) {
            case 1: { // Update primary memory
                std::cout << "Enter new data value: ";
                std::getline(std::cin, input);
                // Convert input to integer
                int new_data = atoi(input.c_str());
                if (new_data == 0 && input != "0") {
                    std::cout << "Invalid data value. Please enter a number." << std::endl;
                } else {
                    updatePrimaryMemory(new_data);
                }
                break;
            }

            case 2: // Display memory state
                displayMemoryState();
                break;

            case 3: { // Connect to another instance
                std::cout << "Enter remote IP: ";
                std::string remote_ip;
                std::getline(std::cin, remote_ip);

                std::cout << "Enter remote port: ";
                std::getline(std::cin, input);
                // Convert input to integer
                int remote_port = atoi(input.c_str());
                if (remote_port == 0) {
                    std::cout << "Invalid port number." << std::endl;
                    break;
                }

                std::cout << "Enter remote instance ID: ";
                std::getline(std::cin, input);

                // Convert input to integer
                int remote_instance_id = atoi(input.c_str());
                if (remote_instance_id == 0) {
                    std::cout << "Invalid instance ID." << std::endl;
                    break;
                }

                // Initialize secondary memory for the remote instance
                if (!initializeSecondaryMemory(remote_instance_id)) {
                    std::cerr << "[ERROR] Failed to initialize secondary memory for remote instance" << std::endl;
                    break;
                }

                // Connect to remote node
                if (!connectToRemoteNode(remote_ip.c_str(), remote_port)) {
                    std::cerr << "[WARNING] Failed to connect to remote node" << std::endl;
                }
                break;
            }

            case 4: // Exit
                running = false;
                break;

            default:
                std::cout << "Unknown command." << std::endl;
                break;
        }
    }

    // Clean up
    std::cout << "[CLEANUP] Stopping shared memory sync..." << std::endl;

    // Stop primary memory sync
    stopSharedMemorySync(primary_memory_name.c_str());
    cleanupSharedMemory(primary_memory_name.c_str());

    // Stop secondary memory syncs
    // Initialize the mutex if needed
    initMemoryNamesMutex();

    // Lock the memory_names map to ensure thread safety
    lockMemoryNamesMutex();

    std::map<int, std::string>::iterator it;
    for (it = secondary_memory_names.begin(); it != secondary_memory_names.end(); ++it) {
        stopSharedMemorySync(it->second.c_str());
        cleanupSharedMemory(it->second.c_str());
    }

    unlockMemoryNamesMutex();

    // Clean up mutex
    if (memory_names_mutex != NULL) {
        CloseHandle(memory_names_mutex);
        memory_names_mutex = NULL;
    }

    // Shutdown network
    shutdownNetworkSync();

    std::cout << "[CLEANUP] Application exited cleanly" << std::endl;
    return 0;
}