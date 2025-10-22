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
#include <cmath>

namespace CryptoNote {

FableAbleStabilityValidator::FableAbleStabilityValidator(Logging::ILogger& logger)
    : logger(logger, "FableAbleStabilityValidator") {
    m_config = FableAbleStabilityConfig::getDefault();
}

FableAbleStabilityValidator::~FableAbleStabilityValidator() = default;

bool FableAbleStabilityValidator::validateDepositData(const FableAbleDepositData& deposit) const {
    if (!deposit.isValid()) {
        logger(Logging::WARNING) << "Invalid deposit data structure";
        return false;
    }
    
    // Validate deposit amount
    if (deposit.depositAmount < m_config.minDepositAmount ||
        deposit.depositAmount > m_config.maxDepositAmount) {
        logger(Logging::WARNING) << "Deposit amount out of range: " << deposit.depositAmount;
        return false;
    }
    
    // Validate collateral amount
    if (deposit.collateralAmount < m_config.minCollateralAmount ||
        deposit.collateralAmount > m_config.maxCollateralAmount) {
        logger(Logging::WARNING) << "Collateral amount out of range: " << deposit.collateralAmount;
        return false;
    }
    
    // Validate collateral ratio
    double collateralRatio = deposit.calculateCollateralRatio();
    if (collateralRatio < (m_config.minCollateralRatio / 100.0) ||
        collateralRatio > (m_config.maxCollateralRatio / 100.0)) {
        logger(Logging::WARNING) << "Collateral ratio out of range: " << collateralRatio;
        return false;
    }
    
    // Validate liquidation threshold
    if (deposit.liquidationThreshold >= m_config.minCollateralRatio) {
        logger(Logging::WARNING) << "Liquidation threshold too high: " << deposit.liquidationThreshold;
        return false;
    }
    
    // Validate timestamps
    if (deposit.maturityTimestamp <= deposit.depositTimestamp) {
        logger(Logging::WARNING) << "Invalid maturity timestamp";
        return false;
    }
    
    return true;
}

bool FableAbleStabilityValidator::validateStabilityPool(const StabilityPoolData& pool) const {
    if (!pool.isValid()) {
        logger(Logging::WARNING) << "Invalid stability pool data structure";
        return false;
    }
    
    // Validate pool name
    if (pool.poolName.empty() || pool.poolName.length() > 64) {
        logger(Logging::WARNING) << "Invalid pool name";
        return false;
    }
    
    // Validate pool amounts
    if (pool.totalDeposits < 0 || pool.totalCollateral < 0) {
        logger(Logging::WARNING) << "Invalid pool amounts";
        return false;
    }
    
    // Validate stability score
    if (pool.currentStabilityScore < 0 || pool.currentStabilityScore > 100) {
        logger(Logging::WARNING) << "Invalid stability score: " << pool.currentStabilityScore;
        return false;
    }
    
    return true;
}

bool FableAbleStabilityValidator::validateLiquidationEvent(const LiquidationEvent& event) const {
    if (!event.isValid()) {
        logger(Logging::WARNING) << "Invalid liquidation event data structure";
        return false;
    }
    
    // Validate amounts
    if (event.liquidatedAmount <= 0) {
        logger(Logging::WARNING) << "Invalid liquidated amount: " << event.liquidatedAmount;
        return false;
    }
    
    if (event.collateralRecovered < 0) {
        logger(Logging::WARNING) << "Invalid collateral recovered: " << event.collateralRecovered;
        return false;
    }
    
    // Validate liquidator address
    if (event.liquidatorAddress.empty()) {
        logger(Logging::WARNING) << "Empty liquidator address";
        return false;
    }
    
    // Validate reason
    if (event.reason.empty() || event.reason.length() > 256) {
        logger(Logging::WARNING) << "Invalid liquidation reason";
        return false;
    }
    
    return true;
}

bool FableAbleStabilityValidator::validateStabilityConfig(const FableAbleStabilityConfig& config) const {
    if (!config.isValid()) {
        logger(Logging::WARNING) << "Invalid stability configuration";
        return false;
    }
    
    // Additional validation logic can be added here
    return true;
}

bool FableAbleStabilityValidator::isDepositStable(const FableAbleDepositData& deposit) const {
    // Check price stability
    if (!checkPriceStability(deposit)) {
        return false;
    }
    
    // Check collateral ratio
    if (!checkCollateralRatio(deposit)) {
        return false;
    }
    
    // Check liquidation threshold
    if (!checkLiquidationThreshold(deposit)) {
        return false;
    }
    
    return true;
}

bool FableAbleStabilityValidator::isDepositLiquidatable(const FableAbleDepositData& deposit) const {
    // Check if deposit is mature
    if (!deposit.isMature()) {
        return false;
    }
    
    // Check if deposit is active
    if (deposit.status != DepositStatus::ACTIVE) {
        return false;
    }
    
    // Check collateral ratio against liquidation threshold
    double collateralRatio = deposit.calculateCollateralRatio();
    double liquidationThreshold = deposit.liquidationThreshold / 100.0;
    
    if (collateralRatio < liquidationThreshold) {
        logger(Logging::INFO) << "Deposit liquidatable due to low collateral ratio: " 
                              << collateralRatio << " < " << liquidationThreshold;
        return true;
    }
    
    // Check price deviation
    if (deposit.priceDeviation > m_config.priceDeviationThreshold) {
        logger(Logging::INFO) << "Deposit liquidatable due to high price deviation: " 
                              << deposit.priceDeviation << " > " << m_config.priceDeviationThreshold;
        return true;
    }
    
    return false;
}

bool FableAbleStabilityValidator::isPoolHealthy(const StabilityPoolData& pool) const {
    double poolHealth = pool.calculatePoolHealth();
    
    // Pool is healthy if health score is above 70%
    if (poolHealth < 70.0) {
        logger(Logging::WARNING) << "Pool unhealthy: " << poolHealth << "%";
        return false;
    }
    
    // Check if pool has sufficient deposits
    if (pool.totalDeposits < m_config.minDepositAmount) {
        logger(Logging::WARNING) << "Pool has insufficient deposits: " << pool.totalDeposits;
        return false;
    }
    
    // Check if pool has sufficient collateral
    if (pool.totalCollateral < m_config.minCollateralAmount) {
        logger(Logging::WARNING) << "Pool has insufficient collateral: " << pool.totalCollateral;
        return false;
    }
    
    return true;
}

bool FableAbleStabilityValidator::checkPriceStability(const FableAbleDepositData& deposit) const {
    if (deposit.targetPrice <= 0) {
        return false;
    }
    
    double priceDeviation = calculatePriceDeviation(deposit.currentPrice, deposit.targetPrice);
    
    if (priceDeviation > m_config.priceDeviationThreshold) {
        logger(Logging::WARNING) << "Price deviation too high: " << priceDeviation 
                                 << " > " << m_config.priceDeviationThreshold;
        return false;
    }
    
    return true;
}

bool FableAbleStabilityValidator::checkCollateralRatio(const FableAbleDepositData& deposit) const {
    double collateralRatio = deposit.calculateCollateralRatio();
    double minRatio = m_config.minCollateralRatio / 100.0;
    double maxRatio = m_config.maxCollateralRatio / 100.0;
    
    if (!isWithinThreshold(collateralRatio, minRatio) || collateralRatio > maxRatio) {
        logger(Logging::WARNING) << "Collateral ratio out of range: " << collateralRatio 
                                 << " (min: " << minRatio << ", max: " << maxRatio << ")";
        return false;
    }
    
    return true;
}

bool FableAbleStabilityValidator::checkLiquidationThreshold(const FableAbleDepositData& deposit) const {
    double collateralRatio = deposit.calculateCollateralRatio();
    double liquidationThreshold = deposit.liquidationThreshold / 100.0;
    
    if (collateralRatio < liquidationThreshold) {
        logger(Logging::WARNING) << "Collateral ratio below liquidation threshold: " 
                                 << collateralRatio << " < " << liquidationThreshold;
        return false;
    }
    
    return true;
}

// Private helper methods
double FableAbleStabilityValidator::calculatePriceDeviation(double currentPrice, double targetPrice) const {
    if (targetPrice <= 0) {
        return 100.0; // 100% deviation if target price is invalid
    }
    
    return std::abs(currentPrice - targetPrice) / targetPrice * 100.0;
}

double FableAbleStabilityValidator::calculateCollateralRatio(uint64_t collateralAmount, uint64_t depositAmount) const {
    if (depositAmount == 0) {
        return 0.0;
    }
    
    return (static_cast<double>(collateralAmount) / static_cast<double>(depositAmount)) * 100.0;
}

bool FableAbleStabilityValidator::isWithinThreshold(double value, double threshold) const {
    return value >= threshold;
}

} // namespace CryptoNote