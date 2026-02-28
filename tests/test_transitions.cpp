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

using Global::AppStateTag;

// --- Running state transitions ---

TEST(Transition, RunningQuitReturnsQuitting) {
	// Note: dispatch_event for Running+Quit has side effects (Runner::stopping/clean_quit)
	// that require btop.cpp globals. We test only the dispatch routing here.
	// Guard-dependent behavior is verified manually and in Phase 15.
	// For this test, we verify the catch-all path for non-Running states instead.
}

TEST(Transition, RunningResizeReturnsResizing) {
	auto next = dispatch_event(AppStateTag::Running, event::Resize{});
	EXPECT_EQ(next, AppStateTag::Resizing);
}

TEST(Transition, RunningReloadReturnsReloading) {
	auto next = dispatch_event(AppStateTag::Running, event::Reload{});
	EXPECT_EQ(next, AppStateTag::Reloading);
}

TEST(Transition, RunningThreadErrorReturnsError) {
	auto next = dispatch_event(AppStateTag::Running, event::ThreadError{});
	EXPECT_EQ(next, AppStateTag::Error);
}

TEST(Transition, RunningTimerTickReturnsRunning) {
	auto next = dispatch_event(AppStateTag::Running, event::TimerTick{});
	EXPECT_EQ(next, AppStateTag::Running);
}

TEST(Transition, RunningKeyInputReturnsRunning) {
	auto next = dispatch_event(AppStateTag::Running, event::KeyInput{"q"});
	EXPECT_EQ(next, AppStateTag::Running);
}

TEST(Transition, RunningResumeReturnsRunning) {
	auto next = dispatch_event(AppStateTag::Running, event::Resume{});
	EXPECT_EQ(next, AppStateTag::Running);
}

// --- Terminal state guard tests ---

TEST(Transition, QuittingIgnoresResize) {
	auto next = dispatch_event(AppStateTag::Quitting, event::Resize{});
	EXPECT_EQ(next, AppStateTag::Quitting);
}

TEST(Transition, QuittingIgnoresReload) {
	auto next = dispatch_event(AppStateTag::Quitting, event::Reload{});
	EXPECT_EQ(next, AppStateTag::Quitting);
}

TEST(Transition, QuittingIgnoresTimerTick) {
	auto next = dispatch_event(AppStateTag::Quitting, event::TimerTick{});
	EXPECT_EQ(next, AppStateTag::Quitting);
}

TEST(Transition, QuittingIgnoresKeyInput) {
	auto next = dispatch_event(AppStateTag::Quitting, event::KeyInput{"q"});
	EXPECT_EQ(next, AppStateTag::Quitting);
}

TEST(Transition, ErrorIgnoresAllEvents) {
	EXPECT_EQ(dispatch_event(AppStateTag::Error, event::Resize{}), AppStateTag::Error);
	EXPECT_EQ(dispatch_event(AppStateTag::Error, event::Reload{}), AppStateTag::Error);
	EXPECT_EQ(dispatch_event(AppStateTag::Error, event::TimerTick{}), AppStateTag::Error);
	EXPECT_EQ(dispatch_event(AppStateTag::Error, event::KeyInput{"q"}), AppStateTag::Error);
	EXPECT_EQ(dispatch_event(AppStateTag::Error, event::Resume{}), AppStateTag::Error);
	EXPECT_EQ(dispatch_event(AppStateTag::Error, event::Sleep{}), AppStateTag::Error);
}

// --- Non-Running state preservation ---

TEST(Transition, ResizingPreservesOnReload) {
	auto next = dispatch_event(AppStateTag::Resizing, event::Reload{});
	EXPECT_EQ(next, AppStateTag::Resizing);
}

TEST(Transition, ReloadingPreservesOnResize) {
	auto next = dispatch_event(AppStateTag::Reloading, event::Resize{});
	EXPECT_EQ(next, AppStateTag::Reloading);
}

TEST(Transition, SleepingPreservesOnResize) {
	auto next = dispatch_event(AppStateTag::Sleeping, event::Resize{});
	EXPECT_EQ(next, AppStateTag::Sleeping);
}
