// Copyright (c) 2017-2025 Fuego Developers
// Copyright (c) 2020-2025 Elderfire Privacy Group
// Copyright (c) 2011-2017 The Cryptonote developers
//
// This file is part of Fuego.
//
// Fuego is free software distributed in the hope that it
// will be useful- but WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. You are encouraged to redistribute it and/or modify it
// under the terms of the GNU General Public License v3 or later
// versions as published by the Free Software Foundation.
// You should receive a copy of the GNU General Public License
// along with Fuego. If not, see <https://www.gnu.org/licenses/>

#pragma once

#include <vector>
#include <boost/variant.hpp>
#include "CryptoTypes.h"

namespace CryptoNote {

struct BaseInput {
  uint32_t blockIndex;
};

struct KeyInput {
  uint64_t amount;
  std::vector<uint32_t> outputIndexes;
  Crypto::KeyImage keyImage;
};

struct MultisignatureInput {
  uint64_t amount;
  uint8_t signatureCount;
  uint32_t outputIndex;
  uint32_t term;
};

struct KeyOutput {
  Crypto::PublicKey key;
};

struct MultisignatureOutput {
  std::vector<Crypto::PublicKey> keys;
  uint8_t requiredSignatureCount;
  uint32_t term;
};

// v10+ ring-signature deposit output.
// Replaces MultisignatureOutput for ALL deposit types: COLD, HEAT burns, Elderfier stakes.
// Amount lives in TransactionOutput.amount (same as KeyOutput).
// HEAT burns use throwaway commitKey (secret discarded) and never withdraw but
// serve as excellent decoys, bulking up decoy pool for COLD/EF withdrawal rings.
// Ring selection by amount only so all commitment outputs
// matching amount are eligible decoys regardless of term.
struct TransactionOutputCommitment {
  Crypto::PublicKey commitKey; // = scalar(H("commit_key"||depositSecret)) * G
  uint32_t term;               // lock term in blocks (same meaning as MultisignatureOutput.term)
};

// v10+ ring-signature withdrawal input.
// Replaces MultisignatureInput for COLD/Elderfier withdrawals.
// outputIndexes are GLOBAL commitment output indices (like KeyInput for key outputs).
struct TransactionInputCommitmentSpend {
  uint64_t amount;                      // must match referenced commitment output amount
  std::vector<uint32_t> outputIndexes;  // ring: global commitment output indices (relative offsets, decoded absolute on verify)
  Crypto::KeyImage keyImage;            // H_p(commitKey) * keyScalar — double-spend prevention via m_spent_keys
};

typedef boost::variant<BaseInput, KeyInput, MultisignatureInput, TransactionInputCommitmentSpend> TransactionInput;

typedef boost::variant<KeyOutput, MultisignatureOutput, TransactionOutputCommitment> TransactionOutputTarget;

struct TransactionOutput {
  uint64_t amount;
  TransactionOutputTarget target;
};

using TransactionInputs = std::vector<TransactionInput>;

struct TransactionPrefix {
  uint8_t version;
  uint64_t unlockTime;
  TransactionInputs inputs;
  std::vector<TransactionOutput> outputs;
  std::vector<uint8_t> extra;
};

struct Transaction : public TransactionPrefix {
  std::vector<std::vector<Crypto::Signature>> signatures;
};

struct ParentBlock {
  uint8_t majorVersion;
  uint8_t minorVersion;
  Crypto::Hash previousBlockHash;
  uint16_t transactionCount;
  std::vector<Crypto::Hash> baseTransactionBranch;
  Transaction baseTransaction;
  std::vector<Crypto::Hash> blockchainBranch;
};

struct BlockHeader {
  uint8_t majorVersion;
  uint8_t minorVersion;
  uint32_t nonce;
  uint64_t timestamp;
  Crypto::Hash previousBlockHash;
};

struct Block : public BlockHeader {
  ParentBlock parentBlock;
  Transaction baseTransaction;
  std::vector<Crypto::Hash> transactionHashes;
};

struct AccountPublicAddress {
  Crypto::PublicKey spendPublicKey;
  Crypto::PublicKey viewPublicKey;
};

struct AccountKeys {
  AccountPublicAddress address;
  Crypto::SecretKey spendSecretKey;
  Crypto::SecretKey viewSecretKey;
};

struct KeyPair {
  Crypto::PublicKey publicKey;
  Crypto::SecretKey secretKey;
};

using BinaryArray = std::vector<uint8_t>;

}
