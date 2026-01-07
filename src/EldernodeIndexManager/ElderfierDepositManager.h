// Copyright (c) 2017-2026 Fuego Developers
//
// This file is part of Fuego.
//
// Fuego is free & open source software distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. You may redistribute it and/or modify it under the terms
// of the GNU General Public License v3 or later versions as published
// by the Free Software Foundation. Fuego includes elements written
// by third parties. See file labeled LICENSE for more details.
// You should have received a copy of the GNU General Public License
// along with Fuego. If not, see <https://www.gnu.org/licenses/>.


#pragma once

#include "EldernodeIndexTypes.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "crypto/hash.h"
#include "crypto/crypto.h"
#include <unordered_map>
#include <vector>
#include <string>

namespace CryptoNote {

// Slashing request structure
struct SlashingRequest {
    Crypto::Hash depositHash;
    Crypto::PublicKey elderfierPublicKey;
    std::string reason;
    uint64_t timestamp;
    std::vector<uint8_t> evidence;

    bool isValid() const;
};

// Slashing result structure
struct SlashingResult {
    bool isSuccess;
    std::string message;
    uint64_t slashedAmount;

    static SlashingResult createSuccess(const std::string& message, uint64_t amount = 0);
    static SlashingResult createFailure(const std::string& message);
};

// Elderfier Deposit Manager
class ElderfierDepositManager {
public:
    ElderfierDepositManager();
    ~ElderfierDepositManager();

    // Deposit management
    bool addElderfierDeposit(const TransactionExtraElderfierDeposit& deposit);
    bool removeElderfierDeposit(const Crypto::Hash& depositHash);
    bool updateElderfierDeposit(const Crypto::Hash& depositHash, const TransactionExtraElderfierDeposit& updatedDeposit);

    // Deposit queries
    bool hasElderfierDeposit(const Crypto::Hash& depositHash) const;
    TransactionExtraElderfierDeposit getElderfierDeposit(const Crypto::Hash& depositHash) const;
    std::vector<TransactionExtraElderfierDeposit> getAllElderfierDeposits() const;

    // Deposit validation
    bool isValidElderfierDeposit(const TransactionExtraElderfierDeposit& deposit) const;
    bool isElderfierSlashable(const Crypto::Hash& depositHash) const;

    // Spending detection
    bool checkIfDepositOutputsSpent(const Crypto::Hash& depositHash) const;

    // Slashing operations
    SlashingResult processSlashingRequest(const SlashingRequest& request) const;

    // Statistics
    size_t getDepositCount() const;
    uint64_t getTotalDepositAmount() const;

private:
    std::unordered_map<Crypto::Hash, TransactionExtraElderfierDeposit> m_deposits;
    mutable std::mutex m_mutex;

    // Helper methods
    bool validateDepositAmount(uint64_t amount) const;
    bool validateDepositSignature(const TransactionExtraElderfierDeposit& deposit) const;
};

} // namespace CryptoNote
