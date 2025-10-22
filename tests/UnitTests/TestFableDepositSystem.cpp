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

#include <gtest/gtest.h>
#include "FableDepositSystem.h"
#include "Common/Logging.h"
#include "crypto/hash.h"
#include "crypto/randomize.h"
#include <chrono>

using namespace CryptoNote;

class FableDepositSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger = Logging::createLogger("TestFableDepositSystem");
        depositManager = std::make_unique<FableDepositManager>(*logger);
        depositValidator = std::make_unique<FableDepositValidator>(*logger);
    }

    void TearDown() override {
        depositManager.reset();
        depositValidator.reset();
    }

    std::shared_ptr<Logging::ILogger> logger;
    std::unique_ptr<FableDepositManager> depositManager;
    std::unique_ptr<FableDepositValidator> depositValidator;

    FableDepositData createTestDeposit() {
        FableDepositData deposit;
        deposit.depositId = Crypto::Hash::random();
        deposit.depositType = FableDepositType::FABLE_STABLE;
        deposit.xfgAmount = 1000000000; // 1 XFG
        deposit.abelAmount = 1000000000; // 1 ABEL (1:1 exchange rate)
        deposit.depositTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        deposit.maturityTimestamp = deposit.depositTimestamp + 86400; // 24 hours
        deposit.depositorAddress = "test_address_123";
        deposit.status = DepositStatus::ACTIVE;
        deposit.metadata = {0x01, 0x02, 0x03};
        deposit.signature = {0x04, 0x05, 0x06};
        
        return deposit;
    }
};

TEST_F(FableDepositSystemTest, DepositDataValidation) {
    auto deposit = createTestDeposit();
    
    EXPECT_TRUE(deposit.isValid());
    EXPECT_FALSE(deposit.isMature());
    EXPECT_TRUE(deposit.isActive());
    
    // Test invalid deposit
    deposit.xfgAmount = 0;
    EXPECT_FALSE(deposit.isValid());
}

TEST_F(FableDepositSystemTest, DepositMaturity) {
    auto deposit = createTestDeposit();
    
    // Set maturity timestamp to past
    deposit.maturityTimestamp = deposit.depositTimestamp - 1;
    EXPECT_TRUE(deposit.isMature());
    
    // Set maturity timestamp to future
    deposit.maturityTimestamp = deposit.depositTimestamp + 86400;
    EXPECT_FALSE(deposit.isMature());
}

TEST_F(FableDepositSystemTest, DepositManagerCreateDeposit) {
    auto deposit = createTestDeposit();
    
    bool result = depositManager->createDeposit(deposit);
    EXPECT_TRUE(result);
    
    // Verify deposit was created
    auto retrievedDeposit = depositManager->getDeposit(deposit.depositId);
    EXPECT_TRUE(retrievedDeposit.has_value());
    EXPECT_EQ(retrievedDeposit->depositId, deposit.depositId);
}

TEST_F(FableDepositSystemTest, DepositManagerUpdateDeposit) {
    auto deposit = createTestDeposit();
    depositManager->createDeposit(deposit);
    
    // Update deposit
    deposit.abelAmount = 2000000000; // 2 ABEL
    
    bool result = depositManager->updateDeposit(deposit.depositId, deposit);
    EXPECT_TRUE(result);
    
    // Verify update
    auto retrievedDeposit = depositManager->getDeposit(deposit.depositId);
    EXPECT_TRUE(retrievedDeposit.has_value());
    EXPECT_EQ(retrievedDeposit->abelAmount, 2000000000);
}

TEST_F(FableDepositSystemTest, DepositManagerLiquidateDeposit) {
    auto deposit = createTestDeposit();
    deposit.xfgAmount = 1000000000; // 1 XFG
    deposit.abelAmount = 2000000000; // 2 ABEL (undercollateralized)
    deposit.maturityTimestamp = deposit.depositTimestamp - 1; // Make it mature
    depositManager->createDeposit(deposit);
    
    bool result = depositManager->liquidateDeposit(deposit.depositId, "Low collateral ratio");
    EXPECT_TRUE(result);
    
    // Verify liquidation
    auto retrievedDeposit = depositManager->getDeposit(deposit.depositId);
    EXPECT_TRUE(retrievedDeposit.has_value());
    EXPECT_EQ(retrievedDeposit->status, DepositStatus::LIQUIDATED);
}

TEST_F(FableDepositSystemTest, DepositManagerWithdrawDeposit) {
    auto deposit = createTestDeposit();
    deposit.maturityTimestamp = deposit.depositTimestamp - 1; // Make it mature
    depositManager->createDeposit(deposit);
    
    bool result = depositManager->withdrawDeposit(deposit.depositId);
    EXPECT_TRUE(result);
    
    // Verify withdrawal
    auto retrievedDeposit = depositManager->getDeposit(deposit.depositId);
    EXPECT_TRUE(retrievedDeposit.has_value());
    EXPECT_EQ(retrievedDeposit->status, DepositStatus::WITHDRAWN);
}

TEST_F(FableDepositSystemTest, DepositManagerGetDepositsByAddress) {
    auto deposit1 = createTestDeposit();
    deposit1.depositorAddress = "test_address_1";
    depositManager->createDeposit(deposit1);
    
    auto deposit2 = createTestDeposit();
    deposit2.depositorAddress = "test_address_2";
    depositManager->createDeposit(deposit2);
    
    auto deposits = depositManager->getDepositsByAddress("test_address_1");
    EXPECT_EQ(deposits.size(), 1);
    EXPECT_EQ(deposits[0].depositorAddress, "test_address_1");
}

TEST_F(FableDepositSystemTest, DepositManagerGetDepositsByType) {
    auto deposit1 = createTestDeposit();
    deposit1.depositType = FableDepositType::FABLE_STABLE;
    depositManager->createDeposit(deposit1);
    
    auto deposit2 = createTestDeposit();
    deposit2.depositType = FableDepositType::ABLE_COLLATERAL;
    depositManager->createDeposit(deposit2);
    
    auto fableDeposits = depositManager->getDepositsByType(FableDepositType::FABLE_STABLE);
    EXPECT_EQ(fableDeposits.size(), 1);
    
    auto ableDeposits = depositManager->getDepositsByType(FableDepositType::ABLE_COLLATERAL);
    EXPECT_EQ(ableDeposits.size(), 1);
}

TEST_F(FableDepositSystemTest, DepositManagerGetActiveDeposits) {
    auto deposit1 = createTestDeposit();
    depositManager->createDeposit(deposit1);
    
    auto deposit2 = createTestDeposit();
    deposit2.depositId = Crypto::Hash::random();
    depositManager->createDeposit(deposit2);
    
    auto activeDeposits = depositManager->getActiveDeposits();
    EXPECT_EQ(activeDeposits.size(), 2);
}

TEST_F(FableDepositSystemTest, DepositManagerGetLiquidatableDeposits) {
    auto deposit = createTestDeposit();
    deposit.xfgAmount = 1000000000; // 1 XFG
    deposit.abelAmount = 2000000000; // 2 ABEL (undercollateralized)
    deposit.maturityTimestamp = deposit.depositTimestamp - 1; // Make it mature
    depositManager->createDeposit(deposit);
    
    auto liquidatableDeposits = depositManager->getLiquidatableDeposits();
    EXPECT_EQ(liquidatableDeposits.size(), 1);
    EXPECT_EQ(liquidatableDeposits[0].depositId, deposit.depositId);
}

TEST_F(FableDepositSystemTest, DepositManagerDepositIndex) {
    auto deposit = createTestDeposit();
    depositManager->createDeposit(deposit);
    
    auto index = depositManager->getDepositIndex(0);
    EXPECT_TRUE(index.isValid());
    EXPECT_EQ(index.totalXfgBurned, deposit.xfgAmount);
    EXPECT_EQ(index.totalAbelMinted, deposit.abelAmount);
    EXPECT_EQ(index.activeDeposits, 1);
}

TEST_F(FableDepositSystemTest, DepositValidatorDepositValidation) {
    auto deposit = createTestDeposit();
    
    bool result = depositValidator->validateDeposit(deposit);
    EXPECT_TRUE(result);
    
    // Test invalid deposit
    deposit.xfgAmount = 0;
    result = depositValidator->validateDeposit(deposit);
    EXPECT_FALSE(result);
}

TEST_F(FableDepositSystemTest, DepositValidatorStabilityChecks) {
    auto deposit = createTestDeposit();
    
    bool isValid = depositValidator->isDepositValid(deposit);
    EXPECT_TRUE(isValid);
    
    bool isMature = depositValidator->isDepositMature(deposit);
    EXPECT_FALSE(isMature);
    
    bool isActive = depositValidator->isDepositActive(deposit);
    EXPECT_TRUE(isActive);
}

TEST_F(FableDepositSystemTest, DepositValidatorLiquidationCheck) {
    auto deposit = createTestDeposit();
    deposit.xfgAmount = 1000000000; // 1 XFG
    deposit.abelAmount = 2000000000; // 2 ABEL (undercollateralized)
    deposit.maturityTimestamp = deposit.depositTimestamp - 1; // Make it mature
    
    bool isLiquidatable = depositValidator->isDepositLiquidatable(deposit);
    EXPECT_TRUE(isLiquidatable);
}

TEST_F(FableDepositSystemTest, DepositConfigValidation) {
    auto config = FableDepositConfig::getDefault();
    
    EXPECT_TRUE(config.isValid());
    EXPECT_GT(config.minDepositAmount, 0);
    EXPECT_GT(config.maxDepositAmount, config.minDepositAmount);
    EXPECT_GT(config.minMaturityTime, 0);
    EXPECT_GT(config.maxMaturityTime, config.minMaturityTime);
    EXPECT_GT(config.abelExchangeRate, 0.0);
}

TEST_F(FableDepositSystemTest, DepositConfigSetting) {
    auto config = FableDepositConfig::getDefault();
    config.minDepositAmount = 500000000; // 0.5 XFG
    config.abelExchangeRate = 2.0; // 2:1 exchange rate
    
    depositManager->setConfig(config);
    
    auto retrievedConfig = depositManager->getConfig();
    EXPECT_EQ(retrievedConfig.minDepositAmount, 500000000);
    EXPECT_EQ(retrievedConfig.abelExchangeRate, 2.0);
}

TEST_F(FableDepositSystemTest, DepositToString) {
    auto deposit = createTestDeposit();
    std::string str = deposit.toString();
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("FableDepositData"), std::string::npos);
    EXPECT_NE(str.find(Common::podToHex(deposit.depositId)), std::string::npos);
}

TEST_F(FableDepositSystemTest, DepositIndexToString) {
    FableDepositIndexEntry entry;
    entry.totalXfgBurned = 1000000000;
    entry.totalAbelMinted = 1000000000;
    entry.activeDeposits = 1;
    entry.liquidatedDeposits = 0;
    entry.withdrawnDeposits = 0;
    entry.timestamp = 1234567890;
    
    std::string str = entry.toString();
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("FableDepositIndexEntry"), std::string::npos);
    EXPECT_NE(str.find("1000000000"), std::string::npos);
}

TEST_F(FableDepositSystemTest, DepositConfigToString) {
    auto config = FableDepositConfig::getDefault();
    std::string str = config.toString();
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("FableDepositConfig"), std::string::npos);
    EXPECT_NE(str.find("1000000000"), std::string::npos);
}