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

#include "gtest/gtest.h"
#include <tuple>

#include "CryptoNoteCore/TransactionApi.h"
#include "Logging/ConsoleLogger.h"
#include "Transfers/TransfersSubscription.h"
#include "Transfers/TypeHelpers.h"
#include "ITransfersContainer.h"

#include "TransactionApiHelpers.h"
#include "TransfersObserver.h"

using namespace CryptoNote;

namespace {

const uint32_t UNCONFIRMED_TRANSACTION_HEIGHT = std::numeric_limits<uint32_t>::max();
const uint32_t UNCONFIRMED = std::numeric_limits<uint32_t>::max();

std::error_code createError() {
  return std::make_error_code(std::errc::invalid_argument);
}


class TransfersSubscriptionTest : public ::testing::Test {
public:

  TransfersSubscriptionTest() :
    currency(CurrencyBuilder(m_logger).currency()),
    account(generateAccountKeys()),
    syncStart(SynchronizationStart{ 0, 0 }),
    sub(currency, AccountSubscription{ account, syncStart, 10 }) {
    sub.addObserver(&observer);
  }

  std::shared_ptr<ITransactionReader> addTransaction(uint64_t amount, uint32_t height, uint32_t outputIndex) {
    TestTransactionBuilder b1;
    auto unknownSender = generateAccountKeys();
    b1.addTestInput(amount, unknownSender);
    auto outInfo = b1.addTestKeyOutput(amount, outputIndex, account);
    auto tx = std::shared_ptr<ITransactionReader>(b1.build().release());

    std::vector<TransactionOutputInformationIn> outputs = { outInfo };
    sub.addTransaction(TransactionBlockInfo{ height, 100000 }, *tx, outputs);
    return tx;
  }

  Logging::ConsoleLogger m_logger;
  Currency currency;
  AccountKeys account;
  SynchronizationStart syncStart;
  TransfersSubscription sub;
  TransfersObserver observer;
};
}



TEST_F(TransfersSubscriptionTest, getInitParameters) {
  ASSERT_EQ(syncStart.height, sub.getSyncStart().height);
  ASSERT_EQ(syncStart.timestamp, sub.getSyncStart().timestamp);
  ASSERT_EQ(account.address, sub.getAddress());
  ASSERT_EQ(account, sub.getKeys());
}

TEST_F(TransfersSubscriptionTest, addTransaction) {
  auto tx1 = addTransaction(10000, 1, 0);
  auto tx2 = addTransaction(10000, 2, 1);

  // this transaction should not be added, so no notification
  auto tx = createTransaction();
  addTestInput(*tx, 20000);
  sub.addTransaction(TransactionBlockInfo{ 2, 100000 }, *tx, {});

  ASSERT_EQ(2, sub.getContainer().transactionsCount());
  ASSERT_EQ(2, observer.updated.size());
  ASSERT_EQ(tx1->getTransactionHash(), observer.updated[0]);
  ASSERT_EQ(tx2->getTransactionHash(), observer.updated[1]);
}

TEST_F(TransfersSubscriptionTest, onBlockchainDetach) {
  addTransaction(10000, 10, 0);
  auto txHash = addTransaction(10000, 11, 1)->getTransactionHash();
  ASSERT_EQ(2, sub.getContainer().transactionsCount());
  
  sub.onBlockchainDetach(11);

  ASSERT_EQ(1, sub.getContainer().transactionsCount());
  ASSERT_EQ(1, observer.deleted.size());
  ASSERT_EQ(txHash, observer.deleted[0]);
}

TEST_F(TransfersSubscriptionTest, onError) {

  auto err = createError();

  addTransaction(10000, 10, 0);
  addTransaction(10000, 11, 1);

  ASSERT_EQ(2, sub.getContainer().transactionsCount());

  sub.onError(err, 12);

  ASSERT_EQ(2, sub.getContainer().transactionsCount());
  ASSERT_EQ(1, observer.errors.size());
  ASSERT_EQ(std::make_tuple(12, err), observer.errors[0]);

  sub.onError(err, 11);

  ASSERT_EQ(1, sub.getContainer().transactionsCount()); // one transaction should be detached
  ASSERT_EQ(2, observer.errors.size());

  ASSERT_EQ(std::make_tuple(12, err), observer.errors[0]);
  ASSERT_EQ(std::make_tuple(11, err), observer.errors[1]);
}

TEST_F(TransfersSubscriptionTest, advanceHeight) {
  ASSERT_TRUE(sub.advanceHeight(10));
  ASSERT_FALSE(sub.advanceHeight(9)); // can't go backwards
}


TEST_F(TransfersSubscriptionTest, markTransactionConfirmed) {
  auto txHash = addTransaction(10000, UNCONFIRMED_TRANSACTION_HEIGHT, UNCONFIRMED)->getTransactionHash();
  ASSERT_EQ(1, sub.getContainer().transactionsCount());
  ASSERT_EQ(1, observer.updated.size()); // added

  sub.markTransactionConfirmed(TransactionBlockInfo{ 10, 100000 }, txHash, { 1 });

  ASSERT_EQ(2, observer.updated.size()); // added + updated
  ASSERT_EQ(txHash, observer.updated[0]);
}

TEST_F(TransfersSubscriptionTest, deleteUnconfirmedTransaction) {
  auto txHash = addTransaction(10000, UNCONFIRMED_TRANSACTION_HEIGHT, UNCONFIRMED)->getTransactionHash();
  ASSERT_EQ(1, sub.getContainer().transactionsCount());

  sub.deleteUnconfirmedTransaction(txHash);

  ASSERT_EQ(0, sub.getContainer().transactionsCount());
  ASSERT_EQ(1, observer.deleted.size());
  ASSERT_EQ(txHash, observer.deleted[0]);
}
