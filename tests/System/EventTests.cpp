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

#include <System/Context.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/InterruptedException.h>
#include <gtest/gtest.h>

using namespace System;

TEST(EventTests, newEventIsNotSet) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  ASSERT_FALSE(event.get());
}

TEST(EventTests, eventIsWorking) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, movedEventIsWorking) {
  Dispatcher dispatcher;
  Event event{Event(dispatcher)};
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, movedEventKeepsState) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  Event event2(std::move(event));
  ASSERT_TRUE(event2.get());
}

TEST(EventTests, movedEventIsWorking2) {
  Dispatcher dispatcher;
  Event srcEvent(dispatcher);
  Event event;
  event = std::move(srcEvent);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, movedEventKeepsState2) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  Event dstEvent;
  dstEvent = std::move(event);
  ASSERT_TRUE(dstEvent.get());
}

TEST(EventTests, moveClearsEventState) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
  Event srcEvent(dispatcher);
  event = std::move(srcEvent);
  ASSERT_FALSE(event.get());
}

TEST(EventTests, movedEventIsTheSame) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto eventPtr1 = &event;
  Event srcEvent(dispatcher);
  event = std::move(srcEvent);
  auto eventPtr2 = &event;
  ASSERT_EQ(eventPtr1, eventPtr2);
}

TEST(EventTests, eventIsWorkingAfterClear) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  event.clear();
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, eventIsWorkingAfterClearOnWaiting) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.clear();
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, eventIsReusableAfterClear) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
    dispatcher.yield();
    event.set();
  });

  event.wait();
  event.clear();
  event.wait();
  SUCCEED();
}

TEST(EventTests, eventSetIsWorkingOnNewEvent) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  event.set();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, setActuallySets) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  SUCCEED();
}

TEST(EventTests, setJustSets) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  bool done = false;
  Context<> context(dispatcher, [&]() {
    event.wait();
    done = true;
  });

  dispatcher.yield();
  ASSERT_FALSE(done);
  event.set();
  ASSERT_FALSE(done);
  dispatcher.yield();
  ASSERT_TRUE(done);
}

TEST(EventTests, setSetsOnlyOnce) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    event.set();
    event.set();
    event.set();
    dispatcher.yield();
    i++;
  });

  event.wait();
  i++;
  event.wait();
  ASSERT_EQ(i, 1);
  dispatcher.yield();
  ASSERT_EQ(i, 2);
}

TEST(EventTests, waitIsWaiting) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  bool done = false;
  Context<> context(dispatcher, [&]() {
    event.wait();
    done = true;
  });

  dispatcher.yield();
  ASSERT_FALSE(done);
  event.set();
  dispatcher.yield();
  ASSERT_TRUE(done);
}

TEST(EventTests, setEventIsNotWaiting) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    event.set();
    dispatcher.yield();
    i++;
  });

  event.wait();
  i++;
  ASSERT_EQ(i, 1);
  event.wait();
  ASSERT_EQ(i, 1);
  dispatcher.yield();
  ASSERT_EQ(i, 2);
}

TEST(EventTests, waitIsParallel) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    i++;
    event.set();
  });

  ASSERT_EQ(i, 0);
  event.wait();
  ASSERT_EQ(i, 1);
}

TEST(EventTests, waitIsMultispawn) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    event.wait();
    i++;
  });

  Context<> contextSecond(dispatcher, [&]() {
    event.wait();
    i++;
  });

  ASSERT_EQ(i, 0);
  dispatcher.yield();
  ASSERT_EQ(i, 0);
  event.set();
  dispatcher.yield();
  ASSERT_EQ(i, 2);
}

TEST(EventTests, setEventInPastUnblocksWaitersEvenAfterClear) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    event.wait();
    i++;
  });

  Context<> contextSecond(dispatcher, [&]() {
    event.wait();
    i++;
  });

  dispatcher.yield();
  ASSERT_EQ(i, 0);
  event.set();
  event.clear();
  dispatcher.yield();
  ASSERT_EQ(i, 2);
}

TEST(EventTests, waitIsInterruptibleOnFront) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  bool interrupted = false;
  Context<>(dispatcher, [&] {
    try {
      event.wait();
    } catch (InterruptedException&) {
      interrupted = true;
    }
  });
  
  ASSERT_TRUE(interrupted);  
}

TEST(EventTests, waitIsInterruptibleOnBody) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Event event2(dispatcher);
  bool interrupted = false;
  Context<> context(dispatcher, [&] {
    try {
      event2.set();
      event.wait();
    } catch (InterruptedException&) {
      interrupted = true;
    }
  });

  event2.wait();
  context.interrupt();
  context.get();
  ASSERT_TRUE(interrupted);
}
