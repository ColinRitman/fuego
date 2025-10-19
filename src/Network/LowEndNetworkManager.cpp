// Copyright (c) 2024 Fuego Developers
// Low-End Network Manager Implementation for ARM64 Devices
// Phase 2: Optimized network protocol for low-end devices

#include "LowEndNetworkManager.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <algorithm>

namespace Network {
namespace LowEnd {

LowEndNetworkManager::LowEndNetworkManager()
    : m_initialized(false)
    , m_shutdown(false)
    , m_nextPeerId(1)
    , m_maxConnections(LOWEND_MAX_CONNECTIONS)
    , m_maxMessageSize(LOWEND_MAX_MESSAGE_SIZE)
    , m_connectionTimeout(LOWEND_CONNECTION_TIMEOUT)
    , m_keepAliveInterval(LOWEND_KEEP_ALIVE_INTERVAL)
    , m_memoryLimit(LOWEND_MAX_MEMORY_USAGE)
    , m_memoryUsage(0)
    , m_bytesSent(0)
    , m_bytesReceived(0)
    , m_messagesSent(0)
    , m_messagesReceived(0)
    , m_averageLatency(0.0)
    , m_packetLossRate(0.0)
    , m_retransmissions(0)
    , m_maxWorkerThreads(LOWEND_MAX_THREADS)
{
    optimizeForLowEnd();
    optimizeForARM64();
}

LowEndNetworkManager::~LowEndNetworkManager() {
    shutdown();
}

bool LowEndNetworkManager::initialize() {
    if (m_initialized) {
        return true;
    }
    
    // Start worker threads
    for (uint32_t i = 0; i < m_maxWorkerThreads; ++i) {
        m_workerThreads.emplace_back(&LowEndNetworkManager::workerThread, this);
    }
    
    m_initialized = true;
    return true;
}

void LowEndNetworkManager::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    m_shutdown = true;
    
    // Disconnect all peers
    disconnectAll();
    
    // Wait for worker threads to finish
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_workerThreads.clear();
    
    m_initialized = false;
}

bool LowEndNetworkManager::connectToPeer(const std::string& address, uint16_t port) {
    if (!m_initialized || m_shutdown) {
        return false;
    }
    
    // Check connection limit
    if (m_connections.size() >= m_maxConnections) {
        return false;
    }
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }
    
    // Set non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Set socket options for low-end devices
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set buffer sizes for low-end devices
    int sendBufferSize = 8192;  // 8KB send buffer
    int recvBufferSize = 8192;  // 8KB receive buffer
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendBufferSize, sizeof(sendBufferSize));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recvBufferSize, sizeof(recvBufferSize));
    
    // Connect
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, address.c_str(), &addr.sin_addr);
    
    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (result < 0 && errno != EINPROGRESS) {
        close(sock);
        return false;
    }
    
    // Create connection object
    uint32_t peerId = m_nextPeerId++;
    auto connection = std::make_unique<PeerConnection>();
    connection->id = peerId;
    connection->address = address;
    connection->port = port;
    connection->socket = sock;
    connection->connected = (result == 0);
    connection->active = true;
    connection->lastActivity = std::chrono::steady_clock::now();
    
    // Add to connections
    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        m_connections[peerId] = std::move(connection);
    }
    
    return true;
}

void LowEndNetworkManager::disconnectPeer(uint32_t peerId) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    auto it = m_connections.find(peerId);
    if (it != m_connections.end()) {
        it->second->active = false;
        it->second->connected = false;
        close(it->second->socket);
        m_connections.erase(it);
    }
}

void LowEndNetworkManager::disconnectAll() {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    for (auto& pair : m_connections) {
        pair.second->active = false;
        pair.second->connected = false;
        close(pair.second->socket);
    }
    m_connections.clear();
}

bool LowEndNetworkManager::sendMessage(uint32_t peerId, const std::vector<uint8_t>& data) {
    if (data.size() > m_maxMessageSize) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    auto it = m_connections.find(peerId);
    if (it == m_connections.end() || !it->second->connected) {
        return false;
    }
    
    // Add to send queue
    {
        std::lock_guard<std::mutex> sendLock(it->second->sendMutex);
        it->second->sendQueue.push(data);
    }
    
    // Notify worker thread
    it->second->sendCondition.notify_one();
    
    m_messagesSent++;
    m_bytesSent += data.size();
    
    return true;
}

bool LowEndNetworkManager::receiveMessage(uint32_t peerId, std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    auto it = m_connections.find(peerId);
    if (it == m_connections.end() || !it->second->connected) {
        return false;
    }
    
    // Check receive queue
    {
        std::lock_guard<std::mutex> recvLock(it->second->receiveMutex);
        if (it->second->receiveQueue.empty()) {
            return false;
        }
        
        data = it->second->receiveQueue.front();
        it->second->receiveQueue.pop();
    }
    
    m_messagesReceived++;
    m_bytesReceived += data.size();
    
    return true;
}

uint32_t LowEndNetworkManager::getActiveConnections() const {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    return m_connections.size();
}

uint64_t LowEndNetworkManager::getBytesSent() const {
    return m_bytesSent;
}

uint64_t LowEndNetworkManager::getBytesReceived() const {
    return m_bytesReceived;
}

uint32_t LowEndNetworkManager::getMessagesSent() const {
    return m_messagesSent;
}

uint32_t LowEndNetworkManager::getMessagesReceived() const {
    return m_messagesReceived;
}

void LowEndNetworkManager::setMaxConnections(uint32_t max) {
    m_maxConnections = std::min(max, LOWEND_MAX_CONNECTIONS);
}

void LowEndNetworkManager::setMaxMessageSize(uint32_t max) {
    m_maxMessageSize = std::min(max, LOWEND_MAX_MESSAGE_SIZE);
}

void LowEndNetworkManager::setConnectionTimeout(uint32_t timeoutMs) {
    m_connectionTimeout = timeoutMs;
}

void LowEndNetworkManager::setKeepAliveInterval(uint32_t intervalMs) {
    m_keepAliveInterval = intervalMs;
}

void LowEndNetworkManager::setMemoryLimit(uint64_t limitBytes) {
    m_memoryLimit = std::min(limitBytes, LOWEND_MAX_MEMORY_USAGE);
}

uint64_t LowEndNetworkManager::getMemoryUsage() const {
    return m_memoryUsage;
}

double LowEndNetworkManager::getAverageLatency() const {
    return m_averageLatency;
}

double LowEndNetworkManager::getPacketLossRate() const {
    return m_packetLossRate;
}

uint32_t LowEndNetworkManager::getRetransmissions() const {
    return m_retransmissions;
}

void LowEndNetworkManager::workerThread() {
    while (!m_shutdown) {
        // Process connections
        {
            std::lock_guard<std::mutex> lock(m_connectionsMutex);
            for (auto& pair : m_connections) {
                if (pair.second->active && pair.second->connected) {
                    handleConnection(std::move(pair.second));
                }
            }
        }
        
        // Update statistics
        updateStatistics();
        
        // Cleanup inactive connections
        cleanupInactiveConnections();
        
        // Sleep for a short time to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void LowEndNetworkManager::handleConnection(std::unique_ptr<PeerConnection> connection) {
    if (!connection || !connection->active) {
        return;
    }
    
    // Process send queue
    processSendQueue(connection.get());
    
    // Process receive queue
    processReceiveQueue(connection.get());
    
    // Update last activity
    connection->lastActivity = std::chrono::steady_clock::now();
}

void LowEndNetworkManager::processSendQueue(PeerConnection* connection) {
    if (!connection) return;
    
    std::lock_guard<std::mutex> lock(connection->sendMutex);
    while (!connection->sendQueue.empty()) {
        const auto& data = connection->sendQueue.front();
        
        // Send data
        ssize_t sent = send(connection->socket, data.data(), data.size(), MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                connection->connected = false;
                break;
            }
        } else {
            connection->sendQueue.pop();
        }
    }
}

void LowEndNetworkManager::processReceiveQueue(PeerConnection* connection) {
    if (!connection) return;
    
    uint8_t buffer[4096];  // 4KB buffer for low-end devices
    ssize_t received = recv(connection->socket, buffer, sizeof(buffer), MSG_DONTWAIT);
    
    if (received > 0) {
        std::vector<uint8_t> data(buffer, buffer + received);
        
        std::lock_guard<std::mutex> lock(connection->receiveMutex);
        connection->receiveQueue.push(data);
        
        // Notify waiting threads
        connection->receiveCondition.notify_one();
    } else if (received == 0) {
        connection->connected = false;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        connection->connected = false;
    }
}

void LowEndNetworkManager::updateStatistics() {
    // Update average latency (simplified)
    m_averageLatency = 50.0;  // 50ms average
    
    // Update packet loss rate (simplified)
    m_packetLossRate = 0.01;  // 1% packet loss
    
    // Update memory usage
    m_memoryUsage = m_connections.size() * 1024;  // 1KB per connection
}

void LowEndNetworkManager::cleanupInactiveConnections() {
    auto now = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    for (auto it = m_connections.begin(); it != m_connections.end();) {
        auto& connection = it->second;
        auto timeSinceActivity = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - connection->lastActivity).count();
        
        if (!connection->active || 
            (!connection->connected && timeSinceActivity > m_connectionTimeout)) {
            close(connection->socket);
            it = m_connections.erase(it);
        } else {
            ++it;
        }
    }
}

void LowEndNetworkManager::optimizeForLowEnd() {
    // Reduce memory usage
    m_maxConnections = std::min(m_maxConnections, 4U);
    m_maxMessageSize = std::min(m_maxMessageSize, 4096U);
    m_maxWorkerThreads = std::min(m_maxWorkerThreads, 2U);
    
    // Set conservative timeouts
    m_connectionTimeout = 30000;  // 30 seconds
    m_keepAliveInterval = 60000;  // 60 seconds
}

void LowEndNetworkManager::reduceMemoryUsage() {
    // Limit queue sizes
    for (auto& pair : m_connections) {
        auto& connection = pair.second;
        
        // Limit send queue
        while (connection->sendQueue.size() > 10) {
            connection->sendQueue.pop();
        }
        
        // Limit receive queue
        while (connection->receiveQueue.size() > 10) {
            connection->receiveQueue.pop();
        }
    }
}

void LowEndNetworkManager::limitConnectionCount() {
    if (m_connections.size() > m_maxConnections) {
        // Disconnect oldest connections
        std::vector<uint32_t> toDisconnect;
        for (auto& pair : m_connections) {
            if (toDisconnect.size() >= m_connections.size() - m_maxConnections) {
                break;
            }
            toDisconnect.push_back(pair.first);
        }
        
        for (uint32_t peerId : toDisconnect) {
            disconnectPeer(peerId);
        }
    }
}

void LowEndNetworkManager::optimizeMessageSizes() {
    // Limit message sizes based on available memory
    uint64_t availableMemory = m_memoryLimit - m_memoryUsage;
    uint32_t maxMessageSize = static_cast<uint32_t>(availableMemory / 10);  // 10% of available memory
    
    m_maxMessageSize = std::min(m_maxMessageSize, maxMessageSize);
}

void LowEndNetworkManager::optimizeForARM64() {
    // Use NEON operations where possible
    useNEONOperations();
    
    // Optimize memory alignment
    optimizeMemoryAlignment();
}

void LowEndNetworkManager::useNEONOperations() {
    // Use NEON for data processing where possible
    // This would be implemented in the actual data processing methods
}

void LowEndNetworkManager::optimizeMemoryAlignment() {
    // Ensure 16-byte alignment for ARM64 NEON operations
    // This would be implemented in the memory allocation methods
}

} // namespace LowEnd
} // namespace Network