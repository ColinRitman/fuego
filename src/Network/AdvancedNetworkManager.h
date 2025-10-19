// Copyright (c) 2024 Fuego Developers
// Advanced Network Manager for ARM64 Low-End Devices
// Phase 3: Advanced networking optimizations for low-end devices

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <functional>
#include <random>

#include "LowEndConfig.h"
#include "LowEndContainers.h"

namespace Network {
namespace Advanced {

// Connection quality metrics
struct ConnectionQuality {
    double latency;
    double bandwidth;
    double packetLoss;
    double jitter;
    uint32_t retransmissions;
    uint64_t bytesTransferred;
    std::chrono::steady_clock::time_point lastActivity;
    uint32_t qualityScore;
};

// Connection pool configuration
struct ConnectionPoolConfig {
    uint32_t maxConnections;
    uint32_t minConnections;
    uint32_t maxIdleTime;
    uint32_t connectionTimeout;
    uint32_t keepAliveInterval;
    uint32_t retryAttempts;
    double qualityThreshold;
    bool enableLoadBalancing;
    bool enableFailover;
};

// Network statistics
struct NetworkStatistics {
    uint64_t totalBytesSent;
    uint64_t totalBytesReceived;
    uint32_t totalConnections;
    uint32_t activeConnections;
    uint32_t failedConnections;
    double averageLatency;
    double averageBandwidth;
    double averagePacketLoss;
    uint32_t totalRetransmissions;
    uint64_t uptime;
};

// Message priority levels
enum class MessagePriority {
    CRITICAL = 0,
    HIGH = 1,
    NORMAL = 2,
    LOW = 3,
    BACKGROUND = 4
};

// Connection state
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AUTHENTICATING,
    AUTHENTICATED,
    ERROR,
    FAILED
};

class AdvancedNetworkManager {
public:
    AdvancedNetworkManager();
    ~AdvancedNetworkManager();

    // Core network operations
    bool initialize(const ConnectionPoolConfig& config);
    void shutdown();
    
    // Connection management
    bool connectToPeer(const std::string& address, uint16_t port, MessagePriority priority = MessagePriority::NORMAL);
    void disconnectPeer(uint32_t peerId);
    void disconnectAll();
    bool reconnectPeer(uint32_t peerId);
    
    // Connection pooling
    uint32_t getConnectionFromPool(const std::string& address, uint16_t port);
    void returnConnectionToPool(uint32_t peerId);
    void evictIdleConnections();
    void optimizeConnectionPool();
    
    // Message handling with priority
    bool sendMessage(uint32_t peerId, const std::vector<uint8_t>& data, MessagePriority priority = MessagePriority::NORMAL);
    bool receiveMessage(uint32_t peerId, std::vector<uint8_t>& data, MessagePriority priority = MessagePriority::NORMAL);
    bool broadcastMessage(const std::vector<uint8_t>& data, MessagePriority priority = MessagePriority::NORMAL);
    
    // Quality management
    void updateConnectionQuality(uint32_t peerId, const ConnectionQuality& quality);
    ConnectionQuality getConnectionQuality(uint32_t peerId) const;
    std::vector<uint32_t> getHighQualityConnections() const;
    void removeLowQualityConnections();
    
    // Load balancing
    uint32_t selectBestConnection(const std::string& address, uint16_t port) const;
    void enableLoadBalancing(bool enabled);
    void setLoadBalancingStrategy(const std::string& strategy);
    
    // Failover management
    void enableFailover(bool enabled);
    void addFailoverPeer(const std::string& address, uint16_t port);
    void removeFailoverPeer(const std::string& address, uint16_t port);
    bool handleConnectionFailure(uint32_t peerId);
    
    // Bandwidth management
    void setBandwidthLimit(uint64_t limitBytesPerSecond);
    void setConnectionBandwidthLimit(uint32_t peerId, uint64_t limitBytesPerSecond);
    void enableBandwidthThrottling(bool enabled);
    void setThrottlingStrategy(const std::string& strategy);
    
    // Network monitoring
    NetworkStatistics getNetworkStatistics() const;
    void resetStatistics();
    void enableMonitoring(bool enabled);
    void setMonitoringInterval(uint32_t intervalMs);
    
    // Low-end optimizations
    void optimizeForLowEnd();
    void setMemoryLimit(uint64_t limitBytes);
    void setConnectionLimit(uint32_t maxConnections);
    void enableCompression(bool enabled);
    void setCompressionLevel(int level);
    
    // ARM64 optimizations
    void optimizeForARM64();
    void useNEONOperations();
    void optimizeMemoryAlignment();
    
    // Performance monitoring
    double getAverageLatency() const;
    double getAverageBandwidth() const;
    double getPacketLossRate() const;
    uint32_t getRetransmissionRate() const;
    uint64_t getMemoryUsage() const;
    
    // Configuration
    void setConfig(const ConnectionPoolConfig& config);
    ConnectionPoolConfig getConfig() const;
    
    // Event handling
    void setConnectionEventHandler(std::function<void(uint32_t, ConnectionState)> handler);
    void setMessageEventHandler(std::function<void(uint32_t, const std::vector<uint8_t>&)> handler);
    void setErrorEventHandler(std::function<void(uint32_t, const std::string&)> handler);

private:
    struct PeerConnection {
        uint32_t id;
        std::string address;
        uint16_t port;
        int socket;
        ConnectionState state;
        ConnectionQuality quality;
        std::chrono::steady_clock::time_point lastActivity;
        std::chrono::steady_clock::time_point creationTime;
        std::queue<std::pair<std::vector<uint8_t>, MessagePriority>> sendQueue;
        std::queue<std::pair<std::vector<uint8_t>, MessagePriority>> receiveQueue;
        std::mutex sendMutex;
        std::mutex receiveMutex;
        std::condition_variable sendCondition;
        std::condition_variable receiveCondition;
        std::atomic<bool> active;
        std::atomic<bool> inUse;
        uint64_t bandwidthLimit;
        uint64_t bytesTransferred;
        std::chrono::steady_clock::time_point lastBandwidthReset;
    };
    
    // Core data
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_shutdown;
    std::atomic<uint32_t> m_nextPeerId;
    
    // Connection management
    std::unordered_map<uint32_t, std::unique_ptr<PeerConnection>> m_connections;
    std::unordered_map<std::string, std::vector<uint32_t>> m_addressToConnections;
    std::unordered_set<uint32_t> m_connectionPool;
    std::mutex m_connectionsMutex;
    std::mutex m_poolMutex;
    
    // Configuration
    ConnectionPoolConfig m_config;
    std::atomic<uint64_t> m_memoryLimit;
    std::atomic<bool> m_compressionEnabled;
    std::atomic<int> m_compressionLevel;
    
    // Bandwidth management
    std::atomic<uint64_t> m_globalBandwidthLimit;
    std::atomic<bool> m_throttlingEnabled;
    std::string m_throttlingStrategy;
    std::chrono::steady_clock::time_point m_lastBandwidthReset;
    
    // Load balancing
    std::atomic<bool> m_loadBalancingEnabled;
    std::string m_loadBalancingStrategy;
    std::random_device m_randomDevice;
    std::mt19937 m_randomGenerator;
    
    // Failover management
    std::atomic<bool> m_failoverEnabled;
    std::vector<std::pair<std::string, uint16_t>> m_failoverPeers;
    std::mutex m_failoverMutex;
    
    // Monitoring
    std::atomic<bool> m_monitoringEnabled;
    std::atomic<uint32_t> m_monitoringInterval;
    std::thread m_monitoringThread;
    std::atomic<bool> m_monitoringActive;
    
    // Statistics
    std::atomic<uint64_t> m_totalBytesSent;
    std::atomic<uint64_t> m_totalBytesReceived;
    std::atomic<uint32_t> m_totalConnections;
    std::atomic<uint32_t> m_activeConnections;
    std::atomic<uint32_t> m_failedConnections;
    std::atomic<double> m_averageLatency;
    std::atomic<double> m_averageBandwidth;
    std::atomic<double> m_averagePacketLoss;
    std::atomic<uint32_t> m_totalRetransmissions;
    std::chrono::steady_clock::time_point m_startTime;
    
    // Event handlers
    std::function<void(uint32_t, ConnectionState)> m_connectionEventHandler;
    std::function<void(uint32_t, const std::vector<uint8_t>&)> m_messageEventHandler;
    std::function<void(uint32_t, const std::string&)> m_errorEventHandler;
    
    // Worker threads
    std::vector<std::thread> m_workerThreads;
    std::atomic<uint32_t> m_maxWorkerThreads;
    
    // Internal methods
    void workerThread();
    void monitoringThread();
    void handleConnection(std::unique_ptr<PeerConnection> connection);
    void processSendQueue(PeerConnection* connection);
    void processReceiveQueue(PeerConnection* connection);
    void updateConnectionQuality(PeerConnection* connection);
    void updateStatistics();
    void cleanupInactiveConnections();
    
    // Connection pooling
    uint32_t createConnection(const std::string& address, uint16_t port);
    void destroyConnection(uint32_t peerId);
    bool isConnectionInPool(uint32_t peerId) const;
    void addConnectionToPool(uint32_t peerId);
    void removeConnectionFromPool(uint32_t peerId);
    
    // Quality management
    void calculateQualityScore(PeerConnection* connection);
    bool isHighQualityConnection(const PeerConnection* connection) const;
    void removeLowQualityConnections();
    
    // Load balancing
    uint32_t selectConnectionByRoundRobin(const std::string& address, uint16_t port) const;
    uint32_t selectConnectionByQuality(const std::string& address, uint16_t port) const;
    uint32_t selectConnectionByLoad(const std::string& address, uint16_t port) const;
    uint32_t selectConnectionByRandom(const std::string& address, uint16_t port) const;
    
    // Failover management
    bool tryFailoverConnection(uint32_t failedPeerId);
    void addFailoverConnection(const std::string& address, uint16_t port);
    
    // Bandwidth management
    bool checkBandwidthLimit(PeerConnection* connection, size_t dataSize);
    void updateBandwidthUsage(PeerConnection* connection, size_t dataSize);
    void resetBandwidthCounters();
    
    // Low-end optimizations
    void reduceMemoryUsage();
    void limitConnectionCount();
    void optimizeMessageSizes();
    void enableCompression();
    
    // ARM64 optimizations
    void useNEONForDataProcessing();
    void optimizeMemoryLayout();
    void useCryptoExtensions();
    
    // Utility methods
    std::string generateConnectionKey(const std::string& address, uint16_t port) const;
    bool validateConnection(uint32_t peerId) const;
    void logConnectionEvent(uint32_t peerId, const std::string& event);
    void logNetworkError(uint32_t peerId, const std::string& error);
};

} // namespace Advanced
} // namespace Network