// Copyright (c) 2017-2026 Fuego Developers
//
// This file is part of Fuego.
//
// Fuego is free software distributed under the GNU General Public License v3.
// See file labeled LICENSE for more details.

#include "AdaptiveDifficulty.h"
#include "../CryptoNoteConfig.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace CryptoNote {

// ============================================================================
// DMWDA PARAMETER EPOCH TABLES
//
// To tune difficulty without a chain reset or major-version bump:
//   1. Add a new entry to TESTNET_DMWDA_EPOCHS (or MAINNET_DMWDA_EPOCHS).
//   2. Set activationHeight to a future block height on that network.
//   3. Rebuild and deploy. Old blocks re-validate with their epoch's config.
//
// Epochs MUST be sorted by activationHeight ascending.
// ============================================================================

struct DmwdaEpoch {
    uint32_t activationHeight;
    AdaptiveDifficulty::DifficultyConfig config;
};

// ---------------------------------------------------------------------------
// TESTNET epochs
// ---------------------------------------------------------------------------
static const DmwdaEpoch TESTNET_DMWDA_EPOCHS[] = {
    {
        // Epoch 0 — v10 genesis. Fixed-indexing DMWDA (uses recent blocks correctly).
        // Tuned for single-miner testnet with bursty behaviour.
        /* activationHeight */ 0,
        {
            /* targetTime                */ 480,
            /* shortWindow               */ 8,
            /* mediumWindow              */ 20,
            /* longWindow                */ 45,
            /* emergencyWindow           */ 5,
            /* minAdjustment             */ 0.70,   // max 30% drop per block
            /* maxAdjustment             */ 5.00,   // up to 5x rise per block
            /* emergencyThreshold        */ 0.40,   // emergency clamp [0.4x, 2.5x]
            /* smoothingFactor           */ 0.70,   // 70% new / 30% prev
            /* weightShort               */ 0.60,   // volatile → trust recent
            /* weightMedium              */ 0.30,   // fixed anchor
            /* weightLong                */ 0.10,   // stable → smooth with history
            /* confidenceMin             */ 0.10,
            /* confidenceMax             */ 1.00,
            /* defaultConfidence         */ 0.50,
            /* recentWindowSize          */ 5,
            /* historicalWindowSize      */ 20,
            /* blockStealingCheckBlocks  */ 5,
            /* blockStealingTimeThreshold*/ 0.10,   // <48s = "fast" block
            /* blockStealingThreshold    */ 4,      // 4 of 5 fast blocks triggers
            /* hashRateChangeThreshold   */ 5.0,
        }
    },
    // Add future testnet epochs here, e.g.:
    // { 5000, { 480, 10, 30, 60, 5, 0.75, 4.0, 0.40, 0.70, 0.55, 0.30, 0.15, 0.10, 1.0, 0.50, 5, 20, 5, 0.10, 4, 5.0 } },
};

// ---------------------------------------------------------------------------
// MAINNET epochs
// ---------------------------------------------------------------------------
static const DmwdaEpoch MAINNET_DMWDA_EPOCHS[] = {
    {
        // Epoch 0 — activates at UPGRADE_HEIGHT_V10 (999999).
        // Conservative multi-miner mainnet params.
        /* activationHeight */ 0,
        {
            /* targetTime                */ 480,
            /* shortWindow               */ parameters::DMWDA_SHORT_WINDOW,
            /* mediumWindow              */ parameters::DMWDA_MEDIUM_WINDOW,
            /* longWindow                */ parameters::DMWDA_LONG_WINDOW,
            /* emergencyWindow           */ parameters::DMWDA_EMERGENCY_WINDOW,
            /* minAdjustment             */ parameters::DMWDA_MIN_ADJUSTMENT,
            /* maxAdjustment             */ parameters::DMWDA_MAX_ADJUSTMENT,
            /* emergencyThreshold        */ parameters::DMWDA_EMERGENCY_THRESHOLD,
            /* smoothingFactor           */ parameters::DMWDA_SMOOTHING_FACTOR,
            /* weightShort               */ parameters::DMWDA_WEIGHT_SHORT,
            /* weightMedium              */ parameters::DMWDA_WEIGHT_MEDIUM,
            /* weightLong                */ parameters::DMWDA_WEIGHT_LONG,
            /* confidenceMin             */ parameters::DMWDA_CONFIDENCE_MIN,
            /* confidenceMax             */ parameters::DMWDA_CONFIDENCE_MAX,
            /* defaultConfidence         */ parameters::DMWDA_DEFAULT_CONFIDENCE,
            /* recentWindowSize          */ parameters::DMWDA_RECENT_WINDOW_SIZE,
            /* historicalWindowSize      */ parameters::DMWDA_HISTORICAL_WINDOW_SIZE,
            /* blockStealingCheckBlocks  */ parameters::DMWDA_BLOCK_STEALING_CHECK_BLOCKS,
            /* blockStealingTimeThreshold*/ parameters::DMWDA_BLOCK_STEALING_TIME_THRESHOLD,
            /* blockStealingThreshold    */ parameters::DMWDA_BLOCK_STEALING_THRESHOLD,
            /* hashRateChangeThreshold   */ parameters::DMWDA_HASH_RATE_CHANGE_THRESHOLD,
        }
    },
    // Add future mainnet epochs here.
};

// ---------------------------------------------------------------------------
// Epoch lookup — returns config for the latest epoch whose activationHeight <= height
// ---------------------------------------------------------------------------
AdaptiveDifficulty::DifficultyConfig getDefaultFuegoConfig(bool testnet, uint32_t height) {
    const DmwdaEpoch* epochs     = testnet ? TESTNET_DMWDA_EPOCHS : MAINNET_DMWDA_EPOCHS;
    size_t            epochCount = testnet ? (sizeof(TESTNET_DMWDA_EPOCHS) / sizeof(TESTNET_DMWDA_EPOCHS[0]))
                                           : (sizeof(MAINNET_DMWDA_EPOCHS) / sizeof(MAINNET_DMWDA_EPOCHS[0]));

    // Walk backwards — pick the last epoch whose activationHeight <= height
    const AdaptiveDifficulty::DifficultyConfig* active = &epochs[0].config;
    for (size_t i = 0; i < epochCount; ++i) {
        if (height >= epochs[i].activationHeight) {
            active = &epochs[i].config;
        }
    }
    return *active;
}

// ============================================================================
// AdaptiveDifficulty implementation
// ============================================================================

AdaptiveDifficulty::AdaptiveDifficulty(const DifficultyConfig& config)
    : m_config(config) {
}

uint64_t AdaptiveDifficulty::calculateNextDifficulty(
    uint32_t height,
    const std::vector<uint64_t>& timestamps,
    const std::vector<uint64_t>& cumulativeDifficulties,
    bool testnet) {

    if (timestamps.size() < 3) {
        return 10000;
    }

    if (detectHashRateAnomaly(timestamps, cumulativeDifficulties, testnet)) {
        return calculateEmergencyDifficulty(timestamps, cumulativeDifficulties, testnet);
    }

    if (detectBlockStealingAttempt(timestamps, cumulativeDifficulties, testnet)) {
        return calculateEmergencyDifficulty(timestamps, cumulativeDifficulties, testnet);
    }

    return calculateMultiWindowDifficulty(timestamps, cumulativeDifficulties, testnet);
}

uint64_t AdaptiveDifficulty::calculateMultiWindowDifficulty(
    const std::vector<uint64_t>& timestamps,
    const std::vector<uint64_t>& cumulativeDifficulties,
    bool testnet) {

    size_t n = timestamps.size();

    double shortLWMA  = calculateLWMA(timestamps, cumulativeDifficulties, m_config.shortWindow);
    double mediumLWMA = calculateLWMA(timestamps, cumulativeDifficulties, m_config.mediumWindow);
    double longLWMA   = calculateLWMA(timestamps, cumulativeDifficulties, m_config.longWindow);

    double confidence = calculateConfidenceScore(timestamps, cumulativeDifficulties, testnet);

    // Low confidence (volatile) → more weight on short window (recent data).
    // High confidence (stable)  → more weight on long window  (smooth history).
    double shortWeight  = m_config.weightShort  * (1.0 - confidence);
    double mediumWeight = m_config.weightMedium;
    double longWeight   = m_config.weightLong   * confidence;

    double totalWeight = shortWeight + mediumWeight + longWeight;
    if (totalWeight <= 0.0) totalWeight = 1.0;

    double weightedSolveTime = (shortLWMA * shortWeight + mediumLWMA * mediumWeight + longLWMA * longWeight)
                               / totalWeight;

    // avgDifficulty: most recent mediumWindow blocks (end of array = newest).
    uint32_t effectiveWindow = std::min(static_cast<uint32_t>(n - 1), m_config.mediumWindow);
    if (effectiveWindow == 0) return 10000;

    uint64_t avgDifficulty = (cumulativeDifficulties[n - 1] - cumulativeDifficulties[n - 1 - effectiveWindow])
                             / effectiveWindow;
    if (avgDifficulty < 10000) avgDifficulty = 10000;

    if (weightedSolveTime < m_config.targetTime / 1000.0) {
        weightedSolveTime = m_config.targetTime / 1000.0;
    }

    double difficultyRatio = static_cast<double>(m_config.targetTime) / weightedSolveTime;
    difficultyRatio = std::max(m_config.minAdjustment, std::min(m_config.maxAdjustment, difficultyRatio));

    double calculatedDifficulty = static_cast<double>(avgDifficulty) * difficultyRatio;
    if (calculatedDifficulty > static_cast<double>(std::numeric_limits<uint64_t>::max())) {
        calculatedDifficulty = static_cast<double>(std::numeric_limits<uint64_t>::max());
    }

    uint64_t newDifficulty = static_cast<uint64_t>(calculatedDifficulty);

    // Smooth against most recent block's actual difficulty.
    if (n >= 2) {
        uint64_t prevDifficulty = cumulativeDifficulties[n - 1] - cumulativeDifficulties[n - 2];
        newDifficulty = applySmoothing(newDifficulty, prevDifficulty, testnet);
    }

    return std::max(static_cast<uint64_t>(10000), newDifficulty);
}

double AdaptiveDifficulty::calculateLWMA(
    const std::vector<uint64_t>& timestamps,
    const std::vector<uint64_t>& cumulativeDifficulties,
    uint32_t windowSize) {

    size_t   n               = timestamps.size();
    uint32_t effectiveWindow = std::min(static_cast<uint32_t>(n - 1), windowSize);
    if (effectiveWindow > 200) effectiveWindow = 200;
    if (effectiveWindow == 0)  return static_cast<double>(m_config.targetTime);

    // Use MOST RECENT effectiveWindow blocks (end of array = newest).
    size_t startIdx = n - 1 - effectiveWindow;

    double weightedSum = 0.0;
    double weightSum   = 0.0;

    for (uint32_t j = 0; j < effectiveWindow; ++j) {
        size_t  idx       = startIdx + 1 + j;
        int64_t solveTime = static_cast<int64_t>(timestamps[idx]) - static_cast<int64_t>(timestamps[idx - 1]);

        // Clamp: min=T/10, max=T*3.  T*3 limit prevents long offline gaps from
        // poisoning the window (old code used T*10 = 80 min which was too much).
        solveTime = std::max(static_cast<int64_t>(m_config.targetTime / 10),
                             std::min(static_cast<int64_t>(m_config.targetTime * 3), solveTime));

        // LWMA-1 increasing weight: oldest=1, newest=effectiveWindow.
        double weight  = static_cast<double>(j + 1);
        weightedSum   += static_cast<double>(solveTime) * weight;
        weightSum     += weight;
    }

    if (weightSum == 0.0) return static_cast<double>(m_config.targetTime);

    double lwma = weightedSum / weightSum;
    if (lwma < m_config.targetTime / 100.0) lwma = m_config.targetTime / 100.0;
    return lwma;
}

double AdaptiveDifficulty::calculateEMA(
    const std::vector<uint64_t>& timestamps,
    uint32_t windowSize,
    double alpha) {

    size_t   n               = timestamps.size();
    uint32_t effectiveWindow = std::min(static_cast<uint32_t>(n - 1), windowSize);
    if (effectiveWindow == 0) return static_cast<double>(m_config.targetTime);

    size_t startIdx = n - 1 - effectiveWindow;

    double ema = static_cast<double>(timestamps[startIdx + 1] - timestamps[startIdx]);
    for (uint32_t j = 1; j < effectiveWindow; ++j) {
        size_t  idx       = startIdx + 1 + j;
        int64_t solveTime = static_cast<int64_t>(timestamps[idx]) - static_cast<int64_t>(timestamps[idx - 1]);
        solveTime = std::max(static_cast<int64_t>(m_config.targetTime / 10),
                             std::min(static_cast<int64_t>(m_config.targetTime * 10), solveTime));
        ema = alpha * static_cast<double>(solveTime) + (1.0 - alpha) * ema;
    }
    return ema;
}

uint64_t AdaptiveDifficulty::calculateEmergencyDifficulty(
    const std::vector<uint64_t>& timestamps,
    const std::vector<uint64_t>& cumulativeDifficulties,
    bool testnet) {

    size_t   n               = timestamps.size();
    uint32_t emergencyWindow = std::min(static_cast<uint32_t>(n - 1), m_config.emergencyWindow);
    if (emergencyWindow == 0) return 10000;

    // Most recent emergencyWindow blocks.
    size_t startIdx = n - 1 - emergencyWindow;

    double recentSolveTime = static_cast<double>(timestamps[n - 1] - timestamps[startIdx]) / emergencyWindow;
    if (recentSolveTime <= 0.0) return 10000;

    uint64_t currentDifficulty = (cumulativeDifficulties[n - 1] - cumulativeDifficulties[startIdx]) / emergencyWindow;
    if (currentDifficulty < 10000) currentDifficulty = 10000;

    double emergencyRatio = static_cast<double>(m_config.targetTime) / recentSolveTime;
    emergencyRatio = std::max(m_config.emergencyThreshold,
                              std::min(1.0 / m_config.emergencyThreshold, emergencyRatio));

    double calc = static_cast<double>(currentDifficulty) * emergencyRatio;
    if (calc > static_cast<double>(std::numeric_limits<uint64_t>::max())) {
        calc = static_cast<double>(std::numeric_limits<uint64_t>::max());
    }

    uint64_t emergencyDiff = static_cast<uint64_t>(calc);

    // Smooth in emergency path too — prevents giant single-block swings.
    if (n >= 2) {
        uint64_t prevDiff = cumulativeDifficulties[n - 1] - cumulativeDifficulties[n - 2];
        emergencyDiff = applySmoothing(emergencyDiff, prevDiff, testnet);
    }

    return std::max(static_cast<uint64_t>(10000), emergencyDiff);
}

bool AdaptiveDifficulty::detectHashRateAnomaly(
    const std::vector<uint64_t>& timestamps,
    const std::vector<uint64_t>& difficulties,
    bool testnet) {

    size_t n = timestamps.size();
    if (n < 5) return false;

    uint32_t recentW = std::min(m_config.recentWindowSize,    static_cast<uint32_t>(n - 1));
    uint32_t histW   = std::min(m_config.historicalWindowSize, static_cast<uint32_t>(n - 1));

    if (histW <= recentW) return false;

    // Recent = last recentW blocks; historical = the period just before that.
    double recentSolveTime     = static_cast<double>(timestamps[n - 1] - timestamps[n - 1 - recentW]) / recentW;
    double historicalSolveTime = static_cast<double>(timestamps[n - 1 - recentW] - timestamps[n - 1 - histW])
                                 / static_cast<double>(histW - recentW);

    if (historicalSolveTime <= 0.0) return false;

    double ratio = recentSolveTime / historicalSolveTime;
    return (ratio < 1.0 / m_config.hashRateChangeThreshold || ratio > m_config.hashRateChangeThreshold);
}

bool AdaptiveDifficulty::detectBlockStealingAttempt(
    const std::vector<uint64_t>& timestamps,
    const std::vector<uint64_t>& difficulties,
    bool testnet) {

    size_t n = timestamps.size();
    if (n < 3) return false;

    uint32_t checkBlocks = std::min(m_config.blockStealingCheckBlocks, static_cast<uint32_t>(n - 1));
    uint32_t fastCount   = 0;

    // Check MOST RECENT checkBlocks blocks.
    for (size_t i = n - checkBlocks; i < n; ++i) {
        int64_t solveTime = static_cast<int64_t>(timestamps[i]) - static_cast<int64_t>(timestamps[i - 1]);
        if (solveTime < static_cast<int64_t>(m_config.targetTime * m_config.blockStealingTimeThreshold)) {
            ++fastCount;
        }
    }

    return fastCount >= m_config.blockStealingThreshold;
}

uint64_t AdaptiveDifficulty::applySmoothing(uint64_t newDifficulty, uint64_t previousDifficulty, bool testnet) {
    double alpha    = m_config.smoothingFactor;
    double smoothed = alpha * static_cast<double>(newDifficulty)
                    + (1.0 - alpha) * static_cast<double>(previousDifficulty);

    if (smoothed > static_cast<double>(std::numeric_limits<uint64_t>::max())) {
        return std::numeric_limits<uint64_t>::max();
    }
    return static_cast<uint64_t>(smoothed);
}

double AdaptiveDifficulty::calculateConfidenceScore(
    const std::vector<uint64_t>& timestamps,
    const std::vector<uint64_t>& difficulties,
    bool testnet) {

    size_t n = timestamps.size();
    if (n < 3) return m_config.defaultConfidence;

    // Use most recent 15 blocks for confidence — old data shouldn't affect variance measure.
    size_t windowForConfidence = std::min(static_cast<size_t>(15), n - 1);
    size_t startIdx            = n - 1 - windowForConfidence;

    std::vector<double> solveTimes;
    solveTimes.reserve(windowForConfidence);
    for (size_t i = startIdx + 1; i < n; ++i) {
        solveTimes.push_back(static_cast<double>(timestamps[i] - timestamps[i - 1]));
    }

    if (solveTimes.empty()) return m_config.defaultConfidence;

    double mean = std::accumulate(solveTimes.begin(), solveTimes.end(), 0.0) / solveTimes.size();
    if (mean <= 0.0) return m_config.confidenceMin;

    double variance = 0.0;
    for (double st : solveTimes) {
        variance += (st - mean) * (st - mean);
    }
    variance /= solveTimes.size();

    // Low CV → stable → high confidence. High CV → volatile → low confidence.
    double cv         = std::sqrt(variance) / mean;
    double confidence = 1.0 - std::min(1.0, cv);

    return std::max(m_config.confidenceMin, std::min(m_config.confidenceMax, confidence));
}

} // namespace CryptoNote
