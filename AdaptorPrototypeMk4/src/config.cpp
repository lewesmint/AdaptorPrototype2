#include "config.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

Config::Config()
    : localIp("127.0.0.1"), localPort(8080), instanceId(1) {
    // Default configuration
}

bool Config::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[CONFIG] Failed to open config file: " << filePath << std::endl;
        return false;
    }

    std::cout << "[CONFIG] Loading configuration from " << filePath << std::endl;

    // Clear any existing remote nodes
    remoteNodes.clear();

    // Parse the file line by line
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Parse the line
        if (!parseLine(line)) {
            std::cerr << "[CONFIG] Failed to parse line: " << line << std::endl;
            // Continue parsing despite the error
        }
    }

    std::cout << "[CONFIG] Configuration loaded successfully" << std::endl;
    return true;
}

bool Config::parseLine(const std::string& line) {
    // Find the equals sign
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
        return false;
    }

    // Extract key and value
    std::string key = trim(line.substr(0, pos));
    std::string value = trim(line.substr(pos + 1));

    // Parse the key-value pair
    if (key == "local_ip") {
        localIp = value;
    } else if (key == "local_port") {
        try {
            localPort = std::stoi(value);
        } catch (const std::exception&) {
            std::cerr << "[CONFIG] Invalid local_port value: " << value << std::endl;
            return false;
        }
    } else if (key == "instance_id") {
        try {
            instanceId = std::stoi(value);
        } catch (const std::exception&) {
            std::cerr << "[CONFIG] Invalid instance_id value: " << value << std::endl;
            return false;
        }
    } else if (key == "remote_node") {
        // Parse remote node (format: IP:port:instance_id)
        std::istringstream iss(value);
        std::string ip;
        std::string portStr;
        std::string idStr;

        // Parse IP
        if (!std::getline(iss, ip, ':')) {
            std::cerr << "[CONFIG] Invalid remote_node format: " << value << std::endl;
            return false;
        }

        // Parse port
        if (!std::getline(iss, portStr, ':')) {
            std::cerr << "[CONFIG] Invalid remote_node format: " << value << std::endl;
            return false;
        }

        // Parse instance ID
        if (!std::getline(iss, idStr)) {
            std::cerr << "[CONFIG] Invalid remote_node format: " << value << std::endl;
            return false;
        }

        // Convert port and instance ID to integers
        int port, id;
        try {
            port = std::stoi(portStr);
            id = std::stoi(idStr);
        } catch (const std::exception&) {
            std::cerr << "[CONFIG] Invalid remote_node port or instance_id: " << value << std::endl;
            return false;
        }

        // Add the remote node
        remoteNodes.push_back(RemoteNode(ip, port, id));
    } else {
        std::cerr << "[CONFIG] Unknown configuration key: " << key << std::endl;
        return false;
    }

    return true;
}

bool Config::isValid() const {
    // Check if the local configuration is valid
    if (localIp.empty() || localPort <= 0 || instanceId <= 0) {
        return false;
    }

    // Configuration is valid
    return true;
}

std::string Config::toString() const {
    std::ostringstream oss;
    oss << "Configuration:" << std::endl;
    oss << "  Local IP: " << localIp << std::endl;
    oss << "  Local Port: " << localPort << std::endl;
    oss << "  Instance ID: " << instanceId << std::endl;
    oss << "  Remote Nodes:" << std::endl;

    for (const auto& node : remoteNodes) {
        oss << "    " << node.ip << ":" << node.port << ":" << node.instanceId << std::endl;
    }

    return oss.str();
}

std::string Config::trim(const std::string& str) {
    // Find the first non-whitespace character
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    // Find the last non-whitespace character
    size_t end = str.find_last_not_of(" \t\r\n");

    // Return the trimmed string
    return str.substr(start, end - start + 1);
}
