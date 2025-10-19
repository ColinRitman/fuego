// Copyright (c) 2024 Fuego Developers
// Low-End Network Manager for ARM64 Devices
// Phase 2: Optimized network protocol for low-end devices

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>

#include "LowEndConfig.h"

namespace Network {
namespace LowEnd {

class LowEndNetworkManager {
public:
    LowEndNetworkManager();
    ~LowEndNetworkManager();

    // Core network operations
    bool initialize();
    void shutdown();
    
    // Connection management
    bool connectToPeer(const std::string& address, uint16_t port);
    void disconnectPeer(uint32_t peerId);
    void disconnectAll();
    
    // Message handling
    bool sendMessage(uint32_t peerId, const std::vector<uint8_t>& data);
    bool receiveMessage(uint32_t peerId, std::vector<uint8_t>& data);
    
    // Network statistics
    uint32_t getActiveConnections() const;
    uint64_t getBytesSent() const;
    uint64_t getBytesReceived() const;
    uint32_t getMessagesSent() const;
    uint32_t getMessagesReceived() const;
    
    // Low-end optimizations
    void setMaxConnections(uint32_t max);
    void setMaxMessageSize(uint32_t max);
    void setConnectionTimeout(uint32_t timeoutMs);
    void setKeepAliveInterval(uint32_t intervalMs);
    
    // Memory management
    void setMemoryLimit(uint64_t limitBytes);
    uint64_t getMemoryUsage() const;
    
    // Performance monitoring
    double getAverageLatency() const;
    double getPacketLossRate() const;
    uint32_t getRetransmissions() const;

private:
    struct PeerConnection {
        uint32_t id;
        std::string address;
        uint16_t port;
        int socket;
        std::atomic<bool> connected;
        std::atomic<bool> active;
        std::chrono::steady_clock::time_point lastActivity;
        std::queue<std::vector<uint8_t>> sendQueue;
        std::queue<std::vector<uint8_t>> receiveQueue;
        std::mutex sendMutex;
        std::mutex receiveMutex;
        std::condition_variable sendCondition;
        std::condition_variable receiveCondition;
    };
    
    // Core data
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_shutdown;
    std::atomic<uint32_t> m_nextPeerId;
    
    // Connection management
    std::unordered_map<uint32_t, std::unique_ptr<PeerConnection>> m_connections;
    std::mutex m_connectionsMutex;
    std::atomic<uint32_t> m_maxConnections;
    
    // Message handling
    std::atomic<uint32_t> m_maxMessageSize;
    std::atomic<uint32_t> m_connectionTimeout;
    std::atomic<uint32_t> m_keepAliveInterval;
    
    // Memory management
    std::atomic<uint64_t> m_memoryLimit;
    std::atomic<uint64_t> m_memoryUsage;
    
    // Statistics
    std::atomic<uint64_t> m_bytesSent;
    std::atomic<uint64_t> m_bytesReceived;
    std::atomic<uint32_t> m_messagesSent;
    std::atomic<uint32_t> m_messagesReceived;
    std::atomic<double> m_averageLatency;
    std::atomic<double> m_packetLossRate;
    std::atomic<uint32_t> m_retransmissions;
    
    // Worker threads
    std::vector<std::thread> m_workerThreads;
    std::atomic<uint32_t> m_maxWorkerThreads;
    
    // Internal methods
    void workerThread();
    void handleConnection(std::unique_ptr<PeerConnection> connection);
    void processSendQueue(PeerConnection* connection);
    void processReceiveQueue(PeerConnection* connection);
    void updateStatistics();
    void cleanupInactiveConnections();
    
    // Low-end optimizations
    void optimizeForLowEnd();
    void reduceMemoryUsage();
    void limitConnectionCount();
    void optimizeMessageSizes();
    
    // ARM64 optimizations
    void optimizeForARM64();
    void useNEONOperations();
    void optimizeMemoryAlignment();
};

} // namespace LowEnd
} // namespace Network