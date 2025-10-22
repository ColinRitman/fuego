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

#include "FableDepositSystem.h"
#include "Common/Logging.h"
#include <chrono>

namespace CryptoNote {

FableDepositValidator::FableDepositValidator(Logging::ILogger& logger)
    : logger(logger, "FableDepositValidator") {
    m_config = FableDepositConfig::getDefault();
}

bool FableDepositValidator::validateDeposit(const FableDepositData& deposit) const {
    if (!deposit.isValid()) {
        logger(Logging::WARNING) << "Invalid deposit data structure";
        return false;
    }
    
    // Validate deposit amounts
    if (!checkDepositAmounts(deposit)) {
        return false;
    }
    
    // Validate deposit timestamps
    if (!checkDepositTimestamps(deposit)) {
        return false;
    }
    
    // Validate deposit address
    if (!checkDepositAddress(deposit)) {
        return false;
    }
    
    // Validate deposit signature
    if (!checkDepositSignature(deposit)) {
        return false;
    }
    
    return true;
}

bool FableDepositValidator::validateDepositIndex(const FableDepositIndexEntry& entry) const {
    if (!entry.isValid()) {
        logger(Logging::WARNING) << "Invalid deposit index entry";
        return false;
    }
    
    // Additional validation logic can be added here
    return true;
}

bool FableDepositValidator::validateConfig(const FableDepositConfig& config) const {
    if (!config.isValid()) {
        logger(Logging::WARNING) << "Invalid deposit configuration";
        return false;
    }
    
    // Additional validation logic can be added here
    return true;
}

bool FableDepositValidator::isDepositValid(const FableDepositData& deposit) const {
    return validateDeposit(deposit);
}

bool FableDepositValidator::isDepositMature(const FableDepositData& deposit) const {
    return deposit.isMature();
}

bool FableDepositValidator::isDepositLiquidatable(const FableDepositData& deposit) const {
    if (!deposit.isActive()) {
        return false;
    }
    
    if (!deposit.isMature()) {
        return false;
    }
    
    // Check if deposit is undercollateralized
    double collateralRatio = static_cast<double>(deposit.xfgAmount) / 
                           (deposit.abelAmount * m_config.abelExchangeRate);
    double liquidationThreshold = m_config.liquidationThreshold / 10000.0;
    
    if (collateralRatio < liquidationThreshold) {
        logger(Logging::INFO) << "Deposit liquidatable due to low collateral ratio: " 
                              << collateralRatio << " < " << liquidationThreshold;
        return true;
    }
    
    return false;
}

bool FableDepositValidator::isDepositActive(const FableDepositData& deposit) const {
    return deposit.isActive();
}

// Private helper methods
bool FableDepositValidator::checkDepositAmounts(const FableDepositData& deposit) const {
    if (deposit.xfgAmount < m_config.minDepositAmount || 
        deposit.xfgAmount > m_config.maxDepositAmount) {
        logger(Logging::WARNING) << "Deposit amount out of range: " << deposit.xfgAmount;
        return false;
    }
    
    if (deposit.abelAmount <= 0) {
        logger(Logging::WARNING) << "Invalid ABEL amount: " << deposit.abelAmount;
        return false;
    }
    
    // Check if ABEL amount matches expected exchange rate
    uint64_t expectedAbelAmount = static_cast<uint64_t>(deposit.xfgAmount * m_config.abelExchangeRate);
    if (deposit.abelAmount != expectedAbelAmount) {
        logger(Logging::WARNING) << "ABEL amount mismatch: expected " << expectedAbelAmount 
                                 << ", got " << deposit.abelAmount;
        return false;
    }
    
    return true;
}

bool FableDepositValidator::checkDepositTimestamps(const FableDepositData& deposit) const {
    if (deposit.maturityTimestamp <= deposit.depositTimestamp) {
        logger(Logging::WARNING) << "Invalid maturity timestamp";
        return false;
    }
    
    uint64_t maturityTime = deposit.maturityTimestamp - deposit.depositTimestamp;
    if (maturityTime < m_config.minMaturityTime || 
        maturityTime > m_config.maxMaturityTime) {
        logger(Logging::WARNING) << "Maturity time out of range: " << maturityTime;
        return false;
    }
    
    // Check if deposit timestamp is not in the future
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (deposit.depositTimestamp > static_cast<uint64_t>(now)) {
        logger(Logging::WARNING) << "Deposit timestamp in the future";
        return false;
    }
    
    return true;
}

bool FableDepositValidator::checkDepositAddress(const FableDepositData& deposit) const {
    if (deposit.depositorAddress.empty()) {
        logger(Logging::WARNING) << "Empty depositor address";
        return false;
    }
    
    if (deposit.depositorAddress.length() < 10) {
        logger(Logging::WARNING) << "Depositor address too short";
        return false;
    }
    
    if (deposit.depositorAddress.length() > 100) {
        logger(Logging::WARNING) << "Depositor address too long";
        return false;
    }
    
    return true;
}

bool FableDepositValidator::checkDepositSignature(const FableDepositData& deposit) const {
    if (deposit.signature.empty()) {
        logger(Logging::WARNING) << "Empty deposit signature";
        return false;
    }
    
    if (deposit.signature.size() < 32) {
        logger(Logging::WARNING) << "Deposit signature too short";
        return false;
    }
    
    if (deposit.signature.size() > 128) {
        logger(Logging::WARNING) << "Deposit signature too long";
        return false;
    }
    
    // Additional signature validation logic can be added here
    return true;
}

} // namespace CryptoNote