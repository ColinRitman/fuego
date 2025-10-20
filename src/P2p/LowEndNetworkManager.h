// Copyright (c) 2024 Fuego Developers
// Low-End Device Network Manager

#pragma once

#include "FuegoLowEndConfig.h"
#include "Common/LowEndContainers.h"
#include <thread>
#include <atomic>
#include <mutex>

#ifdef FUEGO_LOWEND_DEVICE

namespace CryptoNote {

class LowEndNetworkManager {
public:
    LowEndNetworkManager();
    ~LowEndNetworkManager();
    
    // Initialize with reduced resource limits
    bool initialize();
    void shutdown();
    
    // Connection management with limits
    bool addConnection(const std::string& address, uint16_t port);
    void removeConnection(const std::string& address);
    void removeAllConnections();
    
    // Data transmission with size limits
    bool sendData(const std::string& address, const void* data, size_t size);
    bool receiveData(std::string& address, void* buffer, size_t& size);
    
    // Statistics
    size_t getConnectionCount() const { return m_connections.size(); }
    size_t getDataTransmitted() const { return m_dataTransmitted.load(); }
    size_t getDataReceived() const { return m_dataReceived.load(); }
    
    // Resource monitoring
    void printResourceUsage() const;
    
private:
    struct Connection {
        std::string address;
        uint16_t port;
        int socket;
        std::atomic<bool> active;
        std::atomic<size_t> bytesTransmitted;
        std::atomic<size_t> bytesReceived;
    };
    
    // Limited connection pool
    Common::LowEndUnorderedMap<std::string, std::unique_ptr<Connection>> m_connections;
    
    // Single IO thread for low-end devices
    std::thread m_ioThread;
    std::atomic<bool> m_running;
    
    // Statistics
    std::atomic<size_t> m_dataTransmitted;
    std::atomic<size_t> m_dataReceived;
    
    // Resource limits
    static constexpr size_t MAX_PACKET_SIZE = LOWEND_CONSTANT(LOWEND_MAX_PACKET_SIZE);
    static constexpr size_t MAX_CONNECTIONS = LOWEND_CONSTANT(LOWEND_MAX_CONNECTIONS);
    
    // Threading
    mutable std::mutex m_connectionsMutex;
    
    // Internal methods
    void ioThreadFunction();
    bool processConnection(Connection& conn);
    void cleanupInactiveConnections();
};

} // namespace CryptoNote

#endif // FUEGO_LOWEND_DEVICE