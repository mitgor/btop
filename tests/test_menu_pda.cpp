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
