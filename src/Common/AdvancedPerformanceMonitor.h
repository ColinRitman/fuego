// Copyright (c) 2024 Fuego Developers
// Advanced Performance Monitor for ARM64 Low-End Devices
// Phase 6: Performance monitoring improvements for low-end devices

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <queue>

#include "LowEndConfig.h"
#include "LowEndContainers.h"

namespace Common {
namespace Advanced {

// Performance metric types
enum class MetricType {
    COUNTER,
    GAUGE,
    HISTOGRAM,
    TIMER,
    RATE
};

// Performance metric
struct PerformanceMetric {
    std::string name;
    MetricType type;
    double value;
    double min;
    double max;
    double sum;
    uint64_t count;
    std::chrono::steady_clock::time_point timestamp;
    std::string unit;
    std::string description;
    std::unordered_map<std::string, std::string> tags;
};

// Performance alert
struct PerformanceAlert {
    std::string id;
    std::string metricName;
    std::string condition;
    double threshold;
    double currentValue;
    std::string severity;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    bool acknowledged;
    bool resolved;
};

// Performance report
struct PerformanceReport {
    std::chrono::system_clock::time_point timestamp;
    std::string reportId;
    std::vector<PerformanceMetric> metrics;
    std::vector<PerformanceAlert> alerts;
    double overallHealthScore;
    std::string summary;
    std::vector<std::string> recommendations;
};

// Performance dashboard data
struct DashboardData {
    std::chrono::system_clock::time_point timestamp;
    std::vector<PerformanceMetric> realTimeMetrics;
    std::vector<PerformanceAlert> activeAlerts;
    double systemHealthScore;
    std::string systemStatus;
    std::vector<std::string> topIssues;
    std::unordered_map<std::string, double> componentHealthScores;
};

// Performance monitoring configuration
struct MonitoringConfig {
    bool enableRealTimeMonitoring;
    bool enableHistoricalData;
    bool enableAlerting;
    bool enableDashboard;
    uint32_t monitoringInterval;
    uint32_t dataRetentionDays;
    uint32_t maxMetricsPerComponent;
    uint32_t alertCooldownPeriod;
    double healthScoreThreshold;
    std::string alertNotificationMethod;
    std::string dashboardRefreshInterval;
    std::vector<std::string> monitoredComponents;
};

class AdvancedPerformanceMonitor {
public:
    AdvancedPerformanceMonitor();
    ~AdvancedPerformanceMonitor();

    // Core monitoring operations
    bool initialize(const MonitoringConfig& config);
    void shutdown();
    
    // Metric collection
    void recordMetric(const std::string& name, double value, const std::string& unit = "", const std::unordered_map<std::string, std::string>& tags = {});
    void incrementCounter(const std::string& name, double increment = 1.0, const std::unordered_map<std::string, std::string>& tags = {});
    void setGauge(const std::string& name, double value, const std::string& unit = "", const std::unordered_map<std::string, std::string>& tags = {});
    void recordHistogram(const std::string& name, double value, const std::string& unit = "", const std::unordered_map<std::string, std::string>& tags = {});
    void recordTimer(const std::string& name, double duration, const std::string& unit = "ms", const std::unordered_map<std::string, std::string>& tags = {});
    void recordRate(const std::string& name, double rate, const std::string& unit = "ops/s", const std::unordered_map<std::string, std::string>& tags = {});
    
    // Metric retrieval
    PerformanceMetric getMetric(const std::string& name) const;
    std::vector<PerformanceMetric> getMetrics(const std::string& component = "") const;
    std::vector<PerformanceMetric> getMetricsByType(MetricType type) const;
    std::vector<PerformanceMetric> getMetricsInRange(const std::string& name, std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end) const;
    
    // Alerting
    void createAlert(const std::string& metricName, const std::string& condition, double threshold, const std::string& severity, const std::string& message);
    void acknowledgeAlert(const std::string& alertId);
    void resolveAlert(const std::string& alertId);
    std::vector<PerformanceAlert> getActiveAlerts() const;
    std::vector<PerformanceAlert> getAlertsBySeverity(const std::string& severity) const;
    std::vector<PerformanceAlert> getAlertsByComponent(const std::string& component) const;
    
    // Reporting
    PerformanceReport generateReport(const std::string& component = "", uint32_t timeRangeMinutes = 60) const;
    void exportReport(const PerformanceReport& report, const std::string& filePath) const;
    std::vector<PerformanceReport> getHistoricalReports(uint32_t maxReports = 10) const;
    
    // Dashboard
    DashboardData getDashboardData() const;
    void updateDashboard();
    std::vector<std::string> getDashboardComponents() const;
    double getComponentHealthScore(const std::string& component) const;
    
    // Health monitoring
    double getOverallHealthScore() const;
    std::string getSystemStatus() const;
    std::vector<std::string> getTopIssues() const;
    bool isSystemHealthy() const;
    void performHealthCheck();
    
    // Component monitoring
    void registerComponent(const std::string& component, const std::string& description = "");
    void unregisterComponent(const std::string& component);
    std::vector<std::string> getRegisteredComponents() const;
    void setComponentHealthScore(const std::string& component, double score);
    
    // Low-end optimizations
    void optimizeForLowEnd();
    void setMemoryLimit(uint64_t limitBytes);
    void setMetricLimit(uint32_t maxMetrics);
    void enableResourceThrottling(bool enabled);
    void setThrottlingThreshold(double threshold);
    
    // ARM64 optimizations
    void optimizeForARM64();
    void useNEONOperations();
    void optimizeMemoryAlignment();
    void useCryptoExtensions();
    
    // Configuration
    void setMonitoringConfig(const MonitoringConfig& config);
    MonitoringConfig getMonitoringConfig() const;
    void loadConfigFromFile(const std::string& configPath);
    void saveConfigToFile(const std::string& configPath);
    
    // Event handling
    void setAlertHandler(std::function<void(const PerformanceAlert&)> handler);
    void setHealthChangeHandler(std::function<void(double, double)> handler);
    void setMetricHandler(std::function<void(const PerformanceMetric&)> handler);

private:
    struct MetricData {
        std::string name;
        MetricType type;
        double value;
        double min;
        double max;
        double sum;
        uint64_t count;
        std::chrono::steady_clock::time_point timestamp;
        std::string unit;
        std::string description;
        std::unordered_map<std::string, std::string> tags;
        std::string component;
    };
    
    struct ComponentInfo {
        std::string name;
        std::string description;
        double healthScore;
        std::chrono::system_clock::time_point lastUpdate;
        std::vector<std::string> metrics;
    };
    
    struct AlertRule {
        std::string id;
        std::string metricName;
        std::string condition;
        double threshold;
        std::string severity;
        std::string message;
        std::chrono::system_clock::time_point lastTriggered;
        bool enabled;
    };
    
    // Core data
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_shutdown;
    
    // Configuration
    MonitoringConfig m_config;
    std::atomic<uint64_t> m_memoryLimit;
    std::atomic<uint32_t> m_maxMetrics;
    std::atomic<bool> m_resourceThrottlingEnabled;
    std::atomic<double> m_throttlingThreshold;
    
    // Metrics storage
    std::unordered_map<std::string, std::unique_ptr<MetricData>> m_metrics;
    std::mutex m_metricsMutex;
    
    // Components
    std::unordered_map<std::string, std::unique_ptr<ComponentInfo>> m_components;
    std::mutex m_componentsMutex;
    
    // Alerts
    std::vector<AlertRule> m_alertRules;
    std::vector<PerformanceAlert> m_activeAlerts;
    std::mutex m_alertsMutex;
    
    // Historical data
    std::queue<PerformanceReport> m_historicalReports;
    std::mutex m_reportsMutex;
    
    // Dashboard data
    DashboardData m_dashboardData;
    std::mutex m_dashboardMutex;
    
    // Health monitoring
    std::atomic<double> m_overallHealthScore;
    std::atomic<std::string> m_systemStatus;
    std::vector<std::string> m_topIssues;
    std::mutex m_healthMutex;
    
    // Event handlers
    std::function<void(const PerformanceAlert&)> m_alertHandler;
    std::function<void(double, double)> m_healthChangeHandler;
    std::function<void(const PerformanceMetric&)> m_metricHandler;
    
    // Monitoring thread
    std::thread m_monitoringThread;
    std::atomic<bool> m_monitoringActive;
    
    // Internal methods
    void monitoringThread();
    void updateMetrics();
    void checkAlerts();
    void updateHealthScores();
    void cleanupOldData();
    void generateReports();
    
    // Metric operations
    void addMetric(const std::string& name, MetricType type, double value, const std::string& unit, const std::unordered_map<std::string, std::string>& tags);
    void updateMetric(const std::string& name, double value);
    MetricData* getMetricData(const std::string& name);
    const MetricData* getMetricData(const std::string& name) const;
    
    // Alert operations
    void evaluateAlertRules();
    void triggerAlert(const AlertRule& rule, double currentValue);
    void resolveAlert(const std::string& alertId);
    bool isAlertCooldownActive(const AlertRule& rule) const;
    
    // Health operations
    void calculateOverallHealthScore();
    void updateComponentHealthScores();
    void identifyTopIssues();
    void updateSystemStatus();
    
    // Dashboard operations
    void updateDashboardData();
    void refreshRealTimeMetrics();
    void updateActiveAlerts();
    void calculateComponentHealthScores();
    
    // Low-end optimizations
    void reduceMemoryUsage();
    void limitMetricCount();
    void optimizeDataStructures();
    void enableResourceThrottling();
    
    // ARM64 optimizations
    void useNEONForCalculations();
    void optimizeMemoryLayout();
    void useCryptoExtensionsForHashing();
    void optimizeAlgorithms();
    
    // Utility methods
    std::string generateReportId() const;
    std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp) const;
    double calculateHealthScore(const std::vector<PerformanceMetric>& metrics) const;
    std::string getSeverityColor(const std::string& severity) const;
    void logPerformanceEvent(const std::string& event, const std::string& details);
};

} // namespace Advanced
} // namespace Common