#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include <unordered_map>

/**
 * @brief Configuration class for shared memory sync application
 *
 * This class handles loading and parsing configuration from a file.
 * It supports both automatic loading of the default config file (sm_config.ini)
 * and loading from a specified file path.
 */
class Config {
public:
    /**
     * @brief Structure to represent a remote node
     */
    struct RemoteNode {
        std::string ip;
        int port;
        int instanceId;

        RemoteNode(const std::string& _ip, int _port, int _instanceId)
            : ip(_ip), port(_port), instanceId(_instanceId) {}
    };

    /**
     * @brief Default constructor
     *
     * Initializes the configuration with default values.
     */
    Config();

    /**
     * @brief Load configuration from a file
     *
     * @param filePath Path to the configuration file
     * @return true if loading was successful, false otherwise
     */
    bool loadFromFile(const std::string& filePath);

    /**
     * @brief Get the local IP address
     *
     * @return Local IP address
     */
    std::string getLocalIp() const { return localIp; }

    /**
     * @brief Get the local port
     *
     * @return Local port
     */
    int getLocalPort() const { return localPort; }

    /**
     * @brief Get the instance ID
     *
     * @return Instance ID
     */
    int getInstanceId() const { return instanceId; }

    /**
     * @brief Get the list of remote nodes
     *
     * @return Vector of remote nodes
     */
    const std::vector<RemoteNode>& getRemoteNodes() const { return remoteNodes; }

    /**
     * @brief Check if the configuration is valid
     *
     * @return true if the configuration is valid, false otherwise
     */
    bool isValid() const;

    /**
     * @brief Get a string representation of the configuration
     *
     * @return String representation of the configuration
     */
    std::string toString() const;

private:
    // Local node configuration
    std::string localIp;
    int localPort;
    int instanceId;

    // Remote nodes configuration
    std::vector<RemoteNode> remoteNodes;

    // Helper function to parse a line from the config file
    bool parseLine(const std::string& line);

    // Helper function to trim whitespace from a string
    static std::string trim(const std::string& str);
};

#endif // CONFIG_H
