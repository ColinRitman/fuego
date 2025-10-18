// Copyright (c) 2024 Fuego Developers
// Low-End Device Logger with minimal memory footprint

#pragma once

#include "FuegoLowEndConfig.h"
#include <string>
#include <atomic>
#include <mutex>

#ifdef FUEGO_LOWEND_DEVICE

namespace Logging {

enum class LowEndLogLevel {
    ERROR = 0,
    WARNING = 1,
    INFO = 2
};

class LowEndLogger {
public:
    LowEndLogger();
    ~LowEndLogger();
    
    // Initialize with minimal resources
    bool initialize();
    void shutdown();
    
    // Logging with level filtering
    void log(LowEndLogLevel level, const std::string& message);
    void logError(const std::string& message);
    void logWarning(const std::string& message);
    void logInfo(const std::string& message);
    
    // Set maximum log level (default: WARNING for low-end devices)
    void setMaxLevel(LowEndLogLevel level) { m_maxLevel = level; }
    
    // Get statistics
    size_t getLogCount() const { return m_logCount.load(); }
    size_t getDroppedCount() const { return m_droppedCount.load(); }
    
private:
    // Circular buffer for log messages
    struct LogEntry {
        LowEndLogLevel level;
        char message[256]; // Fixed size to avoid dynamic allocation
        uint64_t timestamp;
    };
    
    static constexpr size_t LOG_BUFFER_SIZE = LOWEND_CONSTANT(LOWEND_LOG_BUFFER_SIZE);
    static constexpr size_t MAX_MESSAGE_SIZE = 255;
    
    LogEntry m_logBuffer[LOG_BUFFER_SIZE];
    std::atomic<size_t> m_writeIndex;
    std::atomic<size_t> m_readIndex;
    std::atomic<size_t> m_logCount;
    std::atomic<size_t> m_droppedCount;
    
    LowEndLogLevel m_maxLevel;
    std::mutex m_logMutex;
    
    // Internal methods
    void writeToBuffer(LowEndLogLevel level, const std::string& message);
    void flushBuffer();
    std::string formatMessage(LowEndLogLevel level, const std::string& message);
    uint64_t getCurrentTimestamp();
};

// Global logger instance for low-end devices
extern LowEndLogger* g_lowEndLogger;

// Convenience macros
#define LOWEND_LOG_ERROR(msg) if (g_lowEndLogger) g_lowEndLogger->logError(msg)
#define LOWEND_LOG_WARNING(msg) if (g_lowEndLogger) g_lowEndLogger->logWarning(msg)
#define LOWEND_LOG_INFO(msg) if (g_lowEndLogger) g_lowEndLogger->logInfo(msg)

} // namespace Logging

#else
// Standard logging for non-lowend builds
namespace Logging {
    // Forward declarations for standard logger
    class Logger;
    extern Logger* g_logger;
}

#define LOWEND_LOG_ERROR(msg) if (Logging::g_logger) Logging::g_logger->log(Logging::ERROR, msg)
#define LOWEND_LOG_WARNING(msg) if (Logging::g_logger) Logging::g_logger->log(Logging::WARNING, msg)
#define LOWEND_LOG_INFO(msg) if (Logging::g_logger) Logging::g_logger->log(Logging::INFO, msg)

#endif // FUEGO_LOWEND_DEVICE