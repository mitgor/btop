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

// --- runner:: struct constructibility tests ---

TEST(RunnerStateStructs, IdleDefaultConstructible) {
	runner::Idle s{};
	(void)s;
	SUCCEED();
}

TEST(RunnerStateStructs, CollectingDefaultConstructible) {
	runner::Collecting s{};
	(void)s;
	SUCCEED();
}

TEST(RunnerStateStructs, StoppingDefaultConstructible) {
	runner::Stopping s{};
	(void)s;
	SUCCEED();
}

TEST(RunnerStateStructs, DrawingHasForceRedraw) {
	runner::Drawing d{true};
	EXPECT_TRUE(d.force_redraw);
	runner::Drawing d2{false};
	EXPECT_FALSE(d2.force_redraw);
}

// --- Variant tests ---

TEST(RunnerStateVariant, HasExactlyFourAlternatives) {
	EXPECT_EQ(std::variant_size_v<RunnerStateVar>, 4u);
}

TEST(RunnerStateVariant, HoldsIdle) {
	RunnerStateVar v = runner::Idle{};
	EXPECT_TRUE(std::holds_alternative<runner::Idle>(v));
}

TEST(RunnerStateVariant, HoldsCollecting) {
	RunnerStateVar v = runner::Collecting{};
	EXPECT_TRUE(std::holds_alternative<runner::Collecting>(v));
}

TEST(RunnerStateVariant, HoldsDrawing) {
	RunnerStateVar v = runner::Drawing{false};
	EXPECT_TRUE(std::holds_alternative<runner::Drawing>(v));
}

TEST(RunnerStateVariant, HoldsStopping) {
	RunnerStateVar v = runner::Stopping{};
	EXPECT_TRUE(std::holds_alternative<runner::Stopping>(v));
}

// --- to_runner_tag tests ---

TEST(RunnerStateTag, ToRunnerTagIdle) {
	EXPECT_EQ(to_runner_tag(RunnerStateVar{runner::Idle{}}), RunnerStateTag::Idle);
}

TEST(RunnerStateTag, ToRunnerTagCollecting) {
	EXPECT_EQ(to_runner_tag(RunnerStateVar{runner::Collecting{}}), RunnerStateTag::Collecting);
}

TEST(RunnerStateTag, ToRunnerTagDrawing) {
	EXPECT_EQ(to_runner_tag(RunnerStateVar{runner::Drawing{true}}), RunnerStateTag::Drawing);
}

TEST(RunnerStateTag, ToRunnerTagStopping) {
	EXPECT_EQ(to_runner_tag(RunnerStateVar{runner::Stopping{}}), RunnerStateTag::Stopping);
}

// --- Mutual exclusion test ---

TEST(RunnerStateVariant, MutualExclusion) {
	RunnerStateVar v = runner::Collecting{};
	EXPECT_TRUE(std::holds_alternative<runner::Collecting>(v));
	EXPECT_FALSE(std::holds_alternative<runner::Idle>(v));
	EXPECT_FALSE(std::holds_alternative<runner::Drawing>(v));
	EXPECT_FALSE(std::holds_alternative<runner::Stopping>(v));
}
