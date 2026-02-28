/* Copyright 2021 Aristocratos (jakob@qvantnet.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

indent = tab
tab-size = 4
*/

#include <gtest/gtest.h>
#include "btop_state.hpp"

using Global::AppState;

// --- Enum value tests ---

TEST(AppState, RunningIsZero) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppState::Running), 0u);
}

TEST(AppState, ResizingIsOne) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppState::Resizing), 1u);
}

TEST(AppState, ReloadingIsTwo) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppState::Reloading), 2u);
}

TEST(AppState, SleepingIsThree) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppState::Sleeping), 3u);
}

TEST(AppState, QuittingIsFour) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppState::Quitting), 4u);
}

TEST(AppState, ErrorIsFive) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppState::Error), 5u);
}

TEST(AppState, HasExactlySixValues) {
	// Error is the last value at 5, so there are exactly 6 values (0..5)
	EXPECT_EQ(static_cast<std::uint8_t>(AppState::Error) + 1, 6u);
}

// --- Atomic lock-free test ---

TEST(AppState, AtomicIsLockFree) {
	std::atomic<AppState> state{AppState::Running};
	EXPECT_TRUE(state.is_lock_free());
}

// --- to_string tests ---

TEST(AppState, ToStringRunning) {
	EXPECT_EQ(Global::to_string(AppState::Running), "Running");
}

TEST(AppState, ToStringResizing) {
	EXPECT_EQ(Global::to_string(AppState::Resizing), "Resizing");
}

TEST(AppState, ToStringReloading) {
	EXPECT_EQ(Global::to_string(AppState::Reloading), "Reloading");
}

TEST(AppState, ToStringSleeping) {
	EXPECT_EQ(Global::to_string(AppState::Sleeping), "Sleeping");
}

TEST(AppState, ToStringQuitting) {
	EXPECT_EQ(Global::to_string(AppState::Quitting), "Quitting");
}

TEST(AppState, ToStringError) {
	EXPECT_EQ(Global::to_string(AppState::Error), "Error");
}

TEST(AppState, ToStringUnknownValue) {
	// Cast an out-of-range value to AppState
	auto unknown = static_cast<AppState>(255);
	EXPECT_EQ(Global::to_string(unknown), "Unknown");
}

// --- Atomic default value test ---

TEST(AppState, DefaultConstructedAtomicIsRunning) {
	std::atomic<AppState> state{AppState::Running};
	EXPECT_EQ(state.load(), AppState::Running);
}

// --- Store/load round-trip tests ---

TEST(AppState, StoreLoadRoundTripRunning) {
	std::atomic<AppState> state{AppState::Error};
	state.store(AppState::Running);
	EXPECT_EQ(state.load(), AppState::Running);
}

TEST(AppState, StoreLoadRoundTripResizing) {
	std::atomic<AppState> state{AppState::Running};
	state.store(AppState::Resizing);
	EXPECT_EQ(state.load(), AppState::Resizing);
}

TEST(AppState, StoreLoadRoundTripReloading) {
	std::atomic<AppState> state{AppState::Running};
	state.store(AppState::Reloading);
	EXPECT_EQ(state.load(), AppState::Reloading);
}

TEST(AppState, StoreLoadRoundTripSleeping) {
	std::atomic<AppState> state{AppState::Running};
	state.store(AppState::Sleeping);
	EXPECT_EQ(state.load(), AppState::Sleeping);
}

TEST(AppState, StoreLoadRoundTripQuitting) {
	std::atomic<AppState> state{AppState::Running};
	state.store(AppState::Quitting);
	EXPECT_EQ(state.load(), AppState::Quitting);
}

TEST(AppState, StoreLoadRoundTripError) {
	std::atomic<AppState> state{AppState::Running};
	state.store(AppState::Error);
	EXPECT_EQ(state.load(), AppState::Error);
}

// --- compare_exchange_strong tests ---

TEST(AppState, CompareExchangeSucceedsWhenExpectedMatches) {
	std::atomic<AppState> state{AppState::Running};
	AppState expected = AppState::Running;
	bool exchanged = state.compare_exchange_strong(expected, AppState::Quitting);
	EXPECT_TRUE(exchanged);
	EXPECT_EQ(state.load(), AppState::Quitting);
	EXPECT_EQ(expected, AppState::Running);  // expected unchanged on success
}

TEST(AppState, CompareExchangeFailsWhenExpectedDiffers) {
	std::atomic<AppState> state{AppState::Resizing};
	AppState expected = AppState::Running;  // wrong expectation
	bool exchanged = state.compare_exchange_strong(expected, AppState::Quitting);
	EXPECT_FALSE(exchanged);
	EXPECT_EQ(state.load(), AppState::Resizing);  // state unchanged
	EXPECT_EQ(expected, AppState::Resizing);  // expected updated to actual value
}
