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
#include "btop_shared.hpp"

using Global::RunnerStateTag;

// --- Enum value tests ---

TEST(RunnerStateTag, IdleIsZero) {
	EXPECT_EQ(static_cast<std::uint8_t>(RunnerStateTag::Idle), 0u);
}

TEST(RunnerStateTag, CollectingIsOne) {
	EXPECT_EQ(static_cast<std::uint8_t>(RunnerStateTag::Collecting), 1u);
}

TEST(RunnerStateTag, DrawingIsTwo) {
	EXPECT_EQ(static_cast<std::uint8_t>(RunnerStateTag::Drawing), 2u);
}

TEST(RunnerStateTag, StoppingIsThree) {
	EXPECT_EQ(static_cast<std::uint8_t>(RunnerStateTag::Stopping), 3u);
}

TEST(RunnerStateTag, HasExactlyFourValues) {
	// Stopping is the last value at 3, so there are exactly 4 values (0..3)
	EXPECT_EQ(static_cast<std::uint8_t>(RunnerStateTag::Stopping) + 1, 4u);
}

// --- Atomic lock-free test ---

TEST(RunnerStateTag, AtomicIsLockFree) {
	std::atomic<RunnerStateTag> state{RunnerStateTag::Idle};
	EXPECT_TRUE(state.is_lock_free());
}

// --- to_string tests ---

TEST(RunnerStateTag, ToStringIdle) {
	EXPECT_EQ(Global::to_string(RunnerStateTag::Idle), "Idle");
}

TEST(RunnerStateTag, ToStringCollecting) {
	EXPECT_EQ(Global::to_string(RunnerStateTag::Collecting), "Collecting");
}

TEST(RunnerStateTag, ToStringDrawing) {
	EXPECT_EQ(Global::to_string(RunnerStateTag::Drawing), "Drawing");
}

TEST(RunnerStateTag, ToStringStopping) {
	EXPECT_EQ(Global::to_string(RunnerStateTag::Stopping), "Stopping");
}

TEST(RunnerStateTag, ToStringUnknownValue) {
	auto unknown = static_cast<RunnerStateTag>(255);
	EXPECT_EQ(Global::to_string(unknown), "Unknown");
}

// --- Store/load round-trip tests ---

TEST(RunnerStateTag, StoreLoadRoundTripIdle) {
	std::atomic<RunnerStateTag> state{RunnerStateTag::Stopping};
	state.store(RunnerStateTag::Idle);
	EXPECT_EQ(state.load(), RunnerStateTag::Idle);
}

TEST(RunnerStateTag, StoreLoadRoundTripCollecting) {
	std::atomic<RunnerStateTag> state{RunnerStateTag::Idle};
	state.store(RunnerStateTag::Collecting);
	EXPECT_EQ(state.load(), RunnerStateTag::Collecting);
}

TEST(RunnerStateTag, StoreLoadRoundTripDrawing) {
	std::atomic<RunnerStateTag> state{RunnerStateTag::Idle};
	state.store(RunnerStateTag::Drawing);
	EXPECT_EQ(state.load(), RunnerStateTag::Drawing);
}

TEST(RunnerStateTag, StoreLoadRoundTripStopping) {
	std::atomic<RunnerStateTag> state{RunnerStateTag::Idle};
	state.store(RunnerStateTag::Stopping);
	EXPECT_EQ(state.load(), RunnerStateTag::Stopping);
}

// --- Runner FSM query tests ---
// These tests use Global::runner_state_tag directly to set the state
// and verify Runner::is_active() and Runner::is_stopping() behavior.

namespace {
	/// RAII guard to save and restore Global::runner_state_tag around each test.
	struct RunnerTagGuard {
		RunnerStateTag saved;
		RunnerTagGuard() : saved(Global::runner_state_tag.load(std::memory_order_relaxed)) {}
		~RunnerTagGuard() { Global::runner_state_tag.store(saved, std::memory_order_release); }
	};
}

// --- RunnerFsmQuery: is_active() ---

TEST(RunnerFsmQuery, IsActiveReturnsTrueForCollecting) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Collecting, std::memory_order_release);
	EXPECT_TRUE(Runner::is_active());
}

TEST(RunnerFsmQuery, IsActiveReturnsTrueForDrawing) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Drawing, std::memory_order_release);
	EXPECT_TRUE(Runner::is_active());
}

TEST(RunnerFsmQuery, IsActiveReturnsFalseForIdle) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Idle, std::memory_order_release);
	EXPECT_FALSE(Runner::is_active());
}

TEST(RunnerFsmQuery, IsActiveReturnsFalseForStopping) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Stopping, std::memory_order_release);
	EXPECT_FALSE(Runner::is_active());
}

// --- RunnerFsmQuery: is_stopping() ---

TEST(RunnerFsmQuery, IsStoppingReturnsTrueForStopping) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Stopping, std::memory_order_release);
	EXPECT_TRUE(Runner::is_stopping());
}

TEST(RunnerFsmQuery, IsStoppingReturnsFalseForIdle) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Idle, std::memory_order_release);
	EXPECT_FALSE(Runner::is_stopping());
}

TEST(RunnerFsmQuery, IsStoppingReturnsFalseForCollecting) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Collecting, std::memory_order_release);
	EXPECT_FALSE(Runner::is_stopping());
}

TEST(RunnerFsmQuery, IsStoppingReturnsFalseForDrawing) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Drawing, std::memory_order_release);
	EXPECT_FALSE(Runner::is_stopping());
}

// --- RunnerFsmTransition: state tag transitions via atomic ---

TEST(RunnerFsmTransition, IdleToCollecting) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Idle, std::memory_order_release);
	Global::runner_state_tag.store(RunnerStateTag::Collecting, std::memory_order_release);
	EXPECT_EQ(Global::runner_state_tag.load(std::memory_order_acquire), RunnerStateTag::Collecting);
}

TEST(RunnerFsmTransition, CollectingToDrawing) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Collecting, std::memory_order_release);
	Global::runner_state_tag.store(RunnerStateTag::Drawing, std::memory_order_release);
	EXPECT_EQ(Global::runner_state_tag.load(std::memory_order_acquire), RunnerStateTag::Drawing);
}

TEST(RunnerFsmTransition, DrawingToIdle) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Drawing, std::memory_order_release);
	Global::runner_state_tag.store(RunnerStateTag::Idle, std::memory_order_release);
	EXPECT_EQ(Global::runner_state_tag.load(std::memory_order_acquire), RunnerStateTag::Idle);
}

TEST(RunnerFsmTransition, AnyToStopping) {
	RunnerTagGuard guard;
	Global::runner_state_tag.store(RunnerStateTag::Collecting, std::memory_order_release);
	Global::runner_state_tag.store(RunnerStateTag::Stopping, std::memory_order_release);
	EXPECT_EQ(Global::runner_state_tag.load(std::memory_order_acquire), RunnerStateTag::Stopping);
}

// --- Runner::stop() test ---
// Runner::stop() stores Stopping first, then resets to Idle after completing
// its cancellation protocol. In test context (no active runner thread),
// it safely completes and leaves the tag at Idle.

TEST(RunnerFsmTransition, StopSetsIdleAfterCancellation) {
	RunnerTagGuard guard;
	// Set app_state to Quitting to prevent clean_quit path in stop()
	auto saved_app_state = Global::app_state.load(std::memory_order_relaxed);
	Global::app_state.store(Global::AppStateTag::Quitting, std::memory_order_release);
	Global::runner_state_tag.store(RunnerStateTag::Collecting, std::memory_order_release);

	Runner::stop();

	// stop() transitions through Stopping and resets to Idle
	EXPECT_EQ(Global::runner_state_tag.load(std::memory_order_acquire), RunnerStateTag::Idle);

	// Restore app_state
	Global::app_state.store(saved_app_state, std::memory_order_release);
}
