// Copyright (c) 2017-2025 Elderfire Privacy Council
//
// This file is part of Fuego.
//
// Fuego is free software distributed in the hope that it
// will be useful, but WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. You can redistribute it and/or modify it under the terms
// of the GNU General Public License v3 or later versions as published
// by the Free Software Foundation. Fuego includes elements written
// by third parties. See file labeled LICENSE for more details.
// You should have received a copy of the GNU General Public License
// along with Fuego. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <atomic>
#include "crypto/hash.h"
#include "crypto/crypto.h"
#include "Common/StringTools.h"
#include "CryptoNote.h"

namespace CryptoNote {

// Pricing Source Types
enum class PricingSourceType : uint8_t {
    DEX_AGGREGATOR = 0,      // DEX aggregator (1inch, 0x, etc.)
    CEX_API = 1,             // Centralized exchange API
    CHAINLINK_ORACLE = 2,    // Chainlink price feeds
    BAND_PROTOCOL = 3,       // Band Protocol oracles
    POKT_NETWORK = 4,        // Pocket Network oracles
    CUSTOM_ORACLE = 5,       // Custom oracle implementation
    TWAP_CALCULATOR = 6,     // Time-weighted average price
    VWAP_CALCULATOR = 7,     // Volume-weighted average price
    MEDIAN_PRICE = 8,        // Median of multiple sources
    FALLBACK_SOURCE = 9      // Fallback pricing source
};

// Price Data Structure
struct PriceData {
    std::string assetId;             // Asset identifier (e.g., "XFG", "ETH", "USDC")
    double price;                    // Price in USD
    double volume24h;                // 24-hour volume
    double marketCap;                // Market capitalization
    uint64_t timestamp;              // Price timestamp
    uint64_t blockHeight;            // Block height when price was recorded
    std::string sourceId;            // Pricing source identifier
    PricingSourceType sourceType;    // Type of pricing source
    double confidence;               // Price confidence (0.0 - 1.0)
    bool isStale;                    // Whether price is stale
    std::vector<uint8_t> signature;  // Price signature (if applicable)
    
    bool isValid() const;
    bool isStalePrice(uint64_t maxAgeSeconds = 300) const; // 5 minutes default
    std::string toString() const;
};

// Pricing Source Configuration
struct PricingSourceConfig {
    std::string sourceId;
    std::string sourceName;
    PricingSourceType sourceType;
    std::string apiEndpoint;
    std::string apiKey;              // API key (if required)
    std::string contractAddress;     // Smart contract address (for oracles)
    uint64_t updateInterval;         // Update interval in seconds
    uint64_t timeoutMs;              // Request timeout in milliseconds
    double weight;                   // Weight in price aggregation (0.0 - 1.0)
    bool isEnabled;                  // Whether source is enabled
    uint64_t maxRetries;             // Maximum retry attempts
    uint64_t retryDelayMs;           // Delay between retries in milliseconds
    
    // Oracle-specific configuration
    std::string oracleAddress;       // Oracle contract address
    std::string priceFeedId;         // Price feed identifier
    uint64_t minConfirmations;       // Minimum confirmations required
    
    // DEX-specific configuration
    std::string dexRouterAddress;    // DEX router address
    std::string tokenAddress;        // Token contract address
    std::string wethAddress;         // WETH address for swaps
    
    bool isValid() const;
    std::string toString() const;
};

// Price Aggregation Strategy
enum class PriceAggregationStrategy : uint8_t {
    SIMPLE_AVERAGE = 0,      // Simple arithmetic mean
    WEIGHTED_AVERAGE = 1,    // Weighted average based on source weights
    MEDIAN_PRICE = 2,        // Median price from all sources
    TWAP = 3,                // Time-weighted average price
    VWAP = 4,                // Volume-weighted average price
    OUTLIER_REJECTION = 5,   // Reject outliers and average remaining
    CONSENSUS_PRICE = 6      // Consensus-based price selection
};

// Price Aggregation Configuration
struct PriceAggregationConfig {
    PriceAggregationStrategy strategy;
    double outlierThreshold;         // Outlier rejection threshold (standard deviations)
    uint64_t twapWindow;            // TWAP window in seconds
    uint64_t vwapWindow;            // VWAP window in seconds
    uint32_t minSources;            // Minimum number of sources required
    uint32_t maxSources;            // Maximum number of sources to use
    double minConfidence;           // Minimum confidence threshold
    bool enableOutlierRejection;    // Enable outlier rejection
    bool enableStalenessCheck;      // Enable staleness checking
    uint64_t maxStalenessSeconds;   // Maximum staleness in seconds
    
    static PriceAggregationConfig getDefault();
    bool isValid() const;
};

// Price Update Event
struct PriceUpdateEvent {
    std::string assetId;
    double oldPrice;
    double newPrice;
    double priceChange;
    double priceChangePercent;
    uint64_t timestamp;
    std::string sourceId;
    std::vector<std::string> affectedDeposits;
    
    bool isValid() const;
    std::string toString() const;
};

// Pricing System Interface
class IFableAblePricingSystem {
public:
    virtual ~IFableAblePricingSystem() = default;
    
    // Price management
    virtual bool updatePrice(const std::string& assetId, const PriceData& priceData) = 0;
    virtual std::optional<PriceData> getPrice(const std::string& assetId) const = 0;
    virtual std::vector<PriceData> getAllPrices() const = 0;
    virtual bool refreshPrices() = 0;
    
    // Price aggregation
    virtual std::optional<PriceData> getAggregatedPrice(const std::string& assetId) const = 0;
    virtual std::vector<PriceData> getAggregatedPrices(const std::vector<std::string>& assetIds) const = 0;
    
    // Source management
    virtual bool addPricingSource(const PricingSourceConfig& config) = 0;
    virtual bool removePricingSource(const std::string& sourceId) = 0;
    virtual bool updatePricingSource(const PricingSourceConfig& config) = 0;
    virtual std::vector<PricingSourceConfig> getPricingSources() const = 0;
    
    // Configuration
    virtual void setAggregationConfig(const PriceAggregationConfig& config) = 0;
    virtual PriceAggregationConfig getAggregationConfig() const = 0;
    
    // Event handling
    virtual void setPriceUpdateCallback(std::function<void(const PriceUpdateEvent&)> callback) = 0;
    virtual void setErrorCallback(std::function<void(const std::string&)> callback) = 0;
    
    // Health monitoring
    virtual bool isHealthy() const = 0;
    virtual std::vector<std::string> getUnhealthySources() const = 0;
    virtual double getSystemConfidence() const = 0;
};

// Pricing System Implementation
class FableAblePricingSystem : public IFableAblePricingSystem {
public:
    explicit FableAblePricingSystem(Logging::ILogger& logger);
    ~FableAblePricingSystem();
    
    // Price management
    bool updatePrice(const std::string& assetId, const PriceData& priceData) override;
    std::optional<PriceData> getPrice(const std::string& assetId) const override;
    std::vector<PriceData> getAllPrices() const override;
    bool refreshPrices() override;
    
    // Price aggregation
    std::optional<PriceData> getAggregatedPrice(const std::string& assetId) const override;
    std::vector<PriceData> getAggregatedPrices(const std::vector<std::string>& assetIds) const override;
    
    // Source management
    bool addPricingSource(const PricingSourceConfig& config) override;
    bool removePricingSource(const std::string& sourceId) override;
    bool updatePricingSource(const PricingSourceConfig& config) override;
    std::vector<PricingSourceConfig> getPricingSources() const override;
    
    // Configuration
    void setAggregationConfig(const PriceAggregationConfig& config) override;
    PriceAggregationConfig getAggregationConfig() const override;
    
    // Event handling
    void setPriceUpdateCallback(std::function<void(const PriceUpdateEvent&)> callback) override;
    void setErrorCallback(std::function<void(const std::string&)> callback) override;
    
    // Health monitoring
    bool isHealthy() const override;
    std::vector<std::string> getUnhealthySources() const override;
    double getSystemConfidence() const override;

private:
    Logging::LoggerRef logger;
    mutable std::mutex m_mutex;
    
    // Price data storage
    std::unordered_map<std::string, PriceData> m_prices;
    std::unordered_map<std::string, std::vector<PriceData>> m_priceHistory;
    
    // Source management
    std::unordered_map<std::string, PricingSourceConfig> m_sources;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_lastUpdate;
    std::unordered_map<std::string, uint32_t> m_failureCount;
    
    // Configuration
    PriceAggregationConfig m_aggregationConfig;
    
    // Event callbacks
    std::function<void(const PriceUpdateEvent&)> m_priceUpdateCallback;
    std::function<void(const std::string&)> m_errorCallback;
    
    // Background update thread
    std::atomic<bool> m_running;
    std::thread m_updateThread;
    
    // Helper methods
    bool fetchPriceFromSource(const PricingSourceConfig& source, const std::string& assetId);
    PriceData aggregatePrices(const std::string& assetId, const std::vector<PriceData>& prices) const;
    bool isPriceValid(const PriceData& price) const;
    bool isSourceHealthy(const std::string& sourceId) const;
    void updateThreadFunction();
    void handlePriceUpdate(const std::string& assetId, const PriceData& newPrice);
    void handleError(const std::string& error);
    
    // Price aggregation algorithms
    PriceData calculateSimpleAverage(const std::vector<PriceData>& prices) const;
    PriceData calculateWeightedAverage(const std::vector<PriceData>& prices) const;
    PriceData calculateMedianPrice(const std::vector<PriceData>& prices) const;
    PriceData calculateTWAP(const std::string& assetId, const std::vector<PriceData>& prices) const;
    PriceData calculateVWAP(const std::string& assetId, const std::vector<PriceData>& prices) const;
    PriceData calculateOutlierRejection(const std::vector<PriceData>& prices) const;
    PriceData calculateConsensusPrice(const std::vector<PriceData>& prices) const;
    
    // Validation methods
    bool validatePriceData(const PriceData& price) const;
    bool validateSourceConfig(const PricingSourceConfig& config) const;
    bool isOutlier(const PriceData& price, const std::vector<PriceData>& prices) const;
};

// Chainlink Oracle Integration
class ChainlinkOracleIntegration {
public:
    explicit ChainlinkOracleIntegration(Logging::ILogger& logger);
    ~ChainlinkOracleIntegration();
    
    bool initialize(const std::string& contractAddress, const std::string& rpcEndpoint);
    std::optional<PriceData> getPrice(const std::string& priceFeedId) const;
    bool isPriceFeedValid(const std::string& priceFeedId) const;
    uint64_t getLatestRoundId(const std::string& priceFeedId) const;
    
private:
    Logging::LoggerRef logger;
    std::string m_contractAddress;
    std::string m_rpcEndpoint;
    bool m_initialized;
    
    // Helper methods
    std::string callContract(const std::string& method, const std::vector<std::string>& params) const;
    PriceData parsePriceResponse(const std::string& response) const;
};

// DEX Aggregator Integration
class DEXAggregatorIntegration {
public:
    explicit DEXAggregatorIntegration(Logging::ILogger& logger);
    ~DEXAggregatorIntegration();
    
    bool initialize(const std::string& apiEndpoint, const std::string& apiKey = "");
    std::optional<PriceData> getPrice(const std::string& tokenAddress, const std::string& quoteToken = "0x0000000000000000000000000000000000000000") const;
    std::vector<std::string> getSupportedTokens() const;
    
private:
    Logging::LoggerRef logger;
    std::string m_apiEndpoint;
    std::string m_apiKey;
    bool m_initialized;
    
    // Helper methods
    std::string makeApiRequest(const std::string& endpoint, const std::unordered_map<std::string, std::string>& params) const;
    PriceData parseApiResponse(const std::string& response) const;
};

// Price Validator
class FableAblePriceValidator {
public:
    explicit FableAblePriceValidator(Logging::ILogger& logger);
    ~FableAblePriceValidator();
    
    bool validatePrice(const PriceData& price) const;
    bool validatePriceChange(const PriceData& oldPrice, const PriceData& newPrice) const;
    bool validatePriceSource(const PricingSourceConfig& source) const;
    bool validateAggregationConfig(const PriceAggregationConfig& config) const;
    
    // Price stability checks
    bool isPriceStable(const std::string& assetId, const std::vector<PriceData>& prices) const;
    bool isPriceWithinRange(const PriceData& price, double minPrice, double maxPrice) const;
    bool isPriceChangeReasonable(const PriceData& oldPrice, const PriceData& newPrice, double maxChangePercent) const;
    
private:
    Logging::LoggerRef logger;
    
    // Helper methods
    double calculatePriceVolatility(const std::vector<PriceData>& prices) const;
    bool isPriceAnomaly(const PriceData& price, const std::vector<PriceData>& historicalPrices) const;
    double calculatePriceDeviation(const PriceData& price, const std::vector<PriceData>& prices) const;
};

} // namespace CryptoNote