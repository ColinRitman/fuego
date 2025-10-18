// Copyright (c) 2017-2025 Elderfire Privacy Council
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Copyright (c) 2014-2017 The XDN developers
// Copyright (c) 2012-2018 The CryptoNote developers
//
// This file is part of Fuego.
//
// Fuego is free & open source software distributed in the hope
// that it will be useful, but WITHOUT ANY WARRANTY; without even
// the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. You may redistribute it and/or modify it under the terms
// of the GNU General Public License v3 or later versions as published
// by the Free Software Foundation. Fuego includes elements written
// by third parties. See file labeled LICENSE for more details.
// You should have received a copy of the GNU General Public License
// along with Fuego. If not, see <https://www.gnu.org/licenses/>.

#include <gtest/gtest.h>
#include "CryptoNoteCore/StagedDepositUnlock.h"
#include "CryptoNoteCore/EnhancedDeposit.h"
#include "CryptoNoteCore/Currency.h"
#include "Logging/ConsoleLogger.h"

using namespace CryptoNote;

class StagedDepositUnlockTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_logger = std::make_unique<Logging::ConsoleLogger>();
        m_currency = CurrencyBuilder(*m_logger)
            .upgradeHeightV2(0)
            .depositMinTerm(10)
            .depositMinTotalRateFactor(100)
            .currency();
    }
    
    std::unique_ptr<Logging::ConsoleLogger> m_logger;
    Currency m_currency;
};

TEST_F(StagedDepositUnlockTest, BasicStagedUnlock) {
    // Test basic staged unlock functionality
    uint64_t amount = 1000000000; // 1000 XFG
    uint64_t interest = 100000000; // 100 XFG
    uint32_t depositHeight = 1000;
    
    StagedDepositUnlock stagedUnlock(amount, interest, depositHeight);
    
    // Check initial state
    EXPECT_EQ(stagedUnlock.getTotalUnlockedAmount(), 0);
    EXPECT_EQ(stagedUnlock.getRemainingLockedAmount(), amount + interest);
    EXPECT_FALSE(stagedUnlock.isFullyUnlocked());
    
    // Check stages
    auto stages = stagedUnlock.getStages();
    EXPECT_EQ(stages.size(), 4);
    
    // Stage 1: 25% principal + 100% interest
    EXPECT_EQ(stages[0].stageNumber, 1);
    EXPECT_EQ(stages[0].unlockHeight, depositHeight + StagedUnlockConfig::STAGE_INTERVAL_BLOCKS);
    EXPECT_EQ(stages[0].principalAmount, (amount * 25) / 100);
    EXPECT_EQ(stages[0].interestAmount, interest);
    EXPECT_FALSE(stages[0].isUnlocked);
    
    // Stage 2: 25% principal
    EXPECT_EQ(stages[1].stageNumber, 2);
    EXPECT_EQ(stages[1].unlockHeight, depositHeight + (2 * StagedUnlockConfig::STAGE_INTERVAL_BLOCKS));
    EXPECT_EQ(stages[1].principalAmount, (amount * 25) / 100);
    EXPECT_EQ(stages[1].interestAmount, 0);
    EXPECT_FALSE(stages[1].isUnlocked);
    
    // Stage 3: 25% principal
    EXPECT_EQ(stages[2].stageNumber, 3);
    EXPECT_EQ(stages[2].unlockHeight, depositHeight + (3 * StagedUnlockConfig::STAGE_INTERVAL_BLOCKS));
    EXPECT_EQ(stages[2].principalAmount, (amount * 25) / 100);
    EXPECT_EQ(stages[2].interestAmount, 0);
    EXPECT_FALSE(stages[2].isUnlocked);
    
    // Stage 4: 25% principal (remaining)
    EXPECT_EQ(stages[3].stageNumber, 4);
    EXPECT_EQ(stages[3].unlockHeight, depositHeight + (4 * StagedUnlockConfig::STAGE_INTERVAL_BLOCKS));
    EXPECT_EQ(stages[3].principalAmount, amount - ((amount * 25) / 100) - ((amount * 25) / 100) - ((amount * 25) / 100));
    EXPECT_EQ(stages[3].interestAmount, 0);
    EXPECT_FALSE(stages[3].isUnlocked);
}

TEST_F(StagedDepositUnlockTest, StageUnlocking) {
    uint64_t amount = 1000000000; // 1000 XFG
    uint64_t interest = 100000000; // 100 XFG
    uint32_t depositHeight = 1000;
    
    StagedDepositUnlock stagedUnlock(amount, interest, depositHeight);
    
    // Test stage 1 unlock
    uint32_t stage1Height = depositHeight + StagedUnlockConfig::STAGE_INTERVAL_BLOCKS;
    auto newlyUnlocked = stagedUnlock.checkUnlockStages(stage1Height);
    
    EXPECT_EQ(newlyUnlocked.size(), 1);
    EXPECT_EQ(newlyUnlocked[0].stageNumber, 1);
    EXPECT_TRUE(newlyUnlocked[0].isUnlocked);
    
    // Check totals after stage 1
    uint64_t expectedStage1Amount = ((amount * 25) / 100) + interest;
    EXPECT_EQ(stagedUnlock.getTotalUnlockedAmount(), expectedStage1Amount);
    EXPECT_EQ(stagedUnlock.getRemainingLockedAmount(), amount + interest - expectedStage1Amount);
    
    // Test stage 2 unlock
    uint32_t stage2Height = depositHeight + (2 * StagedUnlockConfig::STAGE_INTERVAL_BLOCKS);
    newlyUnlocked = stagedUnlock.checkUnlockStages(stage2Height);
    
    EXPECT_EQ(newlyUnlocked.size(), 1);
    EXPECT_EQ(newlyUnlocked[0].stageNumber, 2);
    EXPECT_TRUE(newlyUnlocked[0].isUnlocked);
    
    // Test stage 3 unlock
    uint32_t stage3Height = depositHeight + (3 * StagedUnlockConfig::STAGE_INTERVAL_BLOCKS);
    newlyUnlocked = stagedUnlock.checkUnlockStages(stage3Height);
    
    EXPECT_EQ(newlyUnlocked.size(), 1);
    EXPECT_EQ(newlyUnlocked[0].stageNumber, 3);
    EXPECT_TRUE(newlyUnlocked[0].isUnlocked);
    
    // Test stage 4 unlock
    uint32_t stage4Height = depositHeight + (4 * StagedUnlockConfig::STAGE_INTERVAL_BLOCKS);
    newlyUnlocked = stagedUnlock.checkUnlockStages(stage4Height);
    
    EXPECT_EQ(newlyUnlocked.size(), 1);
    EXPECT_EQ(newlyUnlocked[0].stageNumber, 4);
    EXPECT_TRUE(newlyUnlocked[0].isUnlocked);
    
    // Check final state
    EXPECT_EQ(stagedUnlock.getTotalUnlockedAmount(), amount + interest);
    EXPECT_EQ(stagedUnlock.getRemainingLockedAmount(), 0);
    EXPECT_TRUE(stagedUnlock.isFullyUnlocked());
}

TEST_F(StagedDepositUnlockTest, EnhancedDeposit) {
    // Create a basic deposit
    Deposit basicDeposit;
    basicDeposit.amount = 1000000000; // 1000 XFG
    basicDeposit.term = 90; // 90 days
    basicDeposit.interest = 100000000; // 100 XFG
    basicDeposit.height = 1000;
    basicDeposit.unlockHeight = 1000 + 90; // Traditional unlock
    basicDeposit.locked = true;
    basicDeposit.creatingTransactionId = 1;
    basicDeposit.spendingTransactionId = 0;
    
    // Convert to enhanced deposit
    EnhancedDeposit enhancedDeposit;
    enhancedDeposit.initializeFromDeposit(basicDeposit);
    
    // Check that it uses staged unlocking (non-FOREVER term)
    EXPECT_TRUE(enhancedDeposit.useStagedUnlock);
    EXPECT_EQ(enhancedDeposit.amount, basicDeposit.amount);
    EXPECT_EQ(enhancedDeposit.interest, basicDeposit.interest);
    EXPECT_EQ(enhancedDeposit.height, basicDeposit.height);
    
    // Test unlock process
    uint32_t currentHeight = 1000 + StagedUnlockConfig::STAGE_INTERVAL_BLOCKS;
    auto newlyUnlocked = enhancedDeposit.processUnlock(currentHeight);
    
    EXPECT_EQ(newlyUnlocked.size(), 1);
    EXPECT_EQ(newlyUnlocked[0].stageNumber, 1);
    EXPECT_TRUE(newlyUnlocked[0].isUnlocked);
    
    // Check status
    std::string status = EnhancedDepositManager::getUnlockStatus(enhancedDeposit, currentHeight);
    EXPECT_FALSE(status.empty());
    EXPECT_TRUE(status.find("Staged Unlock") != std::string::npos);
}

TEST_F(StagedDepositUnlockTest, StagedUnlockManager) {
    // Test manager functions
    EXPECT_TRUE(StagedUnlockManager::shouldUseStagedUnlock(90)); // Regular term
    EXPECT_FALSE(StagedUnlockManager::shouldUseStagedUnlock(parameters::DEPOSIT_TERM_FOREVER)); // FOREVER term
    
    // Test schedule generation
    auto schedule = StagedUnlockManager::getUnlockSchedule(1000000000, 100000000, 1000);
    EXPECT_EQ(schedule.size(), 4);
    
    // Test deposit processing
    std::vector<Deposit> deposits;
    Deposit deposit1;
    deposit1.amount = 1000000000;
    deposit1.interest = 100000000;
    deposit1.height = 1000;
    deposit1.term = 90;
    deposits.push_back(deposit1);
    
    uint32_t currentHeight = 1000 + StagedUnlockConfig::STAGE_INTERVAL_BLOCKS;
    auto newlyUnlocked = StagedUnlockManager::processStagedUnlocks(currentHeight, deposits);
    
    EXPECT_EQ(newlyUnlocked.size(), 1);
    EXPECT_EQ(newlyUnlocked[0], 0); // First deposit
}

TEST_F(StagedDepositUnlockTest, Serialization) {
    // Test serialization
    StagedDepositUnlock original(1000000000, 100000000, 1000);
    
    // Serialize
    std::ostringstream oss;
    BinaryOutputStreamSerializer serializer(oss);
    original.serialize(serializer);
    
    // Deserialize
    std::istringstream iss(oss.str());
    BinaryInputStreamSerializer deserializer(iss);
    StagedDepositUnlock restored;
    restored.serialize(deserializer);
    
    // Compare
    EXPECT_EQ(original.getTotalUnlockedAmount(), restored.getTotalUnlockedAmount());
    EXPECT_EQ(original.getRemainingLockedAmount(), restored.getRemainingLockedAmount());
    EXPECT_EQ(original.isFullyUnlocked(), restored.isFullyUnlocked());
    
    auto originalStages = original.getStages();
    auto restoredStages = restored.getStages();
    EXPECT_EQ(originalStages.size(), restoredStages.size());
    
    for (size_t i = 0; i < originalStages.size(); ++i) {
        EXPECT_EQ(originalStages[i].stageNumber, restoredStages[i].stageNumber);
        EXPECT_EQ(originalStages[i].unlockHeight, restoredStages[i].unlockHeight);
        EXPECT_EQ(originalStages[i].principalAmount, restoredStages[i].principalAmount);
        EXPECT_EQ(originalStages[i].interestAmount, restoredStages[i].interestAmount);
    }
}

TEST_F(StagedDepositUnlockTest, EdgeCases) {
    // Test zero amount
    StagedDepositUnlock zeroUnlock(0, 0, 1000);
    EXPECT_EQ(zeroUnlock.getTotalUnlockedAmount(), 0);
    EXPECT_EQ(zeroUnlock.getRemainingLockedAmount(), 0);
    EXPECT_TRUE(zeroUnlock.isFullyUnlocked());
    
    // Test very small amounts
    StagedDepositUnlock smallUnlock(1, 1, 1000);
    auto stages = smallUnlock.getStages();
    EXPECT_EQ(stages.size(), 4);
    
    // Test large amounts
    StagedDepositUnlock largeUnlock(1000000000000, 100000000000, 1000);
    stages = largeUnlock.getStages();
    EXPECT_EQ(stages.size(), 4);
    
    // Verify total amounts are preserved
    uint64_t totalPrincipal = 0;
    uint64_t totalInterest = 0;
    for (const auto& stage : stages) {
        totalPrincipal += stage.principalAmount;
        totalInterest += stage.interestAmount;
    }
    EXPECT_EQ(totalPrincipal, 1000000000000);
    EXPECT_EQ(totalInterest, 100000000000);
}

} // namespace CryptoNote