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

// Example usage of the Fable Deposit System
// This demonstrates how to create, manage, and query Fable deposits

#include "FableDepositSystem.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "Common/Logging.h"
#include "crypto/hash.h"
#include <iostream>
#include <chrono>

using namespace CryptoNote;

int main() {
    // Create logger
    auto logger = Logging::createLogger("FableExample");
    
    // Create deposit manager
    FableDepositManager depositManager(*logger);
    
    // Create deposit validator
    FableDepositValidator depositValidator(*logger);
    
    std::cout << "=== Fable Deposit System Example ===" << std::endl;
    
    // 1. Create a Fable stablecoin deposit
    std::cout << "\n1. Creating Fable stablecoin deposit..." << std::endl;
    
    FableDepositData deposit;
    deposit.depositId = Crypto::Hash::random();
    deposit.depositType = FableDepositType::FABLE_STABLE;
    deposit.xfgAmount = 1000000000; // 1 XFG
    deposit.abelAmount = 1000000000; // 1 ABEL (1:1 exchange rate)
    deposit.depositTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    deposit.maturityTimestamp = deposit.depositTimestamp + 86400; // 24 hours
    deposit.depositorAddress = "depositor_address_123";
    deposit.status = DepositStatus::ACTIVE;
    deposit.metadata = {0x01, 0x02, 0x03};
    deposit.signature = {0x04, 0x05, 0x06};
    
    // Validate deposit
    if (depositValidator.validateDeposit(deposit)) {
        std::cout << "✓ Deposit validation passed" << std::endl;
    } else {
        std::cout << "✗ Deposit validation failed" << std::endl;
        return 1;
    }
    
    // Create deposit
    if (depositManager.createDeposit(deposit)) {
        std::cout << "✓ Created deposit: " << deposit.toString() << std::endl;
    } else {
        std::cout << "✗ Failed to create deposit" << std::endl;
        return 1;
    }
    
    // 2. Create transaction extra with Fable commitment
    std::cout << "\n2. Creating transaction extra with Fable commitment..." << std::endl;
    
    std::vector<uint8_t> tx_extra;
    Crypto::Hash commitment = Crypto::Hash::random();
    
    if (createTxExtraWithFableCommitment(
        commitment,
        1000000000, // 1 XFG
        {0x01, 0x02, 0x03}, // metadata
        tx_extra
    )) {
        std::cout << "✓ Created transaction extra with Fable commitment" << std::endl;
        std::cout << "  Extra size: " << tx_extra.size() << " bytes" << std::endl;
        
        // Parse commitment from transaction extra
        TransactionExtraFableCommitment fableCommitment;
        if (getFableCommitmentFromExtra(tx_extra, fableCommitment)) {
            std::cout << "✓ Parsed commitment: " << fableCommitment.toString() << std::endl;
        } else {
            std::cout << "✗ Failed to parse commitment" << std::endl;
        }
    } else {
        std::cout << "✗ Failed to create transaction extra" << std::endl;
    }
    
    // 3. Query deposits
    std::cout << "\n3. Querying deposits..." << std::endl;
    
    // Get all active deposits
    std::vector<FableDepositData> activeDeposits = depositManager.getActiveDeposits();
    std::cout << "✓ Found " << activeDeposits.size() << " active deposits" << std::endl;
    
    // Get deposits by address
    std::vector<FableDepositData> addressDeposits = depositManager.getDepositsByAddress("depositor_address_123");
    std::cout << "✓ Found " << addressDeposits.size() << " deposits for address" << std::endl;
    
    // Get deposits by type
    std::vector<FableDepositData> fableDeposits = depositManager.getDepositsByType(FableDepositType::FABLE_STABLE);
    std::cout << "✓ Found " << fableDeposits.size() << " Fable stablecoin deposits" << std::endl;
    
    // 4. Check for liquidatable deposits
    std::cout << "\n4. Checking for liquidatable deposits..." << std::endl;
    
    std::vector<FableDepositData> liquidatableDeposits = depositManager.getLiquidatableDeposits();
    if (!liquidatableDeposits.empty()) {
        std::cout << "⚠ Found " << liquidatableDeposits.size() << " liquidatable deposits" << std::endl;
        
        for (const auto& liquidatableDeposit : liquidatableDeposits) {
            std::cout << "  Liquidatable deposit: " << Common::podToHex(liquidatableDeposit.depositId) << std::endl;
        }
    } else {
        std::cout << "✓ No liquidatable deposits found" << std::endl;
    }
    
    // 5. Get deposit index
    std::cout << "\n5. Getting deposit index..." << std::endl;
    
    FableDepositIndexEntry index = depositManager.getDepositIndex(0);
    if (index.isValid()) {
        std::cout << "✓ Deposit index: " << index.toString() << std::endl;
    } else {
        std::cout << "✗ Invalid deposit index" << std::endl;
    }
    
    // 6. Configuration management
    std::cout << "\n6. Configuration management..." << std::endl;
    
    FableDepositConfig config = depositManager.getConfig();
    std::cout << "✓ Current config: " << config.toString() << std::endl;
    
    // Update configuration
    config.minDepositAmount = 500000000; // 0.5 XFG
    config.abelExchangeRate = 2.0; // 2:1 exchange rate
    depositManager.setConfig(config);
    
    FableDepositConfig updatedConfig = depositManager.getConfig();
    std::cout << "✓ Updated config: " << updatedConfig.toString() << std::endl;
    
    // 7. Deposit maturity check
    std::cout << "\n7. Checking deposit maturity..." << std::endl;
    
    auto retrievedDeposit = depositManager.getDeposit(deposit.depositId);
    if (retrievedDeposit.has_value()) {
        if (retrievedDeposit->isMature()) {
            std::cout << "✓ Deposit is mature and can be withdrawn" << std::endl;
        } else {
            std::cout << "⏳ Deposit is not yet mature" << std::endl;
        }
        
        if (depositValidator.isDepositLiquidatable(*retrievedDeposit)) {
            std::cout << "⚠ Deposit is liquidatable" << std::endl;
        } else {
            std::cout << "✓ Deposit is not liquidatable" << std::endl;
        }
    } else {
        std::cout << "✗ Deposit not found" << std::endl;
    }
    
    std::cout << "\n=== Example completed successfully ===" << std::endl;
    
    return 0;
}