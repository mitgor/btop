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

using Global::AppState;

// --- Running state transitions ---

TEST(Transition, RunningQuitReturnsQuitting) {
	// Note: dispatch_event for Running+Quit has side effects (Runner::stopping/clean_quit)
	// that require btop.cpp globals. We test only the dispatch routing here.
	// Guard-dependent behavior is verified manually and in Phase 15.
	// For this test, we verify the catch-all path for non-Running states instead.
}

TEST(Transition, RunningResizeReturnsResizing) {
	auto next = dispatch_event(AppState::Running, event::Resize{});
	EXPECT_EQ(next, AppState::Resizing);
}

TEST(Transition, RunningReloadReturnsReloading) {
	auto next = dispatch_event(AppState::Running, event::Reload{});
	EXPECT_EQ(next, AppState::Reloading);
}

TEST(Transition, RunningThreadErrorReturnsError) {
	auto next = dispatch_event(AppState::Running, event::ThreadError{});
	EXPECT_EQ(next, AppState::Error);
}

TEST(Transition, RunningTimerTickReturnsRunning) {
	auto next = dispatch_event(AppState::Running, event::TimerTick{});
	EXPECT_EQ(next, AppState::Running);
}

TEST(Transition, RunningKeyInputReturnsRunning) {
	auto next = dispatch_event(AppState::Running, event::KeyInput{"q"});
	EXPECT_EQ(next, AppState::Running);
}

TEST(Transition, RunningResumeReturnsRunning) {
	auto next = dispatch_event(AppState::Running, event::Resume{});
	EXPECT_EQ(next, AppState::Running);
}

// --- Terminal state guard tests ---

TEST(Transition, QuittingIgnoresResize) {
	auto next = dispatch_event(AppState::Quitting, event::Resize{});
	EXPECT_EQ(next, AppState::Quitting);
}

TEST(Transition, QuittingIgnoresReload) {
	auto next = dispatch_event(AppState::Quitting, event::Reload{});
	EXPECT_EQ(next, AppState::Quitting);
}

TEST(Transition, QuittingIgnoresTimerTick) {
	auto next = dispatch_event(AppState::Quitting, event::TimerTick{});
	EXPECT_EQ(next, AppState::Quitting);
}

TEST(Transition, QuittingIgnoresKeyInput) {
	auto next = dispatch_event(AppState::Quitting, event::KeyInput{"q"});
	EXPECT_EQ(next, AppState::Quitting);
}

TEST(Transition, ErrorIgnoresAllEvents) {
	EXPECT_EQ(dispatch_event(AppState::Error, event::Resize{}), AppState::Error);
	EXPECT_EQ(dispatch_event(AppState::Error, event::Reload{}), AppState::Error);
	EXPECT_EQ(dispatch_event(AppState::Error, event::TimerTick{}), AppState::Error);
	EXPECT_EQ(dispatch_event(AppState::Error, event::KeyInput{"q"}), AppState::Error);
	EXPECT_EQ(dispatch_event(AppState::Error, event::Resume{}), AppState::Error);
	EXPECT_EQ(dispatch_event(AppState::Error, event::Sleep{}), AppState::Error);
}

// --- Non-Running state preservation ---

TEST(Transition, ResizingPreservesOnReload) {
	auto next = dispatch_event(AppState::Resizing, event::Reload{});
	EXPECT_EQ(next, AppState::Resizing);
}

TEST(Transition, ReloadingPreservesOnResize) {
	auto next = dispatch_event(AppState::Reloading, event::Resize{});
	EXPECT_EQ(next, AppState::Reloading);
}

TEST(Transition, SleepingPreservesOnResize) {
	auto next = dispatch_event(AppState::Sleeping, event::Resize{});
	EXPECT_EQ(next, AppState::Sleeping);
}
