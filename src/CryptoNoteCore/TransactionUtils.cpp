// Copyright (c) 2021-2022, The TuringX Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2016 The Cryptonote developers

#include "TransactionUtils.h"

#include <unordered_set>

#include "crypto/crypto.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteFormatUtils.h"
#include "TransactionExtra.h"

using namespace Crypto;

namespace CryptoNote {

bool checkInputsKeyimagesDiff(const CryptoNote::TransactionPrefix& tx) {
  std::unordered_set<Crypto::KeyImage> ki;
  for (const auto& in : tx.inputs) {
    if (in.type() == typeid(KeyInput)) {
      if (!ki.insert(boost::get<KeyInput>(in).keyImage).second)
        return false;
    }
  }
  return true;
}

// TransactionInput helper functions

size_t getRequiredSignaturesCount(const TransactionInput& in) {
  if (in.type() == typeid(KeyInput)) {
    return boost::get<KeyInput>(in).outputIndexes.size();
  }
  if (in.type() == typeid(MultisignatureInput)) {
    return boost::get<MultisignatureInput>(in).signatureCount;
  }
  return 0;
}

uint64_t getTransactionInputAmount(const TransactionInput& in) {
  if (in.type() == typeid(KeyInput)) {
    return boost::get<KeyInput>(in).amount;
  }
  if (in.type() == typeid(MultisignatureInput)) {
    return boost::get<MultisignatureInput>(in).amount;
  }
  return 0;
}

TransactionTypes::InputType getTransactionInputType(const TransactionInput& in) {
  if (in.type() == typeid(KeyInput)) {
    return TransactionTypes::InputType::Key;
  }
  if (in.type() == typeid(MultisignatureInput)) {
    return TransactionTypes::InputType::Multisignature;
  }
  if (in.type() == typeid(BaseInput)) {
    return TransactionTypes::InputType::Generating;
  }
  return TransactionTypes::InputType::Invalid;
}

const TransactionInput& getInputChecked(const CryptoNote::TransactionPrefix& transaction, size_t index) {
  if (transaction.inputs.size() <= index) {
    throw std::runtime_error("Transaction input index out of range");
  }
  return transaction.inputs[index];
}

const TransactionInput& getInputChecked(const CryptoNote::TransactionPrefix& transaction, size_t index, TransactionTypes::InputType type) {
  const auto& input = getInputChecked(transaction, index);
  if (getTransactionInputType(input) != type) {
    throw std::runtime_error("Unexpected transaction input type");
  }
  return input;
}

// TransactionOutput helper functions

TransactionTypes::OutputType getTransactionOutputType(const TransactionOutputTarget& out) {
  if (out.type() == typeid(KeyOutput)) {
    return TransactionTypes::OutputType::Key;
  }
  if (out.type() == typeid(MultisignatureOutput)) {
    return TransactionTypes::OutputType::Multisignature;
  }
  return TransactionTypes::OutputType::Invalid;
}

const TransactionOutput& getOutputChecked(const CryptoNote::TransactionPrefix& transaction, size_t index) {
  if (transaction.outputs.size() <= index) {
    throw std::runtime_error("Transaction output index out of range");
  }
  return transaction.outputs[index];
}

const TransactionOutput& getOutputChecked(const CryptoNote::TransactionPrefix& transaction, size_t index, TransactionTypes::OutputType type) {
  const auto& output = getOutputChecked(transaction, index);
  if (getTransactionOutputType(output.target) != type) {
    throw std::runtime_error("Unexpected transaction output target type");
  }
  return output;
}

bool isOutToKey(const Crypto::PublicKey& spendPublicKey, const Crypto::PublicKey& outKey, const Crypto::KeyDerivation& derivation, size_t keyIndex) {
  Crypto::PublicKey pk;
  derive_public_key(derivation, keyIndex, spendPublicKey, pk);
  return pk == outKey;
}

bool findOutputsToAccount(const CryptoNote::TransactionPrefix& transaction, const AccountPublicAddress& addr,
                          const SecretKey& viewSecretKey, std::vector<uint32_t>& out, uint64_t& amount) {
  AccountKeys keys;
  keys.address = addr;
  // only view secret key is used, spend key is not needed
  keys.viewSecretKey = viewSecretKey;

  Crypto::PublicKey txPubKey = getTransactionPublicKeyFromExtra(transaction.extra);

  amount = 0;
  size_t keyIndex = 0;
  uint32_t outputIndex = 0;

  Crypto::KeyDerivation derivation;
  generate_key_derivation(txPubKey, keys.viewSecretKey, derivation);

  for (const TransactionOutput& o : transaction.outputs) {
    assert(o.target.type() == typeid(KeyOutput) || o.target.type() == typeid(MultisignatureOutput));
    if (o.target.type() == typeid(KeyOutput)) {
      if (is_out_to_acc(keys, boost::get<KeyOutput>(o.target), derivation, keyIndex)) {
        out.push_back(outputIndex);
        amount += o.amount;
      }
      ++keyIndex;
    } else if (o.target.type() == typeid(MultisignatureOutput)) {
      const auto& target = boost::get<MultisignatureOutput>(o.target);
      for (const auto& key : target.keys) {
        if (isOutToKey(keys.address.spendPublicKey, key, derivation, static_cast<size_t>(outputIndex))) {
          out.push_back(outputIndex);
        }
        ++keyIndex;
      }
    }
    ++outputIndex;
  }

  return true;
}

}
