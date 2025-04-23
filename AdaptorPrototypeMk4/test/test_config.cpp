#include <gtest/gtest.h>
#include "../src/config.h"
#include <fstream>
#include <string>

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test config file
        std::ofstream testConfig("test_config.ini");
        testConfig << "# Test Configuration File\n";
        testConfig << "local_ip = 192.168.1.100\n";
        testConfig << "local_port = 9090\n";
        testConfig << "instance_id = 3\n";
        testConfig << "remote_node = 192.168.1.101:9091:4\n";
        testConfig << "remote_node = 192.168.1.102:9092:5\n";
        testConfig.close();
    }

    void TearDown() override {
        // Remove the temporary test config file
        remove("test_config.ini");
    }
};

TEST_F(ConfigTest, DefaultConstructor) {
    Config config;
    
    // Check default values
    EXPECT_EQ(config.getLocalIp(), "127.0.0.1");
    EXPECT_EQ(config.getLocalPort(), 8080);
    EXPECT_EQ(config.getInstanceId(), 1);
    EXPECT_TRUE(config.getRemoteNodes().empty());
    EXPECT_TRUE(config.isValid());
}

TEST_F(ConfigTest, LoadFromFile) {
    Config config;
    
    // Load from the test config file
    EXPECT_TRUE(config.loadFromFile("test_config.ini"));
    
    // Check loaded values
    EXPECT_EQ(config.getLocalIp(), "192.168.1.100");
    EXPECT_EQ(config.getLocalPort(), 9090);
    EXPECT_EQ(config.getInstanceId(), 3);
    EXPECT_FALSE(config.getRemoteNodes().empty());
    EXPECT_EQ(config.getRemoteNodes().size(), 2);
    EXPECT_TRUE(config.isValid());
}

TEST_F(ConfigTest, RemoteNodeStructure) {
    Config config;
    
    // Load from the test config file
    EXPECT_TRUE(config.loadFromFile("test_config.ini"));
    
    // Check remote nodes
    const std::vector<Config::RemoteNode>& nodes = config.getRemoteNodes();
    ASSERT_EQ(nodes.size(), 2);
    
    // Check first remote node
    EXPECT_EQ(nodes[0].ip, "192.168.1.101");
    EXPECT_EQ(nodes[0].port, 9091);
    EXPECT_EQ(nodes[0].instanceId, 4);
    
    // Check second remote node
    EXPECT_EQ(nodes[1].ip, "192.168.1.102");
    EXPECT_EQ(nodes[1].port, 9092);
    EXPECT_EQ(nodes[1].instanceId, 5);
}

TEST_F(ConfigTest, InvalidConfig) {
    // Create an invalid config file
    std::ofstream invalidConfig("invalid_config.ini");
    invalidConfig << "# Invalid Configuration File\n";
    invalidConfig << "local_ip = \n";  // Empty IP
    invalidConfig << "local_port = -1\n";  // Invalid port
    invalidConfig << "instance_id = 0\n";  // Invalid instance ID
    invalidConfig.close();
    
    Config config;
    
    // Load should succeed but config should be invalid
    EXPECT_TRUE(config.loadFromFile("invalid_config.ini"));
    EXPECT_FALSE(config.isValid());
    
    // Clean up
    remove("invalid_config.ini");
}

TEST_F(ConfigTest, InvalidRemoteNode) {
    // Create a config file with invalid remote node
    std::ofstream invalidConfig("invalid_remote.ini");
    invalidConfig << "# Invalid Remote Node Configuration\n";
    invalidConfig << "local_ip = 127.0.0.1\n";
    invalidConfig << "local_port = 8080\n";
    invalidConfig << "instance_id = 1\n";
    invalidConfig << "remote_node = 192.168.1.101:invalid:4\n";  // Invalid port
    invalidConfig.close();
    
    Config config;
    
    // Load should succeed but the remote node should be ignored
    EXPECT_TRUE(config.loadFromFile("invalid_remote.ini"));
    EXPECT_TRUE(config.isValid());  // Local config is still valid
    EXPECT_TRUE(config.getRemoteNodes().empty());  // No remote nodes should be added
    
    // Clean up
    remove("invalid_remote.ini");
}

TEST_F(ConfigTest, ToStringOutput) {
    Config config;
    
    // Load from the test config file
    EXPECT_TRUE(config.loadFromFile("test_config.ini"));
    
    // Get the string representation
    std::string configStr = config.toString();
    
    // Check that the string contains all the expected information
    EXPECT_NE(configStr.find("192.168.1.100"), std::string::npos);
    EXPECT_NE(configStr.find("9090"), std::string::npos);
    EXPECT_NE(configStr.find("3"), std::string::npos);
    EXPECT_NE(configStr.find("192.168.1.101:9091:4"), std::string::npos);
    EXPECT_NE(configStr.find("192.168.1.102:9092:5"), std::string::npos);
}
