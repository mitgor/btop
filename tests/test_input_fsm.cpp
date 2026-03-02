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

#include <string>

#include <gtest/gtest.h>
#include "btop_input.hpp"
#include "btop_config.hpp"
#include "btop_draw.hpp"

using Config::BoolKey;
using Config::StringKey;

// ========================================================================
// InputFSMTest fixture — resets Input FSM to Normal state before each test
// ========================================================================

class InputFSMTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Ensure Normal state regardless of previous test
		if (Input::is_filtering()) Input::exit_filtering(false);
		if (Input::is_menu_active()) Input::exit_menu();
		Config::set(StringKey::proc_filter, "");
		Config::set(BoolKey::proc_filtering, false);
	}

	void TearDown() override {
		// Clean up: ensure Normal state regardless of test outcome
		if (Input::is_filtering()) Input::exit_filtering(false);
		if (Input::is_menu_active()) Input::exit_menu();
		Config::set(StringKey::proc_filter, "");
		Config::set(BoolKey::proc_filtering, false);
	}
};

// --- TEST-02: Normal<->MenuActive transitions ---

TEST_F(InputFSMTest, NormalToMenuActiveToNormal) {
	EXPECT_FALSE(Input::is_menu_active());
	EXPECT_FALSE(Input::is_filtering());

	Input::enter_menu();
	EXPECT_TRUE(Input::is_menu_active());
	EXPECT_FALSE(Input::is_filtering());

	Input::exit_menu();
	EXPECT_FALSE(Input::is_menu_active());
	EXPECT_FALSE(Input::is_filtering());
}

// --- TEST-02: Normal<->Filtering transitions ---

TEST_F(InputFSMTest, NormalToFilteringToNormal_Accept) {
	Config::set(StringKey::proc_filter, "initial");

	Input::enter_filtering();
	EXPECT_TRUE(Input::is_filtering());
	EXPECT_FALSE(Input::is_menu_active());

	Input::exit_filtering(true);
	EXPECT_FALSE(Input::is_filtering());
	EXPECT_FALSE(Input::is_menu_active());
}

// --- TEST-02: Mutual exclusivity ---

TEST_F(InputFSMTest, MenuActiveAndFilteringMutuallyExclusive) {
	// Enter menu, verify not filtering
	Input::enter_menu();
	EXPECT_TRUE(Input::is_menu_active());
	EXPECT_FALSE(Input::is_filtering());

	// Exit menu, enter filtering, verify not menu active
	Input::exit_menu();
	Config::set(StringKey::proc_filter, "test");
	Input::enter_filtering();
	EXPECT_TRUE(Input::is_filtering());
	EXPECT_FALSE(Input::is_menu_active());

	Input::exit_filtering(false);
}

// --- TEST-02: Idempotent exit from Normal ---

TEST_F(InputFSMTest, ExitMenuFromNormalIsSafe) {
	EXPECT_FALSE(Input::is_menu_active());

	// Call exit_menu when already Normal — should not crash
	Input::exit_menu();
	EXPECT_FALSE(Input::is_menu_active());
	EXPECT_FALSE(Input::is_filtering());
}

TEST_F(InputFSMTest, ExitFilteringFromNormalIsSafe) {
	EXPECT_FALSE(Input::is_filtering());

	// Call exit_filtering(false) when in Normal — should not crash
	Input::exit_filtering(false);
	EXPECT_FALSE(Input::is_filtering());
	EXPECT_FALSE(Input::is_menu_active());
}

// ========================================================================
// FilteringTest fixture — pre-seeds proc_filter for filtering exit path tests
// ========================================================================

class FilteringTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Ensure Normal state
		if (Input::is_filtering()) Input::exit_filtering(false);
		if (Input::is_menu_active()) Input::exit_menu();
		// Set known starting filter value
		Config::set(StringKey::proc_filter, "initial");
		Config::set(BoolKey::proc_filtering, false);
	}

	void TearDown() override {
		// Clean up
		if (Input::is_filtering()) Input::exit_filtering(false);
		if (Input::is_menu_active()) Input::exit_menu();
		Config::set(StringKey::proc_filter, "");
		Config::set(BoolKey::proc_filtering, false);
	}
};

// --- TEST-06: Filtering exit reject restores old_filter ---

TEST_F(FilteringTest, NormalToFilteringToNormal_Reject) {
	// Config starts with "initial"
	EXPECT_EQ(Config::getS(StringKey::proc_filter), "initial");

	Input::enter_filtering();
	EXPECT_TRUE(Input::is_filtering());

	// exit_filtering(false) should restore old_filter ("initial")
	Input::exit_filtering(false);
	EXPECT_FALSE(Input::is_filtering());
	EXPECT_EQ(Config::getS(StringKey::proc_filter), "initial");
}

// --- TEST-06: Accept keeps new filter ---

TEST_F(FilteringTest, FilteringExitAccept_KeepsNewFilter) {
	// Config starts with "initial" which becomes old_filter
	Input::enter_filtering();
	EXPECT_TRUE(Input::is_filtering());

	// Simulate user typing: modify Proc::filter.text
	Proc::filter.text = "new_filter_text";

	// Accept — should save the new filter to Config
	Input::exit_filtering(true);
	EXPECT_FALSE(Input::is_filtering());
	EXPECT_EQ(Config::getS(StringKey::proc_filter), "new_filter_text");
}

// --- TEST-06: Reject restores old filter ---

TEST_F(FilteringTest, FilteringExitReject_RestoresOldFilter) {
	// Config starts with "initial"
	Input::enter_filtering();

	// Simulate user typing
	Proc::filter.text = "modified_text";

	// Reject — should restore "initial"
	Input::exit_filtering(false);
	EXPECT_FALSE(Input::is_filtering());
	EXPECT_EQ(Config::getS(StringKey::proc_filter), "initial");
}
