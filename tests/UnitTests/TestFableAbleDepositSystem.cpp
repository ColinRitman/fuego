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
#include "FableAbleDepositSystem.h"
#include "Common/Logging.h"
#include "crypto/hash.h"
#include "crypto/randomize.h"
#include <chrono>

using namespace CryptoNote;

class FableAbleDepositSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger = Logging::createLogger("TestFableAbleDepositSystem");
        depositManager = std::make_unique<FableAbleDepositManager>(*logger);
        stabilityValidator = std::make_unique<FableAbleStabilityValidator>(*logger);
    }

    void TearDown() override {
        depositManager.reset();
        stabilityValidator.reset();
    }

    std::shared_ptr<Logging::ILogger> logger;
    std::unique_ptr<FableAbleDepositManager> depositManager;
    std::unique_ptr<FableAbleStabilityValidator> stabilityValidator;

    FableAbleDepositData createTestDeposit() {
        FableAbleDepositData deposit;
        deposit.depositId = Crypto::Hash::random();
        deposit.depositType = FableAbleDepositType::FABLE_STABLE;
        deposit.stabilityMechanism = StabilityMechanism::PRICE_PEG;
        deposit.depositAmount = 1000000000; // 1 XFG
        deposit.collateralAmount = 1500000000; // 1.5 XFG
        deposit.stabilityTarget = 1000000000; // 1 XFG target
        deposit.minCollateralRatio = 15000; // 150% (15000 basis points)
        deposit.maxCollateralRatio = 50000; // 500% (50000 basis points)
        deposit.liquidationThreshold = 12000; // 120% (12000 basis points)
        deposit.depositTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        deposit.maturityTimestamp = deposit.depositTimestamp + 86400; // 24 hours
        deposit.depositorAddress = "test_address_123";
        deposit.collateralAsset = "XFG";
        deposit.stabilityTargetAsset = "FABLE";
        deposit.status = DepositStatus::ACTIVE;
        deposit.currentPrice = 1.0;
        deposit.targetPrice = 1.0;
        deposit.priceDeviation = 0.0;
        deposit.collateralRatio = 150.0;
        deposit.stabilityScore = 100.0;
        deposit.metadata = {0x01, 0x02, 0x03};
        deposit.signature = {0x04, 0x05, 0x06};
        
        return deposit;
    }

    StabilityPoolData createTestStabilityPool() {
        StabilityPoolData pool;
        pool.poolId = Crypto::Hash::random();
        pool.poolName = "Test Stability Pool";
        pool.poolType = FableAbleDepositType::STABILITY_POOL;
        pool.totalDeposits = 10000000000; // 10 XFG
        pool.totalCollateral = 15000000000; // 15 XFG
        pool.totalLiquidations = 0;
        pool.currentStabilityScore = 95.0;
        pool.targetStabilityScore = 100.0;
        pool.lastUpdateTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        pool.activeDeposits = {Crypto::Hash::random()};
        pool.liquidatedDeposits = {};
        
        return pool;
    }
};

TEST_F(FableAbleDepositSystemTest, DepositDataValidation) {
    auto deposit = createTestDeposit();
    
    EXPECT_TRUE(deposit.isValid());
    EXPECT_FALSE(deposit.isMature());
    EXPECT_FALSE(deposit.isLiquidatable());
    EXPECT_TRUE(deposit.isStable());
    
    // Test invalid deposit
    deposit.depositAmount = 0;
    EXPECT_FALSE(deposit.isValid());
}

TEST_F(FableAbleDepositSystemTest, DepositMaturity) {
    auto deposit = createTestDeposit();
    
    // Set maturity timestamp to past
    deposit.maturityTimestamp = deposit.depositTimestamp - 1;
    EXPECT_TRUE(deposit.isMature());
    
    // Set maturity timestamp to future
    deposit.maturityTimestamp = deposit.depositTimestamp + 86400;
    EXPECT_FALSE(deposit.isMature());
}

TEST_F(FableAbleDepositSystemTest, DepositLiquidation) {
    auto deposit = createTestDeposit();
    
    // Set low collateral ratio to trigger liquidation
    deposit.collateralAmount = 100000000; // 0.1 XFG (10% ratio)
    deposit.collateralRatio = 10.0;
    deposit.maturityTimestamp = deposit.depositTimestamp - 1; // Make it mature
    
    EXPECT_TRUE(deposit.isLiquidatable());
}

TEST_F(FableAbleDepositSystemTest, StabilityScoreCalculation) {
    auto deposit = createTestDeposit();
    
    // Test normal stability score
    deposit.priceDeviation = 2.0; // 2% deviation
    deposit.collateralRatio = 200.0; // 200% ratio
    double score = deposit.calculateStabilityScore();
    
    EXPECT_GT(score, 0.0);
    EXPECT_LE(score, 100.0);
}

TEST_F(FableAbleDepositSystemTest, CollateralRatioCalculation) {
    auto deposit = createTestDeposit();
    
    double ratio = deposit.calculateCollateralRatio();
    EXPECT_EQ(ratio, 150.0); // 1.5 XFG / 1.0 XFG * 100 = 150%
}

TEST_F(FableAbleDepositSystemTest, StabilityPoolValidation) {
    auto pool = createTestStabilityPool();
    
    EXPECT_TRUE(pool.isValid());
    
    double health = pool.calculatePoolHealth();
    EXPECT_GT(health, 0.0);
    EXPECT_LE(health, 100.0);
}

TEST_F(FableAbleDepositSystemTest, DepositManagerCreateDeposit) {
    auto deposit = createTestDeposit();
    
    bool result = depositManager->createDeposit(deposit);
    EXPECT_TRUE(result);
    
    // Verify deposit was created
    auto retrievedDeposit = depositManager->getDeposit(deposit.depositId);
    EXPECT_TRUE(retrievedDeposit.has_value());
    EXPECT_EQ(retrievedDeposit->depositId, deposit.depositId);
}

TEST_F(FableAbleDepositSystemTest, DepositManagerUpdateDeposit) {
    auto deposit = createTestDeposit();
    depositManager->createDeposit(deposit);
    
    // Update deposit
    deposit.collateralAmount = 2000000000; // 2 XFG
    deposit.collateralRatio = 200.0;
    
    bool result = depositManager->updateDeposit(deposit.depositId, deposit);
    EXPECT_TRUE(result);
    
    // Verify update
    auto retrievedDeposit = depositManager->getDeposit(deposit.depositId);
    EXPECT_TRUE(retrievedDeposit.has_value());
    EXPECT_EQ(retrievedDeposit->collateralAmount, 2000000000);
}

TEST_F(FableAbleDepositSystemTest, DepositManagerLiquidateDeposit) {
    auto deposit = createTestDeposit();
    deposit.depositAmount = 1000000000; // 1 XFG
    deposit.collateralAmount = 50000000; // 0.05 XFG (5% ratio)
    deposit.collateralRatio = 5.0;
    deposit.maturityTimestamp = deposit.depositTimestamp - 1; // Make it mature
    depositManager->createDeposit(deposit);
    
    bool result = depositManager->liquidateDeposit(deposit.depositId, "Low collateral ratio");
    EXPECT_TRUE(result);
    
    // Verify liquidation
    auto retrievedDeposit = depositManager->getDeposit(deposit.depositId);
    EXPECT_TRUE(retrievedDeposit.has_value());
    EXPECT_EQ(retrievedDeposit->status, DepositStatus::LIQUIDATED);
}

TEST_F(FableAbleDepositSystemTest, DepositManagerWithdrawDeposit) {
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

TEST_F(FableAbleDepositSystemTest, DepositManagerGetDepositsByAddress) {
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

TEST_F(FableAbleDepositSystemTest, DepositManagerGetDepositsByType) {
    auto deposit1 = createTestDeposit();
    deposit1.depositType = FableAbleDepositType::FABLE_STABLE;
    depositManager->createDeposit(deposit1);
    
    auto deposit2 = createTestDeposit();
    deposit2.depositType = FableAbleDepositType::ABLE_COLLATERAL;
    depositManager->createDeposit(deposit2);
    
    auto fableDeposits = depositManager->getDepositsByType(FableAbleDepositType::FABLE_STABLE);
    EXPECT_EQ(fableDeposits.size(), 1);
    
    auto ableDeposits = depositManager->getDepositsByType(FableAbleDepositType::ABLE_COLLATERAL);
    EXPECT_EQ(ableDeposits.size(), 1);
}

TEST_F(FableAbleDepositSystemTest, DepositManagerStabilityMetrics) {
    auto deposit1 = createTestDeposit();
    depositManager->createDeposit(deposit1);
    
    auto deposit2 = createTestDeposit();
    deposit2.depositId = Crypto::Hash::random();
    depositManager->createDeposit(deposit2);
    
    bool result = depositManager->updateStabilityMetrics();
    EXPECT_TRUE(result);
    
    auto metrics = depositManager->getStabilityMetrics();
    EXPECT_TRUE(metrics.isValid());
    EXPECT_EQ(metrics.activeDeposits, 2);
}

TEST_F(FableAbleDepositSystemTest, DepositManagerStabilityPools) {
    auto pool = createTestStabilityPool();
    
    bool result = depositManager->createStabilityPool(pool);
    EXPECT_TRUE(result);
    
    auto retrievedPool = depositManager->getStabilityPool(pool.poolId);
    EXPECT_TRUE(retrievedPool.has_value());
    EXPECT_EQ(retrievedPool->poolId, pool.poolId);
}

TEST_F(FableAbleDepositSystemTest, StabilityValidatorDepositValidation) {
    auto deposit = createTestDeposit();
    
    bool result = stabilityValidator->validateDepositData(deposit);
    EXPECT_TRUE(result);
    
    // Test invalid deposit
    deposit.depositAmount = 0;
    result = stabilityValidator->validateDepositData(deposit);
    EXPECT_FALSE(result);
}

TEST_F(FableAbleDepositSystemTest, StabilityValidatorStabilityChecks) {
    auto deposit = createTestDeposit();
    
    bool isStable = stabilityValidator->isDepositStable(deposit);
    EXPECT_TRUE(isStable);
    
    bool isLiquidatable = stabilityValidator->isDepositLiquidatable(deposit);
    EXPECT_FALSE(isLiquidatable);
}

TEST_F(FableAbleDepositSystemTest, StabilityValidatorPriceStability) {
    auto deposit = createTestDeposit();
    deposit.currentPrice = 1.05; // 5% above target
    deposit.targetPrice = 1.0;
    
    bool isStable = stabilityValidator->checkPriceStability(deposit);
    EXPECT_TRUE(isStable); // Within 5% threshold
    
    deposit.currentPrice = 1.1; // 10% above target
    isStable = stabilityValidator->checkPriceStability(deposit);
    EXPECT_FALSE(isStable); // Above 5% threshold
}

TEST_F(FableAbleDepositSystemTest, StabilityValidatorCollateralRatio) {
    auto deposit = createTestDeposit();
    deposit.collateralAmount = 2000000000; // 2 XFG (200% ratio)
    
    bool isValid = stabilityValidator->checkCollateralRatio(deposit);
    EXPECT_TRUE(isValid);
    
    deposit.collateralAmount = 50000000; // 0.05 XFG (5% ratio)
    isValid = stabilityValidator->checkCollateralRatio(deposit);
    EXPECT_FALSE(isValid);
}

TEST_F(FableAbleDepositSystemTest, StabilityValidatorLiquidationThreshold) {
    auto deposit = createTestDeposit();
    deposit.collateralAmount = 1500000000; // 1.5 XFG (150% ratio)
    
    bool isValid = stabilityValidator->checkLiquidationThreshold(deposit);
    EXPECT_TRUE(isValid);
    
    deposit.collateralAmount = 100000000; // 0.1 XFG (10% ratio)
    isValid = stabilityValidator->checkLiquidationThreshold(deposit);
    EXPECT_FALSE(isValid);
}

TEST_F(FableAbleDepositSystemTest, StabilityConfigValidation) {
    auto config = FableAbleStabilityConfig::getDefault();
    
    EXPECT_TRUE(config.isValid());
    EXPECT_GT(config.minDepositAmount, 0);
    EXPECT_GT(config.maxDepositAmount, config.minDepositAmount);
    EXPECT_GT(config.minCollateralRatio, 0);
    EXPECT_GT(config.maxCollateralRatio, config.minCollateralRatio);
    EXPECT_LT(config.liquidationThreshold, config.minCollateralRatio);
}

TEST_F(FableAbleDepositSystemTest, LiquidationEventValidation) {
    LiquidationEvent event;
    event.eventId = Crypto::Hash::random();
    event.depositId = Crypto::Hash::random();
    event.liquidatedAmount = 1000000000; // 1 XFG
    event.collateralRecovered = 500000000; // 0.5 XFG
    event.liquidatorAddress = "liquidator_address";
    event.reason = "Low collateral ratio";
    event.evidence = {0x01, 0x02, 0x03};
    
    EXPECT_TRUE(event.isValid());
    
    // Test invalid event
    event.liquidatedAmount = 0;
    EXPECT_FALSE(event.isValid());
}

TEST_F(FableAbleDepositSystemTest, DepositToString) {
    auto deposit = createTestDeposit();
    std::string str = deposit.toString();
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("FableAbleDepositData"), std::string::npos);
    EXPECT_NE(str.find(Common::podToHex(deposit.depositId)), std::string::npos);
}

TEST_F(FableAbleDepositSystemTest, StabilityPoolToString) {
    auto pool = createTestStabilityPool();
    std::string str = pool.toString();
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("StabilityPoolData"), std::string::npos);
    EXPECT_NE(str.find(pool.poolName), std::string::npos);
}

TEST_F(FableAbleDepositSystemTest, LiquidationEventToString) {
    LiquidationEvent event;
    event.eventId = Crypto::Hash::random();
    event.depositId = Crypto::Hash::random();
    event.liquidatedAmount = 1000000000;
    event.collateralRecovered = 500000000;
    event.liquidatorAddress = "liquidator_address";
    event.reason = "Low collateral ratio";
    event.evidence = {0x01, 0x02, 0x03};
    
    std::string str = event.toString();
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("LiquidationEvent"), std::string::npos);
    EXPECT_NE(str.find(Common::podToHex(event.eventId)), std::string::npos);
}