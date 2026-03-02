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

// btop_menu_pda.hpp — Menu pushdown automaton type definitions
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <bitset>
#include <cassert>
#include <cstddef>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "btop_input.hpp"
#include "btop_draw.hpp"

namespace menu {

	// ========================================================================
	// Frame structs — typed per-menu-screen state.
	// Each frame owns its layout geometry, interaction state, and mouse mappings.
	// No behavior methods — these are pure data carriers (Phase 20 scope).
	// ========================================================================

	struct MainFrame {
		// --- Layout fields ---
		int y{};

		// --- Interaction fields ---
		int selected{};
		std::vector<std::string> colors_selected{};
		std::vector<std::string> colors_normal{};

		// --- Per-frame mouse mappings ---
		std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
	};

	struct OptionsFrame {
		// --- Layout fields ---
		int x{};
		int y{};
		int height{};

		// --- Interaction fields ---
		int page{};
		int pages{};
		int selected{};
		int select_max{};
		int item_height{};
		int selected_cat{};
		int max_items{};
		int last_sel{};
		bool editing{false};
		Draw::TextEdit editor{};
		std::string warnings{};
		std::bitset<8> selPred{};

		// --- Per-frame mouse mappings ---
		std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
	};

	struct HelpFrame {
		// --- Layout fields ---
		int x{};
		int y{};
		int height{};

		// --- Interaction fields ---
		int page{};
		int pages{};

		// --- Per-frame mouse mappings ---
		std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
	};

	struct SizeErrorFrame {
		// --- Per-frame mouse mappings ---
		std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
	};

	struct SignalChooseFrame {
		// --- Layout fields ---
		int x{};
		int y{};

		// --- Interaction fields ---
		int selected_signal{-1};

		// --- Per-frame mouse mappings ---
		std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
	};

	struct SignalSendFrame {
		// --- Per-frame mouse mappings ---
		std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
	};

	struct SignalReturnFrame {
		// --- Per-frame mouse mappings ---
		std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
	};

	struct ReniceFrame {
		// --- Layout fields ---
		int x{};
		int y{};

		// --- Interaction fields ---
		int selected_nice{0};
		std::string nice_edit{};

		// --- Per-frame mouse mappings ---
		std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
	};

	// ========================================================================
	// MenuFrameVar — variant holding exactly one active frame.
	// Order matches the logical menu hierarchy.
	// ========================================================================

	using MenuFrameVar = std::variant<
		MainFrame,
		OptionsFrame,
		HelpFrame,
		SizeErrorFrame,
		SignalChooseFrame,
		SignalSendFrame,
		SignalReturnFrame,
		ReniceFrame
	>;

	// ========================================================================
	// PDAAction — return value from transition functions (Phase 21+).
	// Tells the PDA what stack operation to perform after a handler runs.
	// ========================================================================

	enum class PDAAction {
		NoChange,  ///< Keep current frame on top, no stack change
		Push,      ///< Push a new frame onto the stack
		Pop,       ///< Pop the current frame, reveal frame underneath
		Replace,   ///< Replace the current frame with a new one
	};

	// ========================================================================
	// MenuPDA — pushdown automaton wrapping a stack of MenuFrameVar.
	// Owns the bg (background) string lifecycle: captured on first push,
	// cleared on final pop.
	// ========================================================================

	class MenuPDA {
		std::stack<MenuFrameVar, std::vector<MenuFrameVar>> stack_;
		std::string bg_;

	public:
		/// Push a new frame onto the stack.
		void push(MenuFrameVar frame) {
			stack_.push(std::move(frame));
		}

		/// Pop the top frame. Asserts the stack is non-empty.
		void pop() {
			assert(!stack_.empty() && "MenuPDA::pop() called on empty stack");
			stack_.pop();
		}

		/// Replace the top frame with a new one. Asserts non-empty.
		void replace(MenuFrameVar frame) {
			assert(!stack_.empty() && "MenuPDA::replace() called on empty stack");
			stack_.pop();
			stack_.push(std::move(frame));
		}

		/// Access the top frame (mutable). Asserts non-empty.
		MenuFrameVar& top() {
			assert(!stack_.empty() && "MenuPDA::top() called on empty stack");
			return stack_.top();
		}

		/// Access the top frame (const). Asserts non-empty.
		const MenuFrameVar& top() const {
			assert(!stack_.empty() && "MenuPDA::top() const called on empty stack");
			return stack_.top();
		}

		/// Check if the stack is empty (no menu active).
		[[nodiscard]] bool empty() const noexcept {
			return stack_.empty();
		}

		/// Return the number of frames on the stack.
		[[nodiscard]] std::size_t depth() const noexcept {
			return stack_.size();
		}

		/// Get the stored background string.
		const std::string& bg() const noexcept {
			return bg_;
		}

		/// Store a background string (captured on first push in Phase 21+).
		void set_bg(std::string bg) {
			bg_ = std::move(bg);
		}

		/// Clear the background string (on final pop in Phase 21+).
		void clear_bg() {
			bg_.clear();
		}
	};

} // namespace menu
