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

// --- Running state transitions (variant-returning) ---

TEST(TypedTransition, RunningQuitReturnsQuitting) {
	AppStateVar current = state::Running{1000, 5000};
	auto next = dispatch_event(current, event::Quit{42});
	ASSERT_TRUE(std::holds_alternative<state::Quitting>(next));
	EXPECT_EQ(std::get<state::Quitting>(next).exit_code, 42);
}

TEST(TypedTransition, RunningSleepReturnsSleeping) {
	AppStateVar current = state::Running{1000, 5000};
	auto next = dispatch_event(current, event::Sleep{});
	EXPECT_TRUE(std::holds_alternative<state::Sleeping>(next));
}

TEST(TypedTransition, RunningResizeReturnsResizing) {
	AppStateVar current = state::Running{1000, 5000};
	auto next = dispatch_event(current, event::Resize{});
	EXPECT_TRUE(std::holds_alternative<state::Resizing>(next));
}

TEST(TypedTransition, RunningReloadReturnsReloading) {
	AppStateVar current = state::Running{1000, 5000};
	auto next = dispatch_event(current, event::Reload{});
	EXPECT_TRUE(std::holds_alternative<state::Reloading>(next));
}

TEST(TypedTransition, RunningThreadErrorReturnsError) {
	AppStateVar current = state::Running{1000, 5000};
	auto next = dispatch_event(current, event::ThreadError{});
	ASSERT_TRUE(std::holds_alternative<state::Error>(next));
	EXPECT_EQ(std::get<state::Error>(next).message, "Thread error");
}

TEST(TypedTransition, RunningTimerTickReturnsRunning) {
	AppStateVar current = state::Running{1000, 5000};
	auto next = dispatch_event(current, event::TimerTick{});
	ASSERT_TRUE(std::holds_alternative<state::Running>(next));
	// Data preserved (copy of current)
	EXPECT_EQ(std::get<state::Running>(next).update_ms, 1000u);
	EXPECT_EQ(std::get<state::Running>(next).future_time, 5000u);
}

TEST(TypedTransition, RunningKeyInputReturnsRunning) {
	AppStateVar current = state::Running{2000, 9999};
	auto next = dispatch_event(current, event::KeyInput{"q"});
	ASSERT_TRUE(std::holds_alternative<state::Running>(next));
	EXPECT_EQ(std::get<state::Running>(next).update_ms, 2000u);
}

TEST(TypedTransition, RunningResumeReturnsRunning) {
	AppStateVar current = state::Running{1000, 5000};
	auto next = dispatch_event(current, event::Resume{});
	EXPECT_TRUE(std::holds_alternative<state::Running>(next));
}

TEST(TypedTransition, QuitExitCodePreserved) {
	AppStateVar current = state::Running{1000, 5000};
	auto next = dispatch_event(current, event::Quit{0});
	ASSERT_TRUE(std::holds_alternative<state::Quitting>(next));
	EXPECT_EQ(std::get<state::Quitting>(next).exit_code, 0);
}

// --- Terminal state guard tests ---

TEST(TypedTransition, QuittingIgnoresResize) {
	AppStateVar current = state::Quitting{0};
	auto next = dispatch_event(current, event::Resize{});
	EXPECT_TRUE(std::holds_alternative<state::Quitting>(next));
}

TEST(TypedTransition, QuittingIgnoresReload) {
	AppStateVar current = state::Quitting{0};
	auto next = dispatch_event(current, event::Reload{});
	EXPECT_TRUE(std::holds_alternative<state::Quitting>(next));
}

TEST(TypedTransition, QuittingIgnoresTimerTick) {
	AppStateVar current = state::Quitting{0};
	auto next = dispatch_event(current, event::TimerTick{});
	EXPECT_TRUE(std::holds_alternative<state::Quitting>(next));
}

TEST(TypedTransition, QuittingIgnoresKeyInput) {
	AppStateVar current = state::Quitting{0};
	auto next = dispatch_event(current, event::KeyInput{"q"});
	EXPECT_TRUE(std::holds_alternative<state::Quitting>(next));
}

TEST(TypedTransition, QuittingPreservesExitCode) {
	AppStateVar current = state::Quitting{99};
	auto next = dispatch_event(current, event::Resize{});
	ASSERT_TRUE(std::holds_alternative<state::Quitting>(next));
	EXPECT_EQ(std::get<state::Quitting>(next).exit_code, 99);
}

TEST(TypedTransition, ErrorIgnoresAllEvents) {
	AppStateVar current = state::Error{"fatal"};
	EXPECT_TRUE(std::holds_alternative<state::Error>(dispatch_event(current, event::Resize{})));
	EXPECT_TRUE(std::holds_alternative<state::Error>(dispatch_event(current, event::Reload{})));
	EXPECT_TRUE(std::holds_alternative<state::Error>(dispatch_event(current, event::TimerTick{})));
	EXPECT_TRUE(std::holds_alternative<state::Error>(dispatch_event(current, event::KeyInput{"q"})));
	EXPECT_TRUE(std::holds_alternative<state::Error>(dispatch_event(current, event::Resume{})));
	EXPECT_TRUE(std::holds_alternative<state::Error>(dispatch_event(current, event::Sleep{})));
}

TEST(TypedTransition, ErrorPreservesMessage) {
	AppStateVar current = state::Error{"original error"};
	auto next = dispatch_event(current, event::Resize{});
	ASSERT_TRUE(std::holds_alternative<state::Error>(next));
	EXPECT_EQ(std::get<state::Error>(next).message, "original error");
}

// --- Non-Running state preservation ---

TEST(TypedTransition, ResizingPreservesOnReload) {
	AppStateVar current = state::Resizing{};
	auto next = dispatch_event(current, event::Reload{});
	EXPECT_TRUE(std::holds_alternative<state::Resizing>(next));
}

TEST(TypedTransition, ReloadingPreservesOnResize) {
	AppStateVar current = state::Reloading{};
	auto next = dispatch_event(current, event::Resize{});
	EXPECT_TRUE(std::holds_alternative<state::Reloading>(next));
}

TEST(TypedTransition, SleepingPreservesOnResize) {
	AppStateVar current = state::Sleeping{};
	auto next = dispatch_event(current, event::Resize{});
	EXPECT_TRUE(std::holds_alternative<state::Sleeping>(next));
}

// --- Two-variant dispatch tests ---

TEST(TypedDispatch, AllSixStatesRouteCorrectly) {
	// Each state + TimerTick -> same state (catch-all)
	auto check = [](const AppStateVar& v) {
		auto next = dispatch_event(v, event::TimerTick{});
		return to_tag(next) == to_tag(v);
	};
	EXPECT_TRUE(check(state::Running{100, 200}));
	EXPECT_TRUE(check(state::Resizing{}));
	EXPECT_TRUE(check(state::Reloading{}));
	EXPECT_TRUE(check(state::Sleeping{}));
	EXPECT_TRUE(check(state::Quitting{0}));
	EXPECT_TRUE(check(state::Error{"err"}));
}

TEST(TypedDispatch, VariantDispatchPreservesData) {
	AppStateVar current = state::Running{3000, 7777};
	auto next = dispatch_event(current, event::TimerTick{});
	ASSERT_TRUE(std::holds_alternative<state::Running>(next));
	auto& r = std::get<state::Running>(next);
	EXPECT_EQ(r.update_ms, 3000u);
	EXPECT_EQ(r.future_time, 7777u);
}

// --- Entry/exit action tests (TRANS-03) ---
//
// These tests verify the behavioral guarantee that on_enter() and on_exit()
// fire during transition_to() on state-type changes, and do NOT fire on
// same-type self-transitions. Uses a minimal local transition_to() variant
// with callable on_enter/on_exit parameters to avoid dependency on btop globals.

namespace {

/// Minimal transition_to() accepting callable on_enter/on_exit for testing.
/// Mirrors the real transition_to() logic: compares tags, fires exit then entry
/// on state-type change, skips both on same-type self-transition.
template<typename OnExit, typename OnEnter>
void test_transition_to(AppStateVar& current, AppStateVar next,
                        OnExit&& exit_fn, OnEnter&& enter_fn) {
	if (to_tag(current) == to_tag(next)) {
		// Same state type — update data, no entry/exit actions
		current = std::move(next);
		return;
	}

	// Exit action
	std::visit([&](const auto& old_st, const auto& new_st) {
		exit_fn(old_st, new_st);
	}, current, next);

	// Update shadow enum
	Global::app_state.store(to_tag(next), std::memory_order_release);

	// Move new state
	current = std::move(next);

	// Entry action
	std::visit([&](auto& new_st) {
		enter_fn(new_st);
	}, current);
}

} // anonymous namespace

TEST(EntryExit, EntryFiresOnStateTypeChange) {
	AppStateVar state = state::Running{1000, 5000};
	Global::app_state.store(Global::AppStateTag::Running, std::memory_order_release);

	bool enter_fired = false;
	test_transition_to(state, state::Quitting{0},
		[](const auto&, const auto&) {},  // no-op exit
		[&](auto&) { enter_fired = true; }
	);
	EXPECT_TRUE(enter_fired);
}

TEST(EntryExit, ExitFiresOnStateTypeChange) {
	AppStateVar state = state::Running{1000, 5000};
	Global::app_state.store(Global::AppStateTag::Running, std::memory_order_release);

	bool exit_fired = false;
	test_transition_to(state, state::Quitting{0},
		[&](const auto&, const auto&) { exit_fired = true; },
		[](auto&) {}  // no-op enter
	);
	EXPECT_TRUE(exit_fired);
}

TEST(EntryExit, NoEntryOnSameTypeSelfTransition) {
	AppStateVar state = state::Running{1000, 5000};
	Global::app_state.store(Global::AppStateTag::Running, std::memory_order_release);

	bool enter_fired = false;
	test_transition_to(state, state::Running{2000, 9999},
		[](const auto&, const auto&) {},
		[&](auto&) { enter_fired = true; }
	);
	EXPECT_FALSE(enter_fired);
	// Data still updated
	ASSERT_TRUE(std::holds_alternative<state::Running>(state));
	EXPECT_EQ(std::get<state::Running>(state).update_ms, 2000u);
}

TEST(EntryExit, NoExitOnSameTypeSelfTransition) {
	AppStateVar state = state::Running{1000, 5000};
	Global::app_state.store(Global::AppStateTag::Running, std::memory_order_release);

	bool exit_fired = false;
	test_transition_to(state, state::Running{2000, 9999},
		[&](const auto&, const auto&) { exit_fired = true; },
		[](auto&) {}
	);
	EXPECT_FALSE(exit_fired);
}

TEST(EntryExit, ShadowEnumUpdatedOnStateTypeChange) {
	AppStateVar state = state::Running{1000, 5000};
	Global::app_state.store(Global::AppStateTag::Running, std::memory_order_release);

	test_transition_to(state, state::Resizing{},
		[](const auto&, const auto&) {},
		[](auto&) {}
	);
	EXPECT_EQ(Global::app_state.load(std::memory_order_acquire), Global::AppStateTag::Resizing);
}

TEST(EntryExit, ShadowEnumUnchangedOnSelfTransition) {
	AppStateVar state = state::Running{1000, 5000};
	Global::app_state.store(Global::AppStateTag::Running, std::memory_order_release);

	test_transition_to(state, state::Running{2000, 9999},
		[](const auto&, const auto&) {},
		[](auto&) {}
	);
	EXPECT_EQ(Global::app_state.load(std::memory_order_acquire), Global::AppStateTag::Running);
}

TEST(EntryExit, ExitReceivesCorrectOldAndNewState) {
	AppStateVar state = state::Running{1000, 5000};
	Global::app_state.store(Global::AppStateTag::Running, std::memory_order_release);

	bool was_running_to_quitting = false;
	test_transition_to(state, state::Quitting{42},
		[&](const auto& old_st, const auto& new_st) {
			using OldT = std::decay_t<decltype(old_st)>;
			using NewT = std::decay_t<decltype(new_st)>;
			if constexpr (std::is_same_v<OldT, state::Running> &&
			              std::is_same_v<NewT, state::Quitting>) {
				was_running_to_quitting = true;
			}
		},
		[](auto&) {}
	);
	EXPECT_TRUE(was_running_to_quitting);
}

TEST(EntryExit, EntryReceivesCorrectNewState) {
	AppStateVar state = state::Running{1000, 5000};
	Global::app_state.store(Global::AppStateTag::Running, std::memory_order_release);

	int captured_exit_code = -1;
	test_transition_to(state, state::Quitting{77},
		[](const auto&, const auto&) {},
		[&](auto& new_st) {
			using T = std::decay_t<decltype(new_st)>;
			if constexpr (std::is_same_v<T, state::Quitting>) {
				captured_exit_code = new_st.exit_code;
			}
		}
	);
	EXPECT_EQ(captured_exit_code, 77);
}
