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

#include "FableAbleDepositSystem.h"
#include "Common/Logging.h"
#include "crypto/hash.h"
#include "crypto/randomize.h"
#include <algorithm>
#include <chrono>

namespace CryptoNote {

// FableAbleDepositData implementation
bool FableAbleDepositData::isValid() const {
    return depositId != NULL_HASH &&
           depositAmount > 0 &&
           !depositorAddress.empty() &&
           !collateralAsset.empty() &&
           !stabilityTargetAsset.empty() &&
           maturityTimestamp > depositTimestamp &&
           minCollateralRatio <= maxCollateralRatio &&
           liquidationThreshold > 0 &&
           liquidationThreshold < minCollateralRatio &&
           !signature.empty();
}

bool FableAbleDepositData::isMature() const {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return now >= maturityTimestamp;
}

bool FableAbleDepositData::isLiquidatable() const {
    return status == DepositStatus::ACTIVE &&
           collateralRatio < liquidationThreshold &&
           isMature();
}

bool FableAbleDepositData::isStable() const {
    return priceDeviation <= 0.05 && // 5% deviation threshold
           collateralRatio >= minCollateralRatio &&
           collateralRatio <= maxCollateralRatio;
}

double FableAbleDepositData::calculateStabilityScore() const {
    if (targetPrice == 0) return 0.0;
    
    double priceStability = 1.0 - (priceDeviation / 100.0);
    double collateralStability = std::min(1.0, collateralRatio / (minCollateralRatio / 100.0));
    
    return (priceStability * 0.6 + collateralStability * 0.4) * 100.0;
}

double FableAbleDepositData::calculateCollateralRatio() const {
    if (depositAmount == 0) return 0.0;
    return (static_cast<double>(collateralAmount) / static_cast<double>(depositAmount)) * 100.0;
}

std::string FableAbleDepositData::toString() const {
    return "FableAbleDepositData{id=" + Common::podToHex(depositId) + 
           ", type=" + std::to_string(static_cast<uint8_t>(depositType)) +
           ", amount=" + std::to_string(depositAmount) +
           ", collateral=" + std::to_string(collateralAmount) +
           ", ratio=" + std::to_string(collateralRatio) + "%" +
           ", stable=" + (isStable() ? "true" : "false") + "}";
}

// StabilityPoolData implementation
bool StabilityPoolData::isValid() const {
    return poolId != NULL_HASH &&
           !poolName.empty() &&
           totalDeposits >= 0 &&
           totalCollateral >= 0 &&
           !activeDeposits.empty();
}

double StabilityPoolData::calculatePoolHealth() const {
    if (totalDeposits == 0) return 0.0;
    
    double collateralHealth = static_cast<double>(totalCollateral) / static_cast<double>(totalDeposits);
    double stabilityHealth = currentStabilityScore / 100.0;
    
    return (collateralHealth * 0.7 + stabilityHealth * 0.3) * 100.0;
}

std::string StabilityPoolData::toString() const {
    return "StabilityPoolData{id=" + Common::podToHex(poolId) +
           ", name=" + poolName +
           ", deposits=" + std::to_string(totalDeposits) +
           ", collateral=" + std::to_string(totalCollateral) +
           ", health=" + std::to_string(calculatePoolHealth()) + "%" + "}";
}

// LiquidationEvent implementation
bool LiquidationEvent::isValid() const {
    return eventId != NULL_HASH &&
           depositId != NULL_HASH &&
           liquidatedAmount > 0 &&
           !liquidatorAddress.empty() &&
           !reason.empty() &&
           !evidence.empty();
}

std::string LiquidationEvent::toString() const {
    return "LiquidationEvent{id=" + Common::podToHex(eventId) +
           ", deposit=" + Common::podToHex(depositId) +
           ", amount=" + std::to_string(liquidatedAmount) +
           ", recovered=" + std::to_string(collateralRecovered) +
           ", reason=" + reason + "}";
}

// FableAbleStabilityConfig implementation
FableAbleStabilityConfig FableAbleStabilityConfig::getDefault() {
    FableAbleStabilityConfig config;
    config.minDepositAmount = 1000000000;        // 1 XFG
    config.maxDepositAmount = 1000000000000;     // 1000 XFG
    config.minCollateralAmount = 500000000;      // 0.5 XFG
    config.maxCollateralAmount = 500000000000;   // 500 XFG
    config.priceDeviationThreshold = 5.0;        // 5%
    config.minCollateralRatio = 15000;           // 150% (15000 basis points)
    config.maxCollateralRatio = 50000;           // 500% (50000 basis points)
    config.liquidationThreshold = 12000;         // 120% (12000 basis points)
    config.depositMaturityTime = 86400;          // 24 hours
    config.liquidationGracePeriod = 3600;        // 1 hour
    config.stabilityUpdateInterval = 300;        // 5 minutes
    config.stabilityFeeRate = 100;               // 1% (100 basis points)
    config.liquidationPenalty = 500;             // 5% (500 basis points)
    config.governanceThreshold = 1000000000;     // 1 XFG
    config.consensusThreshold = 3;               // 3/5 consensus
    config.maxLiquidationsPerBlock = 10;
    config.enableAutomaticLiquidation = true;
    config.enableGovernanceVoting = true;
    return config;
}

bool FableAbleStabilityConfig::isValid() const {
    return minDepositAmount > 0 &&
           maxDepositAmount >= minDepositAmount &&
           minCollateralAmount > 0 &&
           maxCollateralAmount >= minCollateralAmount &&
           priceDeviationThreshold > 0 &&
           minCollateralRatio > 0 &&
           maxCollateralRatio >= minCollateralRatio &&
           liquidationThreshold > 0 &&
           liquidationThreshold < minCollateralRatio &&
           depositMaturityTime > 0 &&
           liquidationGracePeriod > 0 &&
           stabilityUpdateInterval > 0 &&
           stabilityFeeRate >= 0 &&
           liquidationPenalty >= 0 &&
           governanceThreshold > 0 &&
           consensusThreshold > 0 &&
           maxLiquidationsPerBlock > 0;
}

// StabilityMetrics implementation
bool StabilityMetrics::isValid() const {
    return totalDeposits >= 0 &&
           totalCollateral >= 0 &&
           totalLiquidations >= 0 &&
           averageStabilityScore >= 0 &&
           averageStabilityScore <= 100 &&
           averageCollateralRatio >= 0 &&
           priceStabilityIndex >= 0 &&
           priceStabilityIndex <= 100 &&
           activeDeposits >= 0 &&
           liquidatedDeposits >= 0;
}

std::string StabilityMetrics::toString() const {
    return "StabilityMetrics{deposits=" + std::to_string(totalDeposits) +
           ", collateral=" + std::to_string(totalCollateral) +
           ", liquidations=" + std::to_string(totalLiquidations) +
           ", avgStability=" + std::to_string(averageStabilityScore) +
           ", avgCollateral=" + std::to_string(averageCollateralRatio) +
           ", priceStability=" + std::to_string(priceStabilityIndex) + "}";
}

// FableAbleDepositManager implementation
FableAbleDepositManager::FableAbleDepositManager(Logging::ILogger& logger)
    : logger(logger, "FableAbleDepositManager") {
    m_config = FableAbleStabilityConfig::getDefault();
    m_metrics = StabilityMetrics{};
}

FableAbleDepositManager::~FableAbleDepositManager() = default;

bool FableAbleDepositManager::createDeposit(const FableAbleDepositData& deposit) {
    if (!validateDeposit(deposit)) {
        logger(Logging::WARNING) << "Invalid deposit data";
        return false;
    }
    
    if (!verifyDepositSignature(deposit)) {
        logger(Logging::WARNING) << "Invalid deposit signature";
        return false;
    }
    
    Crypto::Hash depositId = generateDepositId(deposit);
    m_deposits[depositId] = deposit;
    
    logger(Logging::INFO) << "Created deposit: " << deposit.toString();
    return true;
}

bool FableAbleDepositManager::updateDeposit(const Crypto::Hash& depositId, const FableAbleDepositData& deposit) {
    auto it = m_deposits.find(depositId);
    if (it == m_deposits.end()) {
        logger(Logging::WARNING) << "Deposit not found: " << Common::podToHex(depositId);
        return false;
    }
    
    if (!validateDeposit(deposit)) {
        logger(Logging::WARNING) << "Invalid deposit data for update";
        return false;
    }
    
    it->second = deposit;
    updateDepositStability(it->second);
    
    logger(Logging::INFO) << "Updated deposit: " << deposit.toString();
    return true;
}

bool FableAbleDepositManager::liquidateDeposit(const Crypto::Hash& depositId, const std::string& reason) {
    auto it = m_deposits.find(depositId);
    if (it == m_deposits.end()) {
        logger(Logging::WARNING) << "Deposit not found for liquidation: " << Common::podToHex(depositId);
        return false;
    }
    
    if (!checkDepositLiquidation(it->second)) {
        logger(Logging::WARNING) << "Deposit not eligible for liquidation";
        return false;
    }
    
    processLiquidation(depositId, reason);
    
    logger(Logging::INFO) << "Liquidated deposit: " << Common::podToHex(depositId) << " - " << reason;
    return true;
}

bool FableAbleDepositManager::withdrawDeposit(const Crypto::Hash& depositId) {
    auto it = m_deposits.find(depositId);
    if (it == m_deposits.end()) {
        logger(Logging::WARNING) << "Deposit not found for withdrawal: " << Common::podToHex(depositId);
        return false;
    }
    
    if (it->second.status != DepositStatus::ACTIVE) {
        logger(Logging::WARNING) << "Deposit not active for withdrawal";
        return false;
    }
    
    if (!it->second.isMature()) {
        logger(Logging::WARNING) << "Deposit not mature for withdrawal";
        return false;
    }
    
    it->second.status = DepositStatus::WITHDRAWN;
    
    logger(Logging::INFO) << "Withdrew deposit: " << Common::podToHex(depositId);
    return true;
}

std::optional<FableAbleDepositData> FableAbleDepositManager::getDeposit(const Crypto::Hash& depositId) const {
    auto it = m_deposits.find(depositId);
    if (it == m_deposits.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<FableAbleDepositData> FableAbleDepositManager::getDepositsByAddress(const std::string& address) const {
    std::vector<FableAbleDepositData> result;
    for (const auto& pair : m_deposits) {
        if (pair.second.depositorAddress == address) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<FableAbleDepositData> FableAbleDepositManager::getDepositsByType(FableAbleDepositType type) const {
    std::vector<FableAbleDepositData> result;
    for (const auto& pair : m_deposits) {
        if (pair.second.depositType == type) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<FableAbleDepositData> FableAbleDepositManager::getActiveDeposits() const {
    std::vector<FableAbleDepositData> result;
    for (const auto& pair : m_deposits) {
        if (pair.second.status == DepositStatus::ACTIVE) {
            result.push_back(pair.second);
        }
    }
    return result;
}

bool FableAbleDepositManager::updateStabilityMetrics() {
    updateStabilityMetricsInternal();
    return true;
}

StabilityMetrics FableAbleDepositManager::getStabilityMetrics() const {
    return m_metrics;
}

bool FableAbleDepositManager::checkStabilityThresholds() {
    bool allStable = true;
    for (auto& pair : m_deposits) {
        if (pair.second.status == DepositStatus::ACTIVE) {
            updateDepositStability(pair.second);
            if (!pair.second.isStable()) {
                allStable = false;
            }
        }
    }
    return allStable;
}

std::vector<Crypto::Hash> FableAbleDepositManager::getLiquidatableDeposits() const {
    std::vector<Crypto::Hash> result;
    for (const auto& pair : m_deposits) {
        if (pair.second.isLiquidatable()) {
            result.push_back(pair.first);
        }
    }
    return result;
}

bool FableAbleDepositManager::createStabilityPool(const StabilityPoolData& pool) {
    if (!validateStabilityPool(pool)) {
        logger(Logging::WARNING) << "Invalid stability pool data";
        return false;
    }
    
    m_stabilityPools[pool.poolId] = pool;
    
    logger(Logging::INFO) << "Created stability pool: " << pool.toString();
    return true;
}

std::optional<StabilityPoolData> FableAbleDepositManager::getStabilityPool(const Crypto::Hash& poolId) const {
    auto it = m_stabilityPools.find(poolId);
    if (it == m_stabilityPools.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<StabilityPoolData> FableAbleDepositManager::getAllStabilityPools() const {
    std::vector<StabilityPoolData> result;
    for (const auto& pair : m_stabilityPools) {
        result.push_back(pair.second);
    }
    return result;
}

void FableAbleDepositManager::setStabilityConfig(const FableAbleStabilityConfig& config) {
    if (!config.isValid()) {
        logger(Logging::WARNING) << "Invalid stability configuration";
        return;
    }
    m_config = config;
}

FableAbleStabilityConfig FableAbleDepositManager::getStabilityConfig() const {
    return m_config;
}

// Private helper methods
bool FableAbleDepositManager::validateDeposit(const FableAbleDepositData& deposit) const {
    if (!deposit.isValid()) {
        return false;
    }
    
    if (deposit.depositAmount < m_config.minDepositAmount ||
        deposit.depositAmount > m_config.maxDepositAmount) {
        return false;
    }
    
    if (deposit.collateralAmount < m_config.minCollateralAmount ||
        deposit.collateralAmount > m_config.maxCollateralAmount) {
        return false;
    }
    
    return true;
}

bool FableAbleDepositManager::validateStabilityPool(const StabilityPoolData& pool) const {
    return pool.isValid();
}

bool FableAbleDepositManager::checkDepositLiquidation(const FableAbleDepositData& deposit) const {
    return deposit.isLiquidatable();
}

void FableAbleDepositManager::updateDepositStability(FableAbleDepositData& deposit) {
    // Update price deviation
    if (deposit.targetPrice > 0) {
        deposit.priceDeviation = std::abs(deposit.currentPrice - deposit.targetPrice) / deposit.targetPrice * 100.0;
    }
    
    // Update collateral ratio
    deposit.collateralRatio = deposit.calculateCollateralRatio();
    
    // Update stability score
    deposit.stabilityScore = deposit.calculateStabilityScore();
    
    // Update timestamp
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    deposit.lastUpdateTimestamp = now;
}

void FableAbleDepositManager::processLiquidation(const Crypto::Hash& depositId, const std::string& reason) {
    auto it = m_deposits.find(depositId);
    if (it == m_deposits.end()) {
        return;
    }
    
    // Create liquidation event
    LiquidationEvent event;
    event.eventId = Crypto::Hash::random();
    event.depositId = depositId;
    event.liquidatedAmount = it->second.depositAmount;
    event.collateralRecovered = it->second.collateralAmount;
    event.liquidationTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    event.liquidatorAddress = "system"; // In real implementation, this would be the actual liquidator
    event.reason = reason;
    
    m_liquidationEvents.push_back(event);
    
    // Update deposit status
    it->second.status = DepositStatus::LIQUIDATED;
    
    // Update metrics
    m_metrics.totalLiquidations++;
    m_metrics.liquidatedDeposits++;
    m_metrics.activeDeposits--;
}

void FableAbleDepositManager::updateStabilityMetricsInternal() {
    m_metrics.totalDeposits = 0;
    m_metrics.totalCollateral = 0;
    m_metrics.activeDeposits = 0;
    m_metrics.liquidatedDeposits = 0;
    
    double totalStabilityScore = 0.0;
    double totalCollateralRatio = 0.0;
    int activeCount = 0;
    
    for (const auto& pair : m_deposits) {
        const auto& deposit = pair.second;
        
        m_metrics.totalDeposits += deposit.depositAmount;
        m_metrics.totalCollateral += deposit.collateralAmount;
        
        if (deposit.status == DepositStatus::ACTIVE) {
            m_metrics.activeDeposits++;
            totalStabilityScore += deposit.stabilityScore;
            totalCollateralRatio += deposit.collateralRatio;
            activeCount++;
        } else if (deposit.status == DepositStatus::LIQUIDATED) {
            m_metrics.liquidatedDeposits++;
        }
    }
    
    if (activeCount > 0) {
        m_metrics.averageStabilityScore = totalStabilityScore / activeCount;
        m_metrics.averageCollateralRatio = totalCollateralRatio / activeCount;
    }
    
    // Calculate price stability index (simplified)
    m_metrics.priceStabilityIndex = 100.0 - (m_metrics.averageStabilityScore / 10.0);
    if (m_metrics.priceStabilityIndex < 0) m_metrics.priceStabilityIndex = 0;
    if (m_metrics.priceStabilityIndex > 100) m_metrics.priceStabilityIndex = 100;
    
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    m_metrics.lastUpdateTimestamp = now;
}

Crypto::Hash FableAbleDepositManager::generateDepositId(const FableAbleDepositData& deposit) const {
    std::string data = deposit.depositorAddress +
                      std::to_string(deposit.depositAmount) +
                      std::to_string(deposit.depositTimestamp) +
                      std::to_string(static_cast<uint8_t>(deposit.depositType));
    
    Crypto::Hash hash;
    Crypto::cn_fast_hash(data.data(), data.size(), hash.data);
    return hash;
}

bool FableAbleDepositManager::verifyDepositSignature(const FableAbleDepositData& deposit) const {
    // In a real implementation, this would verify the cryptographic signature
    // For now, we just check that the signature is not empty
    return !deposit.signature.empty();
}

} // namespace CryptoNote