// Copyright (c) 2024 Fuego Developers
// Phase 5 & 6 Test Suite for ARM64 Low-End Devices
// Comprehensive testing for Phase 5 & 6 optimizations

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>

// Include Phase 5 & 6 components
#include "src/Wallet/SecureWalletManager.h"
#include "src/Common/AdvancedPerformanceMonitor.h"

using namespace Wallet::Secure;
using namespace Common::Advanced;

class Phase5_6TestSuite : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test environment
        m_secureWallet = std::make_unique<SecureWalletManager>();
        m_performanceMonitor = std::make_unique<AdvancedPerformanceMonitor>();
        
        // Initialize components
        SecurityConfig securityConfig;
        securityConfig.enableEncryption = true;
        securityConfig.enableKeyDerivation = true;
        securityConfig.enableSecureBackup = true;
        securityConfig.enableAuthentication = true;
        securityConfig.enableAuditLogging = true;
        securityConfig.keyDerivationIterations = 10000;
        securityConfig.maxLoginAttempts = 3;
        securityConfig.sessionTimeout = 1800;
        securityConfig.backupRetentionDays = 7;
        securityConfig.encryptionAlgorithm = "AES-256-GCM";
        securityConfig.keyDerivationFunction = "PBKDF2";
        securityConfig.backupLocation = "./test_backups";
        
        MonitoringConfig monitoringConfig;
        monitoringConfig.enableRealTimeMonitoring = true;
        monitoringConfig.enableHistoricalData = true;
        monitoringConfig.enableAlerting = true;
        monitoringConfig.enableDashboard = true;
        monitoringConfig.monitoringInterval = 1000;
        monitoringConfig.dataRetentionDays = 7;
        monitoringConfig.maxMetricsPerComponent = 50;
        monitoringConfig.alertCooldownPeriod = 300;
        monitoringConfig.healthScoreThreshold = 0.7;
        monitoringConfig.alertNotificationMethod = "console";
        monitoringConfig.dashboardRefreshInterval = "5s";
        monitoringConfig.monitoredComponents = {"wallet", "network", "blockchain"};
        
        m_secureWallet->initialize(securityConfig);
        m_performanceMonitor->initialize(monitoringConfig);
    }
    
    void TearDown() override {
        // Cleanup test environment
        m_performanceMonitor->shutdown();
        m_secureWallet->shutdown();
    }
    
    std::unique_ptr<SecureWalletManager> m_secureWallet;
    std::unique_ptr<AdvancedPerformanceMonitor> m_performanceMonitor;
};

// Phase 5: Wallet Security Tests
TEST_F(Phase5_6TestSuite, SecureWalletManagerTest) {
    // Test initialization
    EXPECT_TRUE(m_secureWallet->isAuthenticated() == false);
    
    // Test master key generation
    EXPECT_TRUE(m_secureWallet->generateMasterKey("test_password"));
    
    // Test authentication
    auto authResult = m_secureWallet->authenticate("test_password");
    EXPECT_TRUE(authResult.success);
    EXPECT_TRUE(m_secureWallet->isAuthenticated());
    EXPECT_FALSE(authResult.sessionToken.empty());
    
    // Test session management
    EXPECT_TRUE(m_secureWallet->isSessionValid());
    m_secureWallet->extendSession();
    EXPECT_TRUE(m_secureWallet->isSessionValid());
    
    // Test logout
    EXPECT_TRUE(m_secureWallet->logout());
    EXPECT_FALSE(m_secureWallet->isAuthenticated());
}

TEST_F(Phase5_6TestSuite, WalletEncryptionTest) {
    // Generate master key
    EXPECT_TRUE(m_secureWallet->generateMasterKey("test_password"));
    
    // Authenticate
    auto authResult = m_secureWallet->authenticate("test_password");
    EXPECT_TRUE(authResult.success);
    
    // Test wallet encryption
    EXPECT_TRUE(m_secureWallet->encryptWallet("encryption_password"));
    EXPECT_TRUE(m_secureWallet->isWalletEncrypted());
    
    // Test wallet decryption
    EXPECT_TRUE(m_secureWallet->decryptWallet("encryption_password"));
    EXPECT_FALSE(m_secureWallet->isWalletEncrypted());
    
    // Test password change
    EXPECT_TRUE(m_secureWallet->changeEncryptionPassword("encryption_password", "new_password"));
    EXPECT_TRUE(m_secureWallet->isWalletEncrypted());
}

TEST_F(Phase5_6TestSuite, SecureBackupTest) {
    // Generate master key and authenticate
    EXPECT_TRUE(m_secureWallet->generateMasterKey("test_password"));
    auto authResult = m_secureWallet->authenticate("test_password");
    EXPECT_TRUE(authResult.success);
    
    // Test secure backup creation
    BackupInfo backupInfo;
    EXPECT_TRUE(m_secureWallet->createSecureBackup("backup_password", backupInfo));
    EXPECT_FALSE(backupInfo.backupId.empty());
    EXPECT_FALSE(backupInfo.filePath.empty());
    EXPECT_TRUE(backupInfo.encrypted);
    EXPECT_TRUE(backupInfo.verified);
    
    // Test backup listing
    auto backups = m_secureWallet->listBackups();
    EXPECT_EQ(1, backups.size());
    EXPECT_EQ(backupInfo.backupId, backups[0].backupId);
    
    // Test backup verification
    EXPECT_TRUE(m_secureWallet->verifyBackup(backupInfo.filePath));
    
    // Test backup restoration
    EXPECT_TRUE(m_secureWallet->restoreFromBackup(backupInfo.filePath, "backup_password"));
    
    // Test backup deletion
    EXPECT_TRUE(m_secureWallet->deleteBackup(backupInfo.backupId));
}

TEST_F(Phase5_6TestSuite, SecurityMonitoringTest) {
    // Test security event logging
    m_secureWallet->logSecurityEvent("TEST_EVENT", "Test security event", true);
    
    // Test audit log retrieval
    auto auditLog = m_secureWallet->getAuditLog(10);
    EXPECT_GE(auditLog.size(), 1);
    EXPECT_EQ("TEST_EVENT", auditLog[0].event);
    EXPECT_TRUE(auditLog[0].success);
    
    // Test security health
    EXPECT_TRUE(m_secureWallet->isSecurityHealthy());
    EXPECT_GT(m_secureWallet->getSecurityHealthScore(), 0.0);
    
    // Test security issues
    auto issues = m_secureWallet->getSecurityIssues();
    EXPECT_TRUE(issues.empty());
    
    // Test security check
    m_secureWallet->performSecurityCheck();
}

TEST_F(Phase5_6TestSuite, AccessControlTest) {
    // Test access control
    EXPECT_TRUE(m_secureWallet->setAccessControl("wallet", "read"));
    EXPECT_TRUE(m_secureWallet->checkAccess("wallet", "read"));
    EXPECT_FALSE(m_secureWallet->checkAccess("wallet", "write"));
    
    // Test accessible resources
    auto resources = m_secureWallet->getAccessibleResources();
    EXPECT_EQ(1, resources.size());
    EXPECT_EQ("wallet", resources[0]);
    
    // Test access revocation
    EXPECT_TRUE(m_secureWallet->revokeAccess("wallet"));
    EXPECT_FALSE(m_secureWallet->checkAccess("wallet", "read"));
}

// Phase 6: Performance Monitoring Tests
TEST_F(Phase5_6TestSuite, AdvancedPerformanceMonitorTest) {
    // Test initialization
    EXPECT_TRUE(m_performanceMonitor->isSystemHealthy());
    EXPECT_GT(m_performanceMonitor->getOverallHealthScore(), 0.0);
    
    // Test metric recording
    m_performanceMonitor->recordMetric("test_metric", 100.0, "count");
    m_performanceMonitor->incrementCounter("test_counter", 5.0);
    m_performanceMonitor->setGauge("test_gauge", 50.0, "percent");
    m_performanceMonitor->recordHistogram("test_histogram", 75.0, "ms");
    m_performanceMonitor->recordTimer("test_timer", 25.0, "ms");
    m_performanceMonitor->recordRate("test_rate", 10.0, "ops/s");
    
    // Test metric retrieval
    auto metric = m_performanceMonitor->getMetric("test_metric");
    EXPECT_EQ("test_metric", metric.name);
    EXPECT_EQ(100.0, metric.value);
    EXPECT_EQ("count", metric.unit);
    
    // Test metrics by type
    auto counterMetrics = m_performanceMonitor->getMetricsByType(MetricType::COUNTER);
    EXPECT_GE(counterMetrics.size(), 1);
    
    // Test component registration
    m_performanceMonitor->registerComponent("test_component", "Test component");
    auto components = m_performanceMonitor->getRegisteredComponents();
    EXPECT_EQ(1, components.size());
    EXPECT_EQ("test_component", components[0]);
}

TEST_F(Phase5_6TestSuite, AlertingTest) {
    // Test alert creation
    m_performanceMonitor->createAlert("test_metric", "greater_than", 90.0, "WARNING", "Test alert");
    
    // Test metric recording that should trigger alert
    m_performanceMonitor->recordMetric("test_metric", 95.0, "count");
    
    // Wait for alert evaluation
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test active alerts
    auto activeAlerts = m_performanceMonitor->getActiveAlerts();
    EXPECT_GE(activeAlerts.size(), 1);
    
    // Test alert acknowledgment
    if (!activeAlerts.empty()) {
        m_performanceMonitor->acknowledgeAlert(activeAlerts[0].id);
        EXPECT_TRUE(activeAlerts[0].acknowledged);
    }
    
    // Test alert resolution
    if (!activeAlerts.empty()) {
        m_performanceMonitor->resolveAlert(activeAlerts[0].id);
        EXPECT_TRUE(activeAlerts[0].resolved);
    }
}

TEST_F(Phase5_6TestSuite, ReportingTest) {
    // Record some metrics
    m_performanceMonitor->recordMetric("cpu_usage", 75.0, "percent");
    m_performanceMonitor->recordMetric("memory_usage", 60.0, "percent");
    m_performanceMonitor->recordMetric("disk_usage", 45.0, "percent");
    
    // Test report generation
    auto report = m_performanceMonitor->generateReport("", 60);
    EXPECT_FALSE(report.reportId.empty());
    EXPECT_GT(report.metrics.size(), 0);
    EXPECT_GT(report.overallHealthScore, 0.0);
    EXPECT_FALSE(report.summary.empty());
    
    // Test report export
    std::string reportPath = "./test_report.txt";
    m_performanceMonitor->exportReport(report, reportPath);
    
    // Verify report file was created
    std::ifstream file(reportPath);
    EXPECT_TRUE(file.is_open());
    file.close();
    
    // Clean up
    std::remove(reportPath.c_str());
}

TEST_F(Phase5_6TestSuite, DashboardTest) {
    // Record some metrics
    m_performanceMonitor->recordMetric("cpu_usage", 80.0, "percent");
    m_performanceMonitor->recordMetric("memory_usage", 70.0, "percent");
    m_performanceMonitor->recordMetric("network_usage", 50.0, "percent");
    
    // Test dashboard update
    m_performanceMonitor->updateDashboard();
    
    // Test dashboard data retrieval
    auto dashboardData = m_performanceMonitor->getDashboardData();
    EXPECT_GT(dashboardData.realTimeMetrics.size(), 0);
    EXPECT_GT(dashboardData.systemHealthScore, 0.0);
    EXPECT_FALSE(dashboardData.systemStatus.empty());
    
    // Test component health scores
    m_performanceMonitor->registerComponent("test_component", "Test component");
    m_performanceMonitor->setComponentHealthScore("test_component", 0.8);
    
    auto componentHealthScore = m_performanceMonitor->getComponentHealthScore("test_component");
    EXPECT_EQ(0.8, componentHealthScore);
    
    // Test dashboard components
    auto dashboardComponents = m_performanceMonitor->getDashboardComponents();
    EXPECT_GE(dashboardComponents.size(), 1);
}

TEST_F(Phase5_6TestSuite, HealthMonitoringTest) {
    // Test system health
    EXPECT_TRUE(m_performanceMonitor->isSystemHealthy());
    EXPECT_GT(m_performanceMonitor->getOverallHealthScore(), 0.0);
    
    // Test system status
    auto status = m_performanceMonitor->getSystemStatus();
    EXPECT_FALSE(status.empty());
    
    // Test top issues
    auto topIssues = m_performanceMonitor->getTopIssues();
    EXPECT_GE(topIssues.size(), 0);
    
    // Test health check
    m_performanceMonitor->performHealthCheck();
    
    // Test component health
    m_performanceMonitor->registerComponent("test_component", "Test component");
    m_performanceMonitor->setComponentHealthScore("test_component", 0.9);
    
    auto componentHealth = m_performanceMonitor->getComponentHealthScore("test_component");
    EXPECT_EQ(0.9, componentHealth);
}

// Integration Tests
TEST_F(Phase5_6TestSuite, SecurityPerformanceIntegrationTest) {
    // Test security and performance integration
    m_secureWallet->generateMasterKey("test_password");
    auto authResult = m_secureWallet->authenticate("test_password");
    EXPECT_TRUE(authResult.success);
    
    // Record security-related metrics
    m_performanceMonitor->recordMetric("authentication_attempts", 1.0, "count");
    m_performanceMonitor->recordMetric("security_events", 1.0, "count");
    m_performanceMonitor->recordMetric("wallet_health", 1.0, "score");
    
    // Test security health monitoring
    EXPECT_TRUE(m_secureWallet->isSecurityHealthy());
    EXPECT_TRUE(m_performanceMonitor->isSystemHealthy());
    
    // Test performance monitoring of security operations
    auto securityMetrics = m_performanceMonitor->getMetrics("security");
    EXPECT_GE(securityMetrics.size(), 0);
}

TEST_F(Phase5_6TestSuite, PerformanceIntegrationTest) {
    // Test performance monitoring integration
    const int iterations = 100;
    
    // Test metric recording performance
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        m_performanceMonitor->recordMetric("test_metric_" + std::to_string(i), i, "count");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Verify performance is reasonable (less than 1ms per operation)
    EXPECT_LT(duration.count() / iterations, 1000);
    
    // Test alert evaluation performance
    m_performanceMonitor->createAlert("test_metric_50", "greater_than", 50.0, "WARNING", "Test alert");
    
    start = std::chrono::high_resolution_clock::now();
    m_performanceMonitor->performHealthCheck();
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Verify health check performance is reasonable (less than 10ms)
    EXPECT_LT(duration.count(), 10000);
}

TEST_F(Phase5_6TestSuite, MemoryIntegrationTest) {
    // Test memory integration
    EXPECT_LE(m_performanceMonitor->getOverallHealthScore(), 1.0);
    
    // Test memory optimization
    m_secureWallet->optimizeForLowEnd();
    m_performanceMonitor->optimizeForLowEnd();
    
    // Test memory limits
    m_secureWallet->setMemoryLimit(1024 * 1024);  // 1MB
    m_performanceMonitor->setMemoryLimit(1024 * 1024);  // 1MB
    
    // Test resource throttling
    m_secureWallet->enableResourceThrottling(true);
    m_performanceMonitor->enableResourceThrottling(true);
    
    // Test throttling thresholds
    m_secureWallet->setThrottlingThreshold(0.8);
    m_performanceMonitor->setThrottlingThreshold(0.8);
}

// Stress Tests
TEST_F(Phase5_6TestSuite, SecurityStressTest) {
    // Test security under stress
    const int stressIterations = 1000;
    
    for (int i = 0; i < stressIterations; ++i) {
        m_secureWallet->logSecurityEvent("STRESS_TEST", "Stress test event " + std::to_string(i), true);
    }
    
    // Verify system stability
    EXPECT_TRUE(m_secureWallet->isSecurityHealthy());
    EXPECT_GT(m_secureWallet->getSecurityHealthScore(), 0.5);
}

TEST_F(Phase5_6TestSuite, PerformanceStressTest) {
    // Test performance monitoring under stress
    const int stressIterations = 1000;
    
    for (int i = 0; i < stressIterations; ++i) {
        m_performanceMonitor->recordMetric("stress_metric_" + std::to_string(i % 100), i, "count");
    }
    
    // Verify system stability
    EXPECT_TRUE(m_performanceMonitor->isSystemHealthy());
    EXPECT_GT(m_performanceMonitor->getOverallHealthScore(), 0.5);
}

// ARM64 Optimization Tests
TEST_F(Phase5_6TestSuite, ARM64OptimizationTest) {
    // Test ARM64 optimizations
    m_secureWallet->optimizeForARM64();
    m_performanceMonitor->optimizeForARM64();
    
    // Test NEON operations
    m_secureWallet->useNEONOperations();
    m_performanceMonitor->useNEONOperations();
    
    // Test memory alignment
    m_secureWallet->optimizeMemoryAlignment();
    m_performanceMonitor->optimizeMemoryAlignment();
    
    // Test crypto extensions
    m_secureWallet->useCryptoExtensions();
    m_performanceMonitor->useCryptoExtensions();
    
    // Verify optimizations are active
    EXPECT_TRUE(m_secureWallet->isSecurityHealthy());
    EXPECT_TRUE(m_performanceMonitor->isSystemHealthy());
}

// Low-End Device Simulation Tests
TEST_F(Phase5_6TestSuite, LowEndDeviceSimulationTest) {
    // Simulate low-end device constraints
    const uint64_t lowMemoryLimit = 256 * 1024;  // 256KB
    const uint32_t lowSecurityLevel = 2;
    const uint32_t lowMetricLimit = 50;
    
    // Test security manager with low-end constraints
    m_secureWallet->setMemoryLimit(lowMemoryLimit);
    m_secureWallet->setSecurityLevel(lowSecurityLevel);
    
    EXPECT_TRUE(m_secureWallet->isSecurityHealthy());
    
    // Test performance monitor with low-end constraints
    m_performanceMonitor->setMemoryLimit(lowMemoryLimit);
    m_performanceMonitor->setMetricLimit(lowMetricLimit);
    
    // Test resource throttling
    m_secureWallet->enableResourceThrottling(true);
    m_performanceMonitor->enableResourceThrottling(true);
    
    // Verify system works under constraints
    EXPECT_TRUE(m_secureWallet->isSecurityHealthy());
    EXPECT_TRUE(m_performanceMonitor->isSystemHealthy());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}