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

#include <variant>

#include <gtest/gtest.h>
#include "btop_state.hpp"

using Global::AppStateTag;

// --- Enum value tests ---

TEST(AppStateTag, RunningIsZero) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppStateTag::Running), 0u);
}

TEST(AppStateTag, ResizingIsOne) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppStateTag::Resizing), 1u);
}

TEST(AppStateTag, ReloadingIsTwo) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppStateTag::Reloading), 2u);
}

TEST(AppStateTag, SleepingIsThree) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppStateTag::Sleeping), 3u);
}

TEST(AppStateTag, QuittingIsFour) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppStateTag::Quitting), 4u);
}

TEST(AppStateTag, ErrorIsFive) {
	EXPECT_EQ(static_cast<std::uint8_t>(AppStateTag::Error), 5u);
}

TEST(AppStateTag, HasExactlySixValues) {
	// Error is the last value at 5, so there are exactly 6 values (0..5)
	EXPECT_EQ(static_cast<std::uint8_t>(AppStateTag::Error) + 1, 6u);
}

// --- Atomic lock-free test ---

TEST(AppStateTag, AtomicIsLockFree) {
	std::atomic<AppStateTag> state{AppStateTag::Running};
	EXPECT_TRUE(state.is_lock_free());
}

// --- to_string tests ---

TEST(AppStateTag, ToStringRunning) {
	EXPECT_EQ(Global::to_string(AppStateTag::Running), "Running");
}

TEST(AppStateTag, ToStringResizing) {
	EXPECT_EQ(Global::to_string(AppStateTag::Resizing), "Resizing");
}

TEST(AppStateTag, ToStringReloading) {
	EXPECT_EQ(Global::to_string(AppStateTag::Reloading), "Reloading");
}

TEST(AppStateTag, ToStringSleeping) {
	EXPECT_EQ(Global::to_string(AppStateTag::Sleeping), "Sleeping");
}

TEST(AppStateTag, ToStringQuitting) {
	EXPECT_EQ(Global::to_string(AppStateTag::Quitting), "Quitting");
}

TEST(AppStateTag, ToStringError) {
	EXPECT_EQ(Global::to_string(AppStateTag::Error), "Error");
}

TEST(AppStateTag, ToStringUnknownValue) {
	// Cast an out-of-range value to AppStateTag
	auto unknown = static_cast<AppStateTag>(255);
	EXPECT_EQ(Global::to_string(unknown), "Unknown");
}

// --- Atomic default value test ---

TEST(AppStateTag, DefaultConstructedAtomicIsRunning) {
	std::atomic<AppStateTag> state{AppStateTag::Running};
	EXPECT_EQ(state.load(), AppStateTag::Running);
}

// --- Store/load round-trip tests ---

TEST(AppStateTag, StoreLoadRoundTripRunning) {
	std::atomic<AppStateTag> state{AppStateTag::Error};
	state.store(AppStateTag::Running);
	EXPECT_EQ(state.load(), AppStateTag::Running);
}

TEST(AppStateTag, StoreLoadRoundTripResizing) {
	std::atomic<AppStateTag> state{AppStateTag::Running};
	state.store(AppStateTag::Resizing);
	EXPECT_EQ(state.load(), AppStateTag::Resizing);
}

TEST(AppStateTag, StoreLoadRoundTripReloading) {
	std::atomic<AppStateTag> state{AppStateTag::Running};
	state.store(AppStateTag::Reloading);
	EXPECT_EQ(state.load(), AppStateTag::Reloading);
}

TEST(AppStateTag, StoreLoadRoundTripSleeping) {
	std::atomic<AppStateTag> state{AppStateTag::Running};
	state.store(AppStateTag::Sleeping);
	EXPECT_EQ(state.load(), AppStateTag::Sleeping);
}

TEST(AppStateTag, StoreLoadRoundTripQuitting) {
	std::atomic<AppStateTag> state{AppStateTag::Running};
	state.store(AppStateTag::Quitting);
	EXPECT_EQ(state.load(), AppStateTag::Quitting);
}

TEST(AppStateTag, StoreLoadRoundTripError) {
	std::atomic<AppStateTag> state{AppStateTag::Running};
	state.store(AppStateTag::Error);
	EXPECT_EQ(state.load(), AppStateTag::Error);
}

// --- compare_exchange_strong tests ---

TEST(AppStateTag, CompareExchangeSucceedsWhenExpectedMatches) {
	std::atomic<AppStateTag> state{AppStateTag::Running};
	AppStateTag expected = AppStateTag::Running;
	bool exchanged = state.compare_exchange_strong(expected, AppStateTag::Quitting);
	EXPECT_TRUE(exchanged);
	EXPECT_EQ(state.load(), AppStateTag::Quitting);
	EXPECT_EQ(expected, AppStateTag::Running);  // expected unchanged on success
}

TEST(AppStateTag, CompareExchangeFailsWhenExpectedDiffers) {
	std::atomic<AppStateTag> state{AppStateTag::Resizing};
	AppStateTag expected = AppStateTag::Running;  // wrong expectation
	bool exchanged = state.compare_exchange_strong(expected, AppStateTag::Quitting);
	EXPECT_FALSE(exchanged);
	EXPECT_EQ(state.load(), AppStateTag::Resizing);  // state unchanged
	EXPECT_EQ(expected, AppStateTag::Resizing);  // expected updated to actual value
}

// --- State struct data tests ---

TEST(StateData, RunningCarriesTimingData) {
	state::Running r{1000, 5000};
	EXPECT_EQ(r.update_ms, 1000u);
	EXPECT_EQ(r.future_time, 5000u);
}

TEST(StateData, QuittingCarriesExitCode) {
	state::Quitting q{42};
	EXPECT_EQ(q.exit_code, 42);
}

TEST(StateData, QuittingExitCodeZero) {
	state::Quitting q{0};
	EXPECT_EQ(q.exit_code, 0);
}

TEST(StateData, ErrorCarriesMessage) {
	state::Error e{"test error"};
	EXPECT_EQ(e.message, "test error");
}

TEST(StateData, ErrorEmptyMessage) {
	state::Error e{""};
	EXPECT_TRUE(e.message.empty());
}

TEST(StateData, ResizingDefaultConstructible) {
	state::Resizing r{};
	(void)r;
	SUCCEED();
}

TEST(StateData, ReloadingDefaultConstructible) {
	state::Reloading r{};
	(void)r;
	SUCCEED();
}

TEST(StateData, SleepingDefaultConstructible) {
	state::Sleeping s{};
	(void)s;
	SUCCEED();
}

// --- Variant tests ---

TEST(StateVariant, HasExactlySixAlternatives) {
	EXPECT_EQ(std::variant_size_v<AppStateVar>, 6u);
}

TEST(StateVariant, HoldsRunning) {
	AppStateVar v = state::Running{500, 1000};
	EXPECT_TRUE(std::holds_alternative<state::Running>(v));
}

TEST(StateVariant, HoldsResizing) {
	AppStateVar v = state::Resizing{};
	EXPECT_TRUE(std::holds_alternative<state::Resizing>(v));
}

TEST(StateVariant, HoldsReloading) {
	AppStateVar v = state::Reloading{};
	EXPECT_TRUE(std::holds_alternative<state::Reloading>(v));
}

TEST(StateVariant, HoldsSleeping) {
	AppStateVar v = state::Sleeping{};
	EXPECT_TRUE(std::holds_alternative<state::Sleeping>(v));
}

TEST(StateVariant, HoldsQuitting) {
	AppStateVar v = state::Quitting{1};
	EXPECT_TRUE(std::holds_alternative<state::Quitting>(v));
}

TEST(StateVariant, HoldsError) {
	AppStateVar v = state::Error{"bad"};
	EXPECT_TRUE(std::holds_alternative<state::Error>(v));
}

TEST(StateVariant, GetRunningData) {
	AppStateVar v = state::Running{2000, 9999};
	auto& r = std::get<state::Running>(v);
	EXPECT_EQ(r.update_ms, 2000u);
	EXPECT_EQ(r.future_time, 9999u);
}

TEST(StateVariant, GetQuittingData) {
	AppStateVar v = state::Quitting{7};
	EXPECT_EQ(std::get<state::Quitting>(v).exit_code, 7);
}

TEST(StateVariant, GetErrorData) {
	AppStateVar v = state::Error{"oops"};
	EXPECT_EQ(std::get<state::Error>(v).message, "oops");
}

TEST(StateVariant, MutualExclusion) {
	// When holding Running, cannot hold Quitting
	AppStateVar v = state::Running{100, 200};
	EXPECT_TRUE(std::holds_alternative<state::Running>(v));
	EXPECT_FALSE(std::holds_alternative<state::Quitting>(v));
	EXPECT_FALSE(std::holds_alternative<state::Error>(v));
	EXPECT_FALSE(std::holds_alternative<state::Resizing>(v));
}

// --- to_tag tests ---

TEST(StateTag, RunningMapsCorrectly) {
	EXPECT_EQ(to_tag(AppStateVar{state::Running{0, 0}}), Global::AppStateTag::Running);
}

TEST(StateTag, ResizingMapsCorrectly) {
	EXPECT_EQ(to_tag(AppStateVar{state::Resizing{}}), Global::AppStateTag::Resizing);
}

TEST(StateTag, ReloadingMapsCorrectly) {
	EXPECT_EQ(to_tag(AppStateVar{state::Reloading{}}), Global::AppStateTag::Reloading);
}

TEST(StateTag, SleepingMapsCorrectly) {
	EXPECT_EQ(to_tag(AppStateVar{state::Sleeping{}}), Global::AppStateTag::Sleeping);
}

TEST(StateTag, QuittingMapsCorrectly) {
	EXPECT_EQ(to_tag(AppStateVar{state::Quitting{0}}), Global::AppStateTag::Quitting);
}

TEST(StateTag, ErrorMapsCorrectly) {
	EXPECT_EQ(to_tag(AppStateVar{state::Error{"err"}}), Global::AppStateTag::Error);
}

TEST(StateTag, RoundTripWithToString) {
	EXPECT_EQ(Global::to_string(to_tag(AppStateVar{state::Running{0, 0}})), "Running");
	EXPECT_EQ(Global::to_string(to_tag(AppStateVar{state::Resizing{}})), "Resizing");
	EXPECT_EQ(Global::to_string(to_tag(AppStateVar{state::Quitting{1}})), "Quitting");
	EXPECT_EQ(Global::to_string(to_tag(AppStateVar{state::Error{"x"}})), "Error");
}
