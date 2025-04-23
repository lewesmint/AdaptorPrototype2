# Adaptor Prototype Mk4

This project implements a C++ Win32 application that utilizes shared memory to synchronize data across multiple machines. The application monitors a designated area of shared memory and communicates changes to other instances running on different machines, effectively creating a mirrored shared memory environment.

The code is compatible with C++03 standard, making it suitable for environments where C++11 or newer features are not available.

## Project Structure

```
AdaptorPrototypeMk4
├── src
│   ├── main.cpp               # Entry point of the application
│   ├── shared_memory.h        # Header for shared memory management
│   ├── shared_memory.cpp      # Implementation of shared memory functions
│   ├── network_sync.h         # Header for network synchronization
│   ├── network_sync.cpp       # Implementation of network synchronization functions
│   ├── memory_layout.h        # Defines C structs for shared memory layout
│   ├── sync_message.h         # Defines C structs for synchronization messages
│   ├── config.h               # Header for configuration management
│   ├── config.cpp             # Implementation of configuration functions
│   ├── change_tracking.h      # Header for partial update tracking
│   └── change_tracking.cpp    # Implementation of partial update functions
├── test
│   ├── test_shared_memory.cpp # Unit tests for shared memory functionality
│   ├── test_config.cpp        # Unit tests for configuration functionality
│   ├── test_partial_updates.cpp # Unit tests for partial updates functionality
│   └── CMakeLists.txt         # CMake configuration for tests
├── CMakeLists.txt             # CMake configuration file
├── README.md                  # Project documentation
├── TESTING.md                 # Testing instructions
├── PartialUpdates.md          # Documentation for partial updates feature
├── sm_config.ini              # Default configuration file for instance 1
└── sm_config_instance2.ini    # Configuration file for instance 2
```

## Setup Instructions

1. **Clone the Repository**:
   Clone this repository to your local machine using:
   ```
   git clone <repository-url>
   ```

2. **Build the Project**:
   Navigate to the project directory and create a build directory:
   ```
   cd AdaptorPrototypeMk4
   mkdir build
   cd build
   ```
   Run CMake to configure the project:
   ```
   cmake ..
   ```
   Build the project using:
   ```
   cmake --build .
   ```

3. **Run the Application**:
   After building, you can run the application from the build directory. Ensure that the necessary permissions for shared memory and network communication are granted.

## Usage

### Testing on a Single Machine

The application has been updated to support testing on a single machine by running multiple instances that communicate with each other. Each instance manages its own primary shared memory region and monitors secondary regions from other instances.

1. **Start the First Instance**:
   ```
   AdaptorPrototypeMk4.exe
   ```
   This starts an instance using the default configuration from `sm_config.ini`.

2. **Start the Second Instance**:
   ```
   AdaptorPrototypeMk4.exe -c sm_config_instance2.ini
   ```
   This starts an instance using the configuration from `sm_config_instance2.ini`, which connects to the first instance.

3. **Interact with the Instances**:
   - Use the interactive menu to update memory values, display memory state, or connect to other instances.
   - When you update the primary memory in one instance, the change should be reflected in the corresponding secondary memory in other instances.

For more detailed testing instructions, see [TESTING.md](TESTING.md).

### General Usage

- The application automatically creates and manages shared memory segments.
- It listens for changes in the shared memory and communicates these changes to other instances.
- Each instance has a unique ID and manages its own primary shared memory region.
- Instances can connect to each other to synchronize memory changes.
- Configuration is loaded from an INI file (default: `sm_config.ini`) which can be specified with the `-c` or `--config` command-line option.
- The configuration file specifies the local IP, port, instance ID, and remote nodes to connect to.

## Contributing

Contributions are welcome! Please feel free to submit a pull request or open an issue for any enhancements or bug fixes.

## License

This project is licensed under the MIT License. See the LICENSE file for more details.