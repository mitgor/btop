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
// MenuFrameVar tests (PDA-01, PDA-02)
// ========================================================================

TEST(MenuFrameVar, HasExactlyEightAlternatives) {
	EXPECT_EQ(std::variant_size_v<menu::MenuFrameVar>, 8u);
}

TEST(MenuFrameVar, HoldsMainFrame) {
	menu::MenuFrameVar v = menu::MainFrame{};
	EXPECT_TRUE(std::holds_alternative<menu::MainFrame>(v));
}

TEST(MenuFrameVar, HoldsOptionsFrame) {
	menu::MenuFrameVar v = menu::OptionsFrame{};
	EXPECT_TRUE(std::holds_alternative<menu::OptionsFrame>(v));
}

TEST(MenuFrameVar, HoldsHelpFrame) {
	menu::MenuFrameVar v = menu::HelpFrame{};
	EXPECT_TRUE(std::holds_alternative<menu::HelpFrame>(v));
}

TEST(MenuFrameVar, HoldsSizeErrorFrame) {
	menu::MenuFrameVar v = menu::SizeErrorFrame{};
	EXPECT_TRUE(std::holds_alternative<menu::SizeErrorFrame>(v));
}

TEST(MenuFrameVar, HoldsSignalChooseFrame) {
	menu::MenuFrameVar v = menu::SignalChooseFrame{};
	EXPECT_TRUE(std::holds_alternative<menu::SignalChooseFrame>(v));
}

TEST(MenuFrameVar, HoldsSignalSendFrame) {
	menu::MenuFrameVar v = menu::SignalSendFrame{};
	EXPECT_TRUE(std::holds_alternative<menu::SignalSendFrame>(v));
}

TEST(MenuFrameVar, HoldsSignalReturnFrame) {
	menu::MenuFrameVar v = menu::SignalReturnFrame{};
	EXPECT_TRUE(std::holds_alternative<menu::SignalReturnFrame>(v));
}

TEST(MenuFrameVar, HoldsReniceFrame) {
	menu::MenuFrameVar v = menu::ReniceFrame{};
	EXPECT_TRUE(std::holds_alternative<menu::ReniceFrame>(v));
}

// ========================================================================
// PDAAction tests (PDA-03)
// ========================================================================

TEST(PDAAction, AllValuesDistinct) {
	auto nc = menu::PDAAction::NoChange;
	auto pu = menu::PDAAction::Push;
	auto po = menu::PDAAction::Pop;
	auto re = menu::PDAAction::Replace;
	EXPECT_NE(nc, pu);
	EXPECT_NE(nc, po);
	EXPECT_NE(nc, re);
	EXPECT_NE(pu, po);
	EXPECT_NE(pu, re);
	EXPECT_NE(po, re);
}

TEST(PDAAction, HasFourValues) {
	// Verify all four enum values compile and are distinct
	[[maybe_unused]] auto a = menu::PDAAction::NoChange;
	[[maybe_unused]] auto b = menu::PDAAction::Push;
	[[maybe_unused]] auto c = menu::PDAAction::Pop;
	[[maybe_unused]] auto d = menu::PDAAction::Replace;
	SUCCEED();
}

// ========================================================================
// MenuPDA tests (PDA-01, PDA-02)
// ========================================================================

TEST(MenuPDA, StartsEmpty) {
	menu::MenuPDA pda;
	EXPECT_TRUE(pda.empty());
	EXPECT_EQ(pda.depth(), 0u);
}

TEST(MenuPDA, PushIncreasesDepth) {
	menu::MenuPDA pda;
	pda.push(menu::MainFrame{});
	EXPECT_FALSE(pda.empty());
	EXPECT_EQ(pda.depth(), 1u);
}

TEST(MenuPDA, PopDecreasesDepth) {
	menu::MenuPDA pda;
	pda.push(menu::MainFrame{});
	pda.push(menu::OptionsFrame{});
	EXPECT_EQ(pda.depth(), 2u);
	pda.pop();
	EXPECT_EQ(pda.depth(), 1u);
	EXPECT_TRUE(std::holds_alternative<menu::MainFrame>(pda.top()));
}

TEST(MenuPDA, ReplaceKeepsDepth) {
	menu::MenuPDA pda;
	pda.push(menu::MainFrame{});
	EXPECT_EQ(pda.depth(), 1u);
	pda.replace(menu::OptionsFrame{});
	EXPECT_EQ(pda.depth(), 1u);
	EXPECT_TRUE(std::holds_alternative<menu::OptionsFrame>(pda.top()));
}

TEST(MenuPDA, TopReturnsCorrectFrame) {
	menu::MenuPDA pda;
	menu::SignalChooseFrame scf;
	scf.selected_signal = 9;
	pda.push(scf);
	auto& frame = std::get<menu::SignalChooseFrame>(pda.top());
	EXPECT_EQ(frame.selected_signal, 9);
}

TEST(MenuPDA, TopReturnsMutableRef) {
	menu::MenuPDA pda;
	pda.push(menu::MainFrame{});
	auto& frame = std::get<menu::MainFrame>(pda.top());
	frame.selected = 5;
	auto& again = std::get<menu::MainFrame>(pda.top());
	EXPECT_EQ(again.selected, 5);
}

TEST(MenuPDA, FrameIsolation) {
	menu::MenuPDA pda;
	menu::MainFrame mf;
	mf.selected = 2;
	pda.push(mf);

	menu::OptionsFrame of;
	of.page = 3;
	pda.push(of);

	auto& top_of = std::get<menu::OptionsFrame>(pda.top());
	EXPECT_EQ(top_of.page, 3);

	pda.pop();

	auto& top_mf = std::get<menu::MainFrame>(pda.top());
	EXPECT_EQ(top_mf.selected, 2);
}

TEST(MenuPDA, MultiplePopToEmpty) {
	menu::MenuPDA pda;
	pda.push(menu::MainFrame{});
	pda.push(menu::HelpFrame{});
	pda.pop();
	pda.pop();
	EXPECT_TRUE(pda.empty());
}

// ========================================================================
// bg lifecycle tests (PDA-05)
// ========================================================================

TEST(MenuPDA, BgStartsEmpty) {
	menu::MenuPDA pda;
	EXPECT_TRUE(pda.bg().empty());
}

TEST(MenuPDA, SetBgStores) {
	menu::MenuPDA pda;
	pda.set_bg("test_background");
	EXPECT_EQ(pda.bg(), "test_background");
}

TEST(MenuPDA, ClearBgEmpties) {
	menu::MenuPDA pda;
	pda.set_bg("test_background");
	pda.clear_bg();
	EXPECT_TRUE(pda.bg().empty());
}

// ========================================================================
// Frame default initialization tests (FRAME-04)
// ========================================================================

TEST(FrameDefaults, MainFrameDefaults) {
	menu::MainFrame f;
	EXPECT_EQ(f.y, 0);
	EXPECT_EQ(f.selected, 0);
	EXPECT_TRUE(f.colors_selected.empty());
	EXPECT_TRUE(f.colors_normal.empty());
	EXPECT_TRUE(f.mouse_mappings.empty());
}

TEST(FrameDefaults, OptionsFrameDefaults) {
	menu::OptionsFrame f;
	EXPECT_EQ(f.x, 0);
	EXPECT_EQ(f.y, 0);
	EXPECT_EQ(f.height, 0);
	EXPECT_EQ(f.page, 0);
	EXPECT_EQ(f.pages, 0);
	EXPECT_EQ(f.selected, 0);
	EXPECT_EQ(f.select_max, 0);
	EXPECT_EQ(f.item_height, 0);
	EXPECT_EQ(f.selected_cat, 0);
	EXPECT_EQ(f.max_items, 0);
	EXPECT_EQ(f.last_sel, 0);
	EXPECT_FALSE(f.editing);
	EXPECT_TRUE(f.warnings.empty());
	EXPECT_EQ(f.selPred.to_ulong(), 0u);
	EXPECT_TRUE(f.mouse_mappings.empty());
}

TEST(FrameDefaults, HelpFrameDefaults) {
	menu::HelpFrame f;
	EXPECT_EQ(f.x, 0);
	EXPECT_EQ(f.y, 0);
	EXPECT_EQ(f.height, 0);
	EXPECT_EQ(f.page, 0);
	EXPECT_EQ(f.pages, 0);
	EXPECT_TRUE(f.mouse_mappings.empty());
}

TEST(FrameDefaults, SignalChooseFrameDefaults) {
	menu::SignalChooseFrame f;
	EXPECT_EQ(f.x, 0);
	EXPECT_EQ(f.y, 0);
	EXPECT_EQ(f.selected_signal, -1);
	EXPECT_TRUE(f.mouse_mappings.empty());
}

TEST(FrameDefaults, SignalSendFrameDefaults) {
	menu::SignalSendFrame f;
	EXPECT_TRUE(f.mouse_mappings.empty());
}

TEST(FrameDefaults, SignalReturnFrameDefaults) {
	menu::SignalReturnFrame f;
	EXPECT_TRUE(f.mouse_mappings.empty());
}

TEST(FrameDefaults, SizeErrorFrameDefaults) {
	menu::SizeErrorFrame f;
	EXPECT_TRUE(f.mouse_mappings.empty());
}

TEST(FrameDefaults, ReniceFrameDefaults) {
	menu::ReniceFrame f;
	EXPECT_EQ(f.x, 0);
	EXPECT_EQ(f.y, 0);
	EXPECT_EQ(f.selected_nice, 0);
	EXPECT_TRUE(f.nice_edit.empty());
	EXPECT_TRUE(f.mouse_mappings.empty());
}

// ========================================================================
// Per-frame mouse mappings test (FRAME-05)
// ========================================================================

// ========================================================================
// InvalidateLayout tests (FRAME-03)
// ========================================================================

TEST(InvalidateLayout, MainFrameZerosLayoutPreservesInteraction) {
	menu::MainFrame f;
	// Set layout fields to non-zero
	f.y = 42;
	f.mouse_mappings["btn"] = Input::Mouse_loc{1, 2, 3, 4};
	// Set interaction fields to non-zero
	f.selected = 7;
	f.colors_selected = {"red", "blue"};
	f.colors_normal = {"green"};

	f.invalidate_layout();

	// Layout fields zeroed
	EXPECT_EQ(f.y, 0);
	EXPECT_TRUE(f.mouse_mappings.empty());
	// Interaction fields preserved
	EXPECT_EQ(f.selected, 7);
	EXPECT_EQ(f.colors_selected.size(), 2u);
	EXPECT_EQ(f.colors_normal.size(), 1u);
}

TEST(InvalidateLayout, OptionsFrameZerosLayoutPreservesInteraction) {
	menu::OptionsFrame f;
	// Set layout fields
	f.x = 10; f.y = 20; f.height = 30;
	f.mouse_mappings["opt"] = Input::Mouse_loc{5, 6, 7, 8};
	// Set interaction fields
	f.page = 2; f.pages = 5; f.selected = 3; f.select_max = 10;
	f.item_height = 4; f.selected_cat = 1; f.max_items = 8;
	f.last_sel = 2; f.editing = true; f.warnings = "warn";
	f.selPred.set(3);

	f.invalidate_layout();

	// Layout zeroed
	EXPECT_EQ(f.x, 0);
	EXPECT_EQ(f.y, 0);
	EXPECT_EQ(f.height, 0);
	EXPECT_TRUE(f.mouse_mappings.empty());
	// Interaction preserved
	EXPECT_EQ(f.page, 2);
	EXPECT_EQ(f.pages, 5);
	EXPECT_EQ(f.selected, 3);
	EXPECT_EQ(f.select_max, 10);
	EXPECT_EQ(f.item_height, 4);
	EXPECT_EQ(f.selected_cat, 1);
	EXPECT_EQ(f.max_items, 8);
	EXPECT_EQ(f.last_sel, 2);
	EXPECT_TRUE(f.editing);
	EXPECT_EQ(f.warnings, "warn");
	EXPECT_TRUE(f.selPred.test(3));
}

TEST(InvalidateLayout, HelpFrameZerosLayoutPreservesInteraction) {
	menu::HelpFrame f;
	f.x = 15; f.y = 25; f.height = 35;
	f.mouse_mappings["help"] = Input::Mouse_loc{1, 1, 1, 1};
	f.page = 3; f.pages = 7;

	f.invalidate_layout();

	EXPECT_EQ(f.x, 0);
	EXPECT_EQ(f.y, 0);
	EXPECT_EQ(f.height, 0);
	EXPECT_TRUE(f.mouse_mappings.empty());
	EXPECT_EQ(f.page, 3);
	EXPECT_EQ(f.pages, 7);
}

TEST(InvalidateLayout, SizeErrorFrameClearsMouseMappings) {
	menu::SizeErrorFrame f;
	f.mouse_mappings["err"] = Input::Mouse_loc{9, 9, 9, 9};

	f.invalidate_layout();

	EXPECT_TRUE(f.mouse_mappings.empty());
}

TEST(InvalidateLayout, SignalChooseFrameZerosLayoutPreservesInteraction) {
	menu::SignalChooseFrame f;
	f.x = 100; f.y = 200;
	f.mouse_mappings["sig"] = Input::Mouse_loc{2, 3, 4, 5};
	f.selected_signal = 15;

	f.invalidate_layout();

	EXPECT_EQ(f.x, 0);
	EXPECT_EQ(f.y, 0);
	EXPECT_TRUE(f.mouse_mappings.empty());
	EXPECT_EQ(f.selected_signal, 15);
}

TEST(InvalidateLayout, SignalSendFrameClearsMouseMappings) {
	menu::SignalSendFrame f;
	f.mouse_mappings["send"] = Input::Mouse_loc{1, 2, 3, 4};

	f.invalidate_layout();

	EXPECT_TRUE(f.mouse_mappings.empty());
}

TEST(InvalidateLayout, SignalReturnFrameClearsMouseMappings) {
	menu::SignalReturnFrame f;
	f.mouse_mappings["ret"] = Input::Mouse_loc{5, 6, 7, 8};

	f.invalidate_layout();

	EXPECT_TRUE(f.mouse_mappings.empty());
}

TEST(InvalidateLayout, ReniceFrameZerosLayoutPreservesInteraction) {
	menu::ReniceFrame f;
	f.x = 50; f.y = 60;
	f.mouse_mappings["ren"] = Input::Mouse_loc{3, 4, 5, 6};
	f.selected_nice = 10;
	f.nice_edit = "editing";

	f.invalidate_layout();

	EXPECT_EQ(f.x, 0);
	EXPECT_EQ(f.y, 0);
	EXPECT_TRUE(f.mouse_mappings.empty());
	EXPECT_EQ(f.selected_nice, 10);
	EXPECT_EQ(f.nice_edit, "editing");
}

TEST(InvalidateLayout, MenuPDADispatchesToTopFrame) {
	menu::MenuPDA pda;
	menu::MainFrame mf;
	mf.y = 99;
	mf.selected = 5;
	pda.push(mf);

	pda.invalidate_layout();

	auto& frame = std::get<menu::MainFrame>(pda.top());
	EXPECT_EQ(frame.y, 0);
	EXPECT_EQ(frame.selected, 5);
}

TEST(InvalidateLayout, MenuPDAEmptyIsNoOp) {
	menu::MenuPDA pda;
	// Should not crash
	pda.invalidate_layout();
	EXPECT_TRUE(pda.empty());
}

// ========================================================================
// Per-frame mouse mappings test (FRAME-05)
// ========================================================================

TEST(FrameMouseMappings, FrameOwnsMouseMappings) {
	menu::MainFrame mf;
	mf.mouse_mappings["button1"] = Input::Mouse_loc{1, 2, 3, 4};
	EXPECT_EQ(mf.mouse_mappings.size(), 1u);
	EXPECT_EQ(mf.mouse_mappings["button1"].line, 1);
	EXPECT_EQ(mf.mouse_mappings["button1"].col, 2);

	// Push onto PDA, push another frame, pop, verify original mappings survive
	menu::MenuPDA pda;
	pda.push(mf);
	pda.push(menu::HelpFrame{});
	pda.pop();

	auto& restored = std::get<menu::MainFrame>(pda.top());
	EXPECT_EQ(restored.mouse_mappings.size(), 1u);
	EXPECT_EQ(restored.mouse_mappings["button1"].line, 1);
	EXPECT_EQ(restored.mouse_mappings["button1"].col, 2);
	EXPECT_EQ(restored.mouse_mappings["button1"].height, 3);
	EXPECT_EQ(restored.mouse_mappings["button1"].width, 4);
}
