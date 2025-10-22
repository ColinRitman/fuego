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
#include "crypto/hash.h"
#include "crypto/randomize.h"
#include <algorithm>
#include <chrono>
#include <sstream>

namespace CryptoNote {

// FableDepositData implementation
bool FableDepositData::isValid() const {
    return depositId != NULL_HASH &&
           xfgAmount > 0 &&
           abelAmount > 0 &&
           !depositorAddress.empty() &&
           maturityTimestamp > depositTimestamp &&
           status == DepositStatus::ACTIVE;
}

bool FableDepositData::isMature() const {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return static_cast<uint64_t>(now) >= maturityTimestamp;
}

bool FableDepositData::isActive() const {
    return status == DepositStatus::ACTIVE;
}

std::string FableDepositData::toString() const {
    std::ostringstream oss;
    oss << "FableDepositData{id=" << Common::podToHex(depositId)
        << ", type=" << static_cast<int>(depositType)
        << ", xfgAmount=" << xfgAmount
        << ", abelAmount=" << abelAmount
        << ", status=" << static_cast<int>(status) << "}";
    return oss.str();
}

// FableDepositIndexEntry implementation
bool FableDepositIndexEntry::serialize(ISerializer& serializer) {
    serializer(totalXfgBurned, "totalXfgBurned");
    serializer(totalAbelMinted, "totalAbelMinted");
    serializer(activeDeposits, "activeDeposits");
    serializer(liquidatedDeposits, "liquidatedDeposits");
    serializer(withdrawnDeposits, "withdrawnDeposits");
    serializer(timestamp, "timestamp");
    return true;
}

bool FableDepositIndexEntry::isValid() const {
    return totalXfgBurned >= 0 &&
           totalAbelMinted >= 0 &&
           activeDeposits >= 0 &&
           liquidatedDeposits >= 0 &&
           withdrawnDeposits >= 0 &&
           timestamp > 0;
}

std::string FableDepositIndexEntry::toString() const {
    std::ostringstream oss;
    oss << "FableDepositIndexEntry{xfgBurned=" << totalXfgBurned
        << ", abelMinted=" << totalAbelMinted
        << ", active=" << activeDeposits
        << ", liquidated=" << liquidatedDeposits
        << ", withdrawn=" << withdrawnDeposits << "}";
    return oss.str();
}

// FableDepositConfig implementation
FableDepositConfig FableDepositConfig::getDefault() {
    FableDepositConfig config;
    config.minDepositAmount = 1000000000;        // 1 XFG
    config.maxDepositAmount = 1000000000000;     // 1000 XFG
    config.minMaturityTime = 86400;              // 24 hours
    config.maxMaturityTime = 31536000;           // 1 year
    config.abelExchangeRate = 1.0;               // 1:1 exchange rate
    config.liquidationThreshold = 12000;         // 120% (12000 basis points)
    config.enableAutomaticLiquidation = true;
    config.enableGovernanceVoting = true;
    return config;
}

bool FableDepositConfig::isValid() const {
    return minDepositAmount > 0 &&
           maxDepositAmount > minDepositAmount &&
           minMaturityTime > 0 &&
           maxMaturityTime > minMaturityTime &&
           abelExchangeRate > 0.0 &&
           liquidationThreshold > 0;
}

std::string FableDepositConfig::toString() const {
    std::ostringstream oss;
    oss << "FableDepositConfig{minAmount=" << minDepositAmount
        << ", maxAmount=" << maxDepositAmount
        << ", minMaturity=" << minMaturityTime
        << ", maxMaturity=" << maxMaturityTime
        << ", exchangeRate=" << abelExchangeRate << "}";
    return oss.str();
}

// FableDepositManager implementation
FableDepositManager::FableDepositManager(Logging::ILogger& logger)
    : logger(logger, "FableDepositManager") {
    m_config = FableDepositConfig::getDefault();
}

bool FableDepositManager::createDeposit(const FableDepositData& deposit) {
    if (!validateDeposit(deposit)) {
        logger(Logging::WARNING) << "Invalid deposit data";
        return false;
    }
    
    if (m_deposits.find(deposit.depositId) != m_deposits.end()) {
        logger(Logging::WARNING) << "Deposit already exists: " << Common::podToHex(deposit.depositId);
        return false;
    }
    
    m_deposits[deposit.depositId] = deposit;
    
    // Add to deposit index
    addDepositToIndex(deposit);
    
    logger(Logging::INFO) << "Created deposit: " << deposit.toString();
    return true;
}

bool FableDepositManager::updateDeposit(const Crypto::Hash& depositId, const FableDepositData& deposit) {
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
    
    logger(Logging::INFO) << "Updated deposit: " << deposit.toString();
    return true;
}

bool FableDepositManager::liquidateDeposit(const Crypto::Hash& depositId, const std::string& reason) {
    auto it = m_deposits.find(depositId);
    if (it == m_deposits.end()) {
        logger(Logging::WARNING) << "Deposit not found for liquidation: " << Common::podToHex(depositId);
        return false;
    }
    
    if (it->second.status != DepositStatus::ACTIVE) {
        logger(Logging::WARNING) << "Deposit not active for liquidation: " << Common::podToHex(depositId);
        return false;
    }
    
    return processLiquidation(depositId, reason);
}

bool FableDepositManager::withdrawDeposit(const Crypto::Hash& depositId) {
    auto it = m_deposits.find(depositId);
    if (it == m_deposits.end()) {
        logger(Logging::WARNING) << "Deposit not found for withdrawal: " << Common::podToHex(depositId);
        return false;
    }
    
    if (it->second.status != DepositStatus::ACTIVE) {
        logger(Logging::WARNING) << "Deposit not active for withdrawal: " << Common::podToHex(depositId);
        return false;
    }
    
    if (!it->second.isMature()) {
        logger(Logging::WARNING) << "Deposit not mature for withdrawal: " << Common::podToHex(depositId);
        return false;
    }
    
    it->second.status = DepositStatus::WITHDRAWN;
    
    // Update deposit index
    removeDepositFromIndex(depositId);
    
    logger(Logging::INFO) << "Withdrew deposit: " << Common::podToHex(depositId);
    return true;
}

std::optional<FableDepositData> FableDepositManager::getDeposit(const Crypto::Hash& depositId) const {
    auto it = m_deposits.find(depositId);
    if (it == m_deposits.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<FableDepositData> FableDepositManager::getDepositsByAddress(const std::string& address) const {
    std::vector<FableDepositData> result;
    
    for (const auto& pair : m_deposits) {
        if (pair.second.depositorAddress == address) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

std::vector<FableDepositData> FableDepositManager::getDepositsByType(FableDepositType type) const {
    std::vector<FableDepositData> result;
    
    for (const auto& pair : m_deposits) {
        if (pair.second.depositType == type) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

std::vector<FableDepositData> FableDepositManager::getActiveDeposits() const {
    std::vector<FableDepositData> result;
    
    for (const auto& pair : m_deposits) {
        if (pair.second.status == DepositStatus::ACTIVE) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

std::vector<FableDepositData> FableDepositManager::getLiquidatableDeposits() const {
    std::vector<FableDepositData> result;
    
    for (const auto& pair : m_deposits) {
        if (pair.second.status == DepositStatus::ACTIVE && 
            pair.second.isMature() && 
            pair.second.xfgAmount < pair.second.abelAmount * m_config.abelExchangeRate * (m_config.liquidationThreshold / 10000.0)) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

bool FableDepositManager::addDepositToIndex(const FableDepositData& deposit) {
    // Get current block height (placeholder - would be passed from blockchain)
    uint32_t blockHeight = 0; // This should be the actual block height
    
    auto it = m_depositIndex.find(blockHeight);
    if (it == m_depositIndex.end()) {
        FableDepositIndexEntry entry;
        entry.totalXfgBurned = 0;
        entry.totalAbelMinted = 0;
        entry.activeDeposits = 0;
        entry.liquidatedDeposits = 0;
        entry.withdrawnDeposits = 0;
        entry.timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        m_depositIndex[blockHeight] = entry;
        it = m_depositIndex.find(blockHeight);
    }
    
    it->second.totalXfgBurned += deposit.xfgAmount;
    it->second.totalAbelMinted += deposit.abelAmount;
    it->second.activeDeposits++;
    
    return true;
}

bool FableDepositManager::removeDepositFromIndex(const Crypto::Hash& depositId) {
    auto depositIt = m_deposits.find(depositId);
    if (depositIt == m_deposits.end()) {
        return false;
    }
    
    const auto& deposit = depositIt->second;
    
    // Get current block height (placeholder - would be passed from blockchain)
    uint32_t blockHeight = 0; // This should be the actual block height
    
    auto it = m_depositIndex.find(blockHeight);
    if (it != m_depositIndex.end()) {
        if (deposit.status == DepositStatus::WITHDRAWN) {
            it->second.withdrawnDeposits++;
        } else if (deposit.status == DepositStatus::LIQUIDATED) {
            it->second.liquidatedDeposits++;
        }
        
        it->second.activeDeposits--;
    }
    
    return true;
}

FableDepositIndexEntry FableDepositManager::getDepositIndex(uint32_t blockHeight) const {
    auto it = m_depositIndex.find(blockHeight);
    if (it == m_depositIndex.end()) {
        return FableDepositIndexEntry{};
    }
    return it->second;
}

std::vector<FableDepositIndexEntry> FableDepositManager::getDepositIndexRange(uint32_t startHeight, uint32_t endHeight) const {
    std::vector<FableDepositIndexEntry> result;
    
    for (uint32_t height = startHeight; height <= endHeight; ++height) {
        auto it = m_depositIndex.find(height);
        if (it != m_depositIndex.end()) {
            result.push_back(it->second);
        }
    }
    
    return result;
}

void FableDepositManager::setConfig(const FableDepositConfig& config) {
    if (config.isValid()) {
        m_config = config;
        logger(Logging::INFO) << "Updated config: " << config.toString();
    } else {
        logger(Logging::WARNING) << "Invalid config provided";
    }
}

FableDepositConfig FableDepositManager::getConfig() const {
    return m_config;
}

// Private helper methods
bool FableDepositManager::validateDeposit(const FableDepositData& deposit) const {
    if (!deposit.isValid()) {
        return false;
    }
    
    if (deposit.xfgAmount < m_config.minDepositAmount || 
        deposit.xfgAmount > m_config.maxDepositAmount) {
        return false;
    }
    
    uint64_t maturityTime = deposit.maturityTimestamp - deposit.depositTimestamp;
    if (maturityTime < m_config.minMaturityTime || 
        maturityTime > m_config.maxMaturityTime) {
        return false;
    }
    
    return true;
}

bool FableDepositManager::processLiquidation(const Crypto::Hash& depositId, const std::string& reason) {
    auto it = m_deposits.find(depositId);
    if (it == m_deposits.end()) {
        return false;
    }
    
    it->second.status = DepositStatus::LIQUIDATED;
    
    // Update deposit index
    removeDepositFromIndex(depositId);
    
    logger(Logging::INFO) << "Liquidated deposit: " << Common::podToHex(depositId) << " - " << reason;
    return true;
}

Crypto::Hash FableDepositManager::generateDepositId(const FableDepositData& deposit) const {
    // Generate a unique deposit ID based on deposit data
    std::string data = Common::podToHex(deposit.depositId) + 
                      std::to_string(deposit.xfgAmount) + 
                      std::to_string(deposit.depositTimestamp) + 
                      deposit.depositorAddress;
    
    return Crypto::cn_fast_hash(data.data(), data.size());
}

uint64_t FableDepositManager::calculateAbelAmount(uint64_t xfgAmount) const {
    return static_cast<uint64_t>(xfgAmount * m_config.abelExchangeRate);
}

} // namespace CryptoNote