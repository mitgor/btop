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
#include <variant>

#include <gtest/gtest.h>
#include "btop_menu_pda.hpp"

// ========================================================================
// PDAInvariant tests — behavioral invariants for the Menu PDA
// Covers: TEST-01 (frame isolation, reopen-fresh), TEST-03 (SizeError override),
//         TEST-04 (SignalSend->SignalReturn), TEST-05 (resize invalidation)
// ========================================================================

// --- TEST-01: Frame isolation ---

TEST(PDAInvariant, FrameIsolation_OptionsDoesNotBleedIntoHelp) {
	menu::MenuPDA pda;
	menu::OptionsFrame of;
	of.page = 5;
	of.selected = 3;
	of.editing = true;
	pda.push(of);

	// Push Help on top
	pda.push(menu::HelpFrame{});

	// Help frame has its own defaults — no bleed from Options
	auto& help = std::get<menu::HelpFrame>(pda.top());
	EXPECT_EQ(help.page, 0);
	EXPECT_EQ(help.pages, 0);
	EXPECT_EQ(help.x, 0);
	EXPECT_EQ(help.y, 0);
	EXPECT_EQ(help.height, 0);

	// Pop Help, Options data intact
	pda.pop();
	auto& opts = std::get<menu::OptionsFrame>(pda.top());
	EXPECT_EQ(opts.page, 5);
	EXPECT_EQ(opts.selected, 3);
	EXPECT_TRUE(opts.editing);
}

TEST(PDAInvariant, FrameIsolation_MainDoesNotBleedIntoOptions) {
	menu::MenuPDA pda;
	menu::MainFrame mf;
	mf.selected = 7;
	pda.push(mf);

	menu::OptionsFrame of;
	of.page = 2;
	pda.push(of);

	// Options has its own state
	auto& opts = std::get<menu::OptionsFrame>(pda.top());
	EXPECT_EQ(opts.page, 2);
	EXPECT_EQ(opts.selected, 0);  // Default, not 7

	// Pop Options, Main data intact
	pda.pop();
	auto& main = std::get<menu::MainFrame>(pda.top());
	EXPECT_EQ(main.selected, 7);
}

// --- TEST-01: Reopen produces fresh state ---

TEST(PDAInvariant, ReopenProducesFreshState) {
	menu::MenuPDA pda;
	menu::MainFrame mf;
	mf.selected = 7;
	pda.push(mf);

	// Close menu
	pda.pop();
	EXPECT_TRUE(pda.empty());

	// Reopen — should be fresh default state
	pda.push(menu::MainFrame{});
	auto& fresh = std::get<menu::MainFrame>(pda.top());
	EXPECT_EQ(fresh.selected, 0);  // Default, not 7
	EXPECT_EQ(fresh.y, 0);
	EXPECT_TRUE(fresh.mouse_mappings.empty());
}

// --- TEST-01: Push/Pop/Replace chain ---

TEST(PDAInvariant, PushPopReplaceChain) {
	menu::MenuPDA pda;

	// Push Main
	pda.push(menu::MainFrame{});
	EXPECT_EQ(pda.depth(), 1u);
	EXPECT_TRUE(std::holds_alternative<menu::MainFrame>(pda.top()));

	// Replace with Options (depth stays 1)
	pda.replace(menu::OptionsFrame{});
	EXPECT_EQ(pda.depth(), 1u);
	EXPECT_TRUE(std::holds_alternative<menu::OptionsFrame>(pda.top()));

	// Push Help (depth 2)
	pda.push(menu::HelpFrame{});
	EXPECT_EQ(pda.depth(), 2u);
	EXPECT_TRUE(std::holds_alternative<menu::HelpFrame>(pda.top()));

	// Pop Help (back to Options)
	pda.pop();
	EXPECT_EQ(pda.depth(), 1u);
	EXPECT_TRUE(std::holds_alternative<menu::OptionsFrame>(pda.top()));

	// Pop Options (empty)
	pda.pop();
	EXPECT_TRUE(pda.empty());
}

// --- TEST-03: SizeError override ---

TEST(PDAInvariant, SizeErrorOverrideExistingMenu) {
	menu::MenuPDA pda;

	// User opens Main menu
	pda.push(menu::MainFrame{});
	EXPECT_EQ(pda.depth(), 1u);

	// SizeError pushes over existing menu
	pda.push(menu::SizeErrorFrame{});
	EXPECT_EQ(pda.depth(), 2u);
	EXPECT_TRUE(std::holds_alternative<menu::SizeErrorFrame>(pda.top()));

	// Pop SizeError returns to Main
	pda.pop();
	EXPECT_EQ(pda.depth(), 1u);
	EXPECT_TRUE(std::holds_alternative<menu::MainFrame>(pda.top()));
}

TEST(PDAInvariant, SizeErrorOverrideOptionsMenu) {
	menu::MenuPDA pda;

	// User opens Options with specific interaction state
	menu::OptionsFrame of;
	of.page = 3;
	of.selected = 5;
	of.editing = true;
	pda.push(of);

	// SizeError pushes over Options
	pda.push(menu::SizeErrorFrame{});
	EXPECT_EQ(pda.depth(), 2u);
	EXPECT_TRUE(std::holds_alternative<menu::SizeErrorFrame>(pda.top()));

	// Pop SizeError, Options interaction state preserved
	pda.pop();
	auto& opts = std::get<menu::OptionsFrame>(pda.top());
	EXPECT_EQ(opts.page, 3);
	EXPECT_EQ(opts.selected, 5);
	EXPECT_TRUE(opts.editing);
}

// --- TEST-04: SignalSend->SignalReturn sequence ---

TEST(PDAInvariant, SignalSendToSignalReturnSequence) {
	menu::MenuPDA pda;

	// Step 1: User opens SignalChoose from proc box
	pda.push(menu::SignalChooseFrame{});
	EXPECT_EQ(pda.depth(), 1u);

	// Step 2: SignalChoose pushes SignalSend
	pda.push(menu::SignalSendFrame{});
	EXPECT_EQ(pda.depth(), 2u);

	// Step 3: SignalSend completes, pops itself
	pda.pop();
	EXPECT_EQ(pda.depth(), 1u);
	EXPECT_TRUE(std::holds_alternative<menu::SignalChooseFrame>(pda.top()));

	// Step 4: Dispatch loop pushes SignalReturn (post-pop push pattern)
	pda.push(menu::SignalReturnFrame{});
	EXPECT_EQ(pda.depth(), 2u);
	EXPECT_TRUE(std::holds_alternative<menu::SignalReturnFrame>(pda.top()));

	// Step 5: SignalReturn pops
	pda.pop();
	EXPECT_EQ(pda.depth(), 1u);

	// Step 6: SignalChoose pops (user dismisses)
	pda.pop();
	EXPECT_TRUE(pda.empty());
}

// --- TEST-05: Resize invalidation ---

TEST(PDAInvariant, ResizePreservesInteractionInvalidatesLayout_Options) {
	menu::MenuPDA pda;

	// Push Options with all fields populated
	menu::OptionsFrame of;
	of.x = 100;
	of.y = 200;
	of.height = 50;
	of.page = 3;
	of.selected = 7;
	of.editing = true;
	of.mouse_mappings["btn"] = Input::Mouse_loc{1, 2, 3, 4};
	pda.push(of);

	// Simulate resize: invalidate layout via PDA
	pda.invalidate_layout();

	auto& frame = std::get<menu::OptionsFrame>(pda.top());
	// Layout fields zeroed
	EXPECT_EQ(frame.x, 0);
	EXPECT_EQ(frame.y, 0);
	EXPECT_EQ(frame.height, 0);
	EXPECT_TRUE(frame.mouse_mappings.empty());
	// Interaction fields preserved
	EXPECT_EQ(frame.page, 3);
	EXPECT_EQ(frame.selected, 7);
	EXPECT_TRUE(frame.editing);
}

TEST(PDAInvariant, ResizeMultiFrameStack) {
	menu::MenuPDA pda;

	// Push Main with layout and interaction set
	menu::MainFrame mf;
	mf.y = 50;
	mf.selected = 3;
	mf.mouse_mappings["main_btn"] = Input::Mouse_loc{10, 20, 30, 40};
	pda.push(mf);

	// Push Options on top with layout and interaction set
	menu::OptionsFrame of;
	of.x = 10;
	of.y = 20;
	of.height = 30;
	of.page = 2;
	of.selected = 4;
	of.mouse_mappings["opt_btn"] = Input::Mouse_loc{5, 6, 7, 8};
	pda.push(of);

	// PDA invalidate_layout only affects top frame
	pda.invalidate_layout();

	// Top frame (Options) layout zeroed
	auto& top_opts = std::get<menu::OptionsFrame>(pda.top());
	EXPECT_EQ(top_opts.x, 0);
	EXPECT_EQ(top_opts.y, 0);
	EXPECT_EQ(top_opts.height, 0);
	EXPECT_TRUE(top_opts.mouse_mappings.empty());
	// Top frame interaction preserved
	EXPECT_EQ(top_opts.page, 2);
	EXPECT_EQ(top_opts.selected, 4);

	// Pop Options, verify Main layout is UNCHANGED (invalidate only hit top)
	pda.pop();
	auto& main = std::get<menu::MainFrame>(pda.top());
	EXPECT_EQ(main.y, 50);
	EXPECT_EQ(main.selected, 3);
	EXPECT_EQ(main.mouse_mappings.size(), 1u);
	EXPECT_EQ(main.mouse_mappings["main_btn"].line, 10);
}

TEST(PDAInvariant, InvalidateLayoutPreservesAllInteractionFields) {
	// Test each frame type with interaction fields set, then invalidate via PDA

	// MainFrame
	{
		menu::MenuPDA pda;
		menu::MainFrame f;
		f.y = 99;
		f.selected = 5;
		f.colors_selected = {"red", "blue"};
		f.colors_normal = {"green"};
		f.mouse_mappings["x"] = Input::Mouse_loc{1, 1, 1, 1};
		pda.push(f);
		pda.invalidate_layout();
		auto& r = std::get<menu::MainFrame>(pda.top());
		EXPECT_EQ(r.y, 0);
		EXPECT_TRUE(r.mouse_mappings.empty());
		EXPECT_EQ(r.selected, 5);
		EXPECT_EQ(r.colors_selected.size(), 2u);
		EXPECT_EQ(r.colors_normal.size(), 1u);
	}

	// OptionsFrame
	{
		menu::MenuPDA pda;
		menu::OptionsFrame f;
		f.x = 10; f.y = 20; f.height = 30;
		f.page = 1; f.pages = 3; f.selected = 2; f.select_max = 8;
		f.item_height = 5; f.selected_cat = 2; f.max_items = 10;
		f.last_sel = 1; f.editing = true; f.warnings = "w";
		f.selPred.set(5);
		f.mouse_mappings["x"] = Input::Mouse_loc{1, 1, 1, 1};
		pda.push(f);
		pda.invalidate_layout();
		auto& r = std::get<menu::OptionsFrame>(pda.top());
		EXPECT_EQ(r.x, 0); EXPECT_EQ(r.y, 0); EXPECT_EQ(r.height, 0);
		EXPECT_TRUE(r.mouse_mappings.empty());
		EXPECT_EQ(r.page, 1); EXPECT_EQ(r.pages, 3);
		EXPECT_EQ(r.selected, 2); EXPECT_EQ(r.select_max, 8);
		EXPECT_EQ(r.item_height, 5); EXPECT_EQ(r.selected_cat, 2);
		EXPECT_EQ(r.max_items, 10); EXPECT_EQ(r.last_sel, 1);
		EXPECT_TRUE(r.editing); EXPECT_EQ(r.warnings, "w");
		EXPECT_TRUE(r.selPred.test(5));
	}

	// HelpFrame
	{
		menu::MenuPDA pda;
		menu::HelpFrame f;
		f.x = 5; f.y = 10; f.height = 15;
		f.page = 2; f.pages = 4;
		f.mouse_mappings["x"] = Input::Mouse_loc{1, 1, 1, 1};
		pda.push(f);
		pda.invalidate_layout();
		auto& r = std::get<menu::HelpFrame>(pda.top());
		EXPECT_EQ(r.x, 0); EXPECT_EQ(r.y, 0); EXPECT_EQ(r.height, 0);
		EXPECT_TRUE(r.mouse_mappings.empty());
		EXPECT_EQ(r.page, 2); EXPECT_EQ(r.pages, 4);
	}

	// SignalChooseFrame
	{
		menu::MenuPDA pda;
		menu::SignalChooseFrame f;
		f.x = 30; f.y = 40;
		f.selected_signal = 15;
		f.mouse_mappings["x"] = Input::Mouse_loc{1, 1, 1, 1};
		pda.push(f);
		pda.invalidate_layout();
		auto& r = std::get<menu::SignalChooseFrame>(pda.top());
		EXPECT_EQ(r.x, 0); EXPECT_EQ(r.y, 0);
		EXPECT_TRUE(r.mouse_mappings.empty());
		EXPECT_EQ(r.selected_signal, 15);
	}

	// ReniceFrame
	{
		menu::MenuPDA pda;
		menu::ReniceFrame f;
		f.x = 50; f.y = 60;
		f.selected_nice = 10;
		f.nice_edit = "editing";
		f.mouse_mappings["x"] = Input::Mouse_loc{1, 1, 1, 1};
		pda.push(f);
		pda.invalidate_layout();
		auto& r = std::get<menu::ReniceFrame>(pda.top());
		EXPECT_EQ(r.x, 0); EXPECT_EQ(r.y, 0);
		EXPECT_TRUE(r.mouse_mappings.empty());
		EXPECT_EQ(r.selected_nice, 10);
		EXPECT_EQ(r.nice_edit, "editing");
	}

	// SizeErrorFrame (no layout or interaction beyond mouse_mappings)
	{
		menu::MenuPDA pda;
		menu::SizeErrorFrame f;
		f.mouse_mappings["x"] = Input::Mouse_loc{1, 1, 1, 1};
		pda.push(f);
		pda.invalidate_layout();
		auto& r = std::get<menu::SizeErrorFrame>(pda.top());
		EXPECT_TRUE(r.mouse_mappings.empty());
	}

	// SignalSendFrame (no layout or interaction beyond mouse_mappings)
	{
		menu::MenuPDA pda;
		menu::SignalSendFrame f;
		f.mouse_mappings["x"] = Input::Mouse_loc{1, 1, 1, 1};
		pda.push(f);
		pda.invalidate_layout();
		auto& r = std::get<menu::SignalSendFrame>(pda.top());
		EXPECT_TRUE(r.mouse_mappings.empty());
	}

	// SignalReturnFrame (no layout or interaction beyond mouse_mappings)
	{
		menu::MenuPDA pda;
		menu::SignalReturnFrame f;
		f.mouse_mappings["x"] = Input::Mouse_loc{1, 1, 1, 1};
		pda.push(f);
		pda.invalidate_layout();
		auto& r = std::get<menu::SignalReturnFrame>(pda.top());
		EXPECT_TRUE(r.mouse_mappings.empty());
	}
}
