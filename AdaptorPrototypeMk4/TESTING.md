# Testing Adaptor Prototype Mk4 on a Single Machine

## Overview

This document explains how to test the shared memory synchronization functionality on a single machine by running multiple instances of the application. The application is designed to be C++03 compatible and uses configuration files to set up the instances.

## Architecture for Testing

For testing on a single machine, we'll use the following approach:

1. Each instance of the application will create and manage multiple shared memory regions:
   - A "primary" region that it owns and writes to
   - "Secondary" regions that it monitors for changes made by other instances

2. The network synchronization will still be used, but all instances will run on localhost with different ports.

3. When an instance detects a change in its primary shared memory region, it will:
   - Broadcast the change to other instances via the network
   - Other instances will update their corresponding secondary shared memory regions

4. This simulates a distributed environment where each machine has its own primary shared memory and receives updates from other machines.

## Shared Memory Naming Convention

To avoid conflicts and clearly identify which instance owns which shared memory region, we'll use the following naming convention:

- Primary region: `AdaptorPrototypeMk4_<instance_id>`
- Secondary regions: `AdaptorPrototypeMk4_<other_instance_id>`

Where `<instance_id>` is a unique identifier for each instance (e.g., a number or a port number).

## Testing Procedure

1. Start the first instance using the default configuration file:
   ```
   AdaptorPrototypeMk4.exe
   ```
   This will load the default configuration from `sm_config.ini`.

2. Start the second instance with a specific configuration file:
   ```
   AdaptorPrototypeMk4.exe -c sm_config_instance2.ini
   ```
   This loads the configuration for instance 2, which connects to instance 1.

   Note: Make sure both configuration files are properly set up. The `sm_config.ini` file should have the remote_node line uncommented to connect to instance 2, and `sm_config_instance2.ini` should have a remote_node entry pointing to instance 1.

3. Each instance will:
   - Create its primary shared memory region
   - Connect to other instances and create secondary shared memory regions for them
   - Display a menu for interacting with the shared memory regions

4. Use the menu to:
   - Modify the primary shared memory region
   - View all shared memory regions
   - Monitor for changes in secondary regions

5. Verify that changes made to the primary region in one instance are reflected in the corresponding secondary region in other instances.

## Expected Behavior

1. When you modify the primary shared memory in Instance 1:
   - Instance 1 should log that it's updating its primary shared memory
   - Instance 2 should log that it received a network update
   - Instance 2 should update its secondary shared memory for Instance 1
   - Instance 2 should log the new values in the secondary shared memory

2. Similarly, when you modify the primary shared memory in Instance 2:
   - Instance 2 should log that it's updating its primary shared memory
   - Instance 1 should log that it received a network update
   - Instance 1 should update its secondary shared memory for Instance 2
   - Instance 1 should log the new values in the secondary shared memory

## Troubleshooting

If synchronization doesn't work as expected:

1. Check that both instances are running and connected to each other
2. Verify that the shared memory regions are created successfully
3. Check the network communication (ensure no firewall is blocking localhost communication)
4. Examine the logs for any error messages
5. Verify that the configuration files are correctly set up with the appropriate remote_node entries
6. If you're manually connecting instances using the menu option, make sure to use the correct IP, port, and instance ID
