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
#include "btop_events.hpp"

// --- EventType tests ---

TEST(EventType, VariantHoldsTimerTick) {
	AppEvent ev = event::TimerTick{};
	EXPECT_TRUE(std::holds_alternative<event::TimerTick>(ev));
}

TEST(EventType, VariantHoldsResize) {
	AppEvent ev = event::Resize{};
	EXPECT_TRUE(std::holds_alternative<event::Resize>(ev));
}

TEST(EventType, VariantHoldsQuit) {
	AppEvent ev = event::Quit{0};
	EXPECT_TRUE(std::holds_alternative<event::Quit>(ev));
}

TEST(EventType, VariantHoldsSleep) {
	AppEvent ev = event::Sleep{};
	EXPECT_TRUE(std::holds_alternative<event::Sleep>(ev));
}

TEST(EventType, VariantHoldsResume) {
	AppEvent ev = event::Resume{};
	EXPECT_TRUE(std::holds_alternative<event::Resume>(ev));
}

TEST(EventType, VariantHoldsReload) {
	AppEvent ev = event::Reload{};
	EXPECT_TRUE(std::holds_alternative<event::Reload>(ev));
}

TEST(EventType, VariantHoldsThreadError) {
	AppEvent ev = event::ThreadError{};
	EXPECT_TRUE(std::holds_alternative<event::ThreadError>(ev));
}

TEST(EventType, HasExactlyEightTypes) {
	EXPECT_EQ(std::variant_size_v<AppEvent>, 8u);
}

TEST(EventType, IsTriviallyCopyable) {
	EXPECT_TRUE(std::is_trivially_copyable_v<AppEvent>);
}

TEST(EventType, QuitHasExitCode) {
	event::Quit q{42};
	EXPECT_EQ(q.exit_code, 42);
}

TEST(EventType, QuitExitCodeZero) {
	event::Quit q{0};
	EXPECT_EQ(q.exit_code, 0);
}

TEST(EventType, TimerTickDefaultConstructible) {
	event::TimerTick t{};
	(void)t;
	SUCCEED();
}

TEST(EventType, ResizeDefaultConstructible) {
	event::Resize r{};
	(void)r;
	SUCCEED();
}

TEST(EventType, SleepDefaultConstructible) {
	event::Sleep s{};
	(void)s;
	SUCCEED();
}

TEST(EventType, ResumeDefaultConstructible) {
	event::Resume r{};
	(void)r;
	SUCCEED();
}

TEST(EventType, ReloadDefaultConstructible) {
	event::Reload r{};
	(void)r;
	SUCCEED();
}

TEST(EventType, ThreadErrorDefaultConstructible) {
	event::ThreadError e{};
	(void)e;
	SUCCEED();
}

TEST(EventType, VariantHoldsKeyInput) {
	AppEvent ev = event::KeyInput{"test"};
	EXPECT_TRUE(std::holds_alternative<event::KeyInput>(ev));
}

TEST(EventType, KeyInputKey) {
	event::KeyInput ki{"hello"};
	EXPECT_EQ(ki.key(), "hello");
}

TEST(EventType, KeyInputDefaultEmpty) {
	event::KeyInput ki{};
	EXPECT_EQ(ki.key(), "");
	EXPECT_EQ(ki.key_len, 0);
}

TEST(EventType, KeyInputTruncatesLongKey) {
	std::string long_key(50, 'x');
	event::KeyInput ki{long_key};
	EXPECT_EQ(ki.key_len, 31);
	EXPECT_EQ(ki.key().size(), 31u);
}

TEST(EventType, KeyInputMouseSequence) {
	event::KeyInput ki{"[<0;123;456M"};
	EXPECT_EQ(ki.key(), "[<0;123;456M");
}

TEST(DispatchState, AllSixStatesDispatch) {
	using Global::AppState;
	auto check = [](AppState s) -> int {
		return dispatch_state(s, [](auto tag) -> int {
			if constexpr (std::is_same_v<decltype(tag), state_tag::Running>)   return 0;
			if constexpr (std::is_same_v<decltype(tag), state_tag::Resizing>)  return 1;
			if constexpr (std::is_same_v<decltype(tag), state_tag::Reloading>) return 2;
			if constexpr (std::is_same_v<decltype(tag), state_tag::Sleeping>)  return 3;
			if constexpr (std::is_same_v<decltype(tag), state_tag::Quitting>)  return 4;
			if constexpr (std::is_same_v<decltype(tag), state_tag::Error>)     return 5;
			return -1;
		});
	};
	EXPECT_EQ(check(AppState::Running), 0);
	EXPECT_EQ(check(AppState::Resizing), 1);
	EXPECT_EQ(check(AppState::Reloading), 2);
	EXPECT_EQ(check(AppState::Sleeping), 3);
	EXPECT_EQ(check(AppState::Quitting), 4);
	EXPECT_EQ(check(AppState::Error), 5);
}

// --- EventQueue tests ---

TEST(EventQueue, PushPopSingleEvent) {
	EventQueue<AppEvent, 4> q;
	EXPECT_TRUE(q.push(event::Quit{0}));
	auto ev = q.try_pop();
	ASSERT_TRUE(ev.has_value());
	EXPECT_TRUE(std::holds_alternative<event::Quit>(*ev));
	EXPECT_EQ(std::get<event::Quit>(*ev).exit_code, 0);
}

TEST(EventQueue, FIFOOrdering) {
	EventQueue<AppEvent, 8> q;
	q.push(event::Resize{});
	q.push(event::Quit{1});
	q.push(event::Sleep{});

	auto e1 = q.try_pop();
	ASSERT_TRUE(e1.has_value());
	EXPECT_TRUE(std::holds_alternative<event::Resize>(*e1));

	auto e2 = q.try_pop();
	ASSERT_TRUE(e2.has_value());
	EXPECT_TRUE(std::holds_alternative<event::Quit>(*e2));
	EXPECT_EQ(std::get<event::Quit>(*e2).exit_code, 1);

	auto e3 = q.try_pop();
	ASSERT_TRUE(e3.has_value());
	EXPECT_TRUE(std::holds_alternative<event::Sleep>(*e3));
}

TEST(EventQueue, TryPopReturnsNulloptWhenEmpty) {
	EventQueue<AppEvent, 4> q;
	auto ev = q.try_pop();
	EXPECT_FALSE(ev.has_value());
}

TEST(EventQueue, PushReturnsFalseWhenFull) {
	EventQueue<AppEvent, 4> q;
	EXPECT_TRUE(q.push(event::Resize{}));
	EXPECT_TRUE(q.push(event::Resize{}));
	EXPECT_TRUE(q.push(event::Resize{}));
	EXPECT_TRUE(q.push(event::Resize{}));
	// Queue is full (capacity 4)
	EXPECT_FALSE(q.push(event::Resize{}));
}

TEST(EventQueue, EmptyOnFreshQueue) {
	EventQueue<AppEvent, 4> q;
	EXPECT_TRUE(q.empty());
}

TEST(EventQueue, NotEmptyAfterPush) {
	EventQueue<AppEvent, 4> q;
	q.push(event::Resize{});
	EXPECT_FALSE(q.empty());
}

TEST(EventQueue, EmptyAfterDraining) {
	EventQueue<AppEvent, 4> q;
	q.push(event::Resize{});
	q.push(event::Quit{0});
	q.try_pop();
	q.try_pop();
	EXPECT_TRUE(q.empty());
}

TEST(EventQueue, LockFreeAtomics) {
	// uint32_t atomics must be always lock-free for signal safety
	EXPECT_TRUE(std::atomic<std::uint32_t>::is_always_lock_free);
}

TEST(EventQueue, FillToCapacityThenDrain) {
	constexpr std::size_t cap = 8;
	EventQueue<AppEvent, cap> q;

	// Fill to capacity with distinguishable events
	q.push(event::TimerTick{});
	q.push(event::Resize{});
	q.push(event::Quit{42});
	q.push(event::Sleep{});
	q.push(event::Resume{});
	q.push(event::Reload{});
	q.push(event::ThreadError{});
	q.push(event::Quit{7});

	// Drain all and verify order
	auto e0 = q.try_pop();
	ASSERT_TRUE(e0.has_value());
	EXPECT_TRUE(std::holds_alternative<event::TimerTick>(*e0));

	auto e1 = q.try_pop();
	ASSERT_TRUE(e1.has_value());
	EXPECT_TRUE(std::holds_alternative<event::Resize>(*e1));

	auto e2 = q.try_pop();
	ASSERT_TRUE(e2.has_value());
	EXPECT_TRUE(std::holds_alternative<event::Quit>(*e2));
	EXPECT_EQ(std::get<event::Quit>(*e2).exit_code, 42);

	auto e3 = q.try_pop();
	ASSERT_TRUE(e3.has_value());
	EXPECT_TRUE(std::holds_alternative<event::Sleep>(*e3));

	auto e4 = q.try_pop();
	ASSERT_TRUE(e4.has_value());
	EXPECT_TRUE(std::holds_alternative<event::Resume>(*e4));

	auto e5 = q.try_pop();
	ASSERT_TRUE(e5.has_value());
	EXPECT_TRUE(std::holds_alternative<event::Reload>(*e5));

	auto e6 = q.try_pop();
	ASSERT_TRUE(e6.has_value());
	EXPECT_TRUE(std::holds_alternative<event::ThreadError>(*e6));

	auto e7 = q.try_pop();
	ASSERT_TRUE(e7.has_value());
	EXPECT_TRUE(std::holds_alternative<event::Quit>(*e7));
	EXPECT_EQ(std::get<event::Quit>(*e7).exit_code, 7);

	// Queue should be empty now
	EXPECT_TRUE(q.empty());
	EXPECT_FALSE(q.try_pop().has_value());
}

TEST(EventQueue, PushAfterDrain) {
	EventQueue<AppEvent, 4> q;
	// Fill and drain
	q.push(event::Resize{});
	q.push(event::Resize{});
	q.push(event::Resize{});
	q.push(event::Resize{});
	q.try_pop();
	q.try_pop();
	q.try_pop();
	q.try_pop();

	// Should be able to push again after draining
	EXPECT_TRUE(q.push(event::Quit{0}));
	auto ev = q.try_pop();
	ASSERT_TRUE(ev.has_value());
	EXPECT_TRUE(std::holds_alternative<event::Quit>(*ev));
}
