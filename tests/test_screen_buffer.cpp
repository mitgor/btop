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

// Unit tests for Cell, ScreenBuffer, render_to_buffer, and diff_and_emit (Phase 05 Plan 03)

#include <string>

#include <gtest/gtest.h>

#include "btop_draw.hpp"

using namespace Draw;

// ===== Cell equality tests =====

TEST(CellTest, DefaultCellsAreEqual) {
	Cell a{}, b{};
	EXPECT_EQ(a, b);
}

TEST(CellTest, DifferentCharsAreNotEqual) {
	Cell a{}, b{};
	a.set_char('A');
	b.set_char('B');
	EXPECT_NE(a, b);
}

TEST(CellTest, DifferentFgColorsAreNotEqual) {
	Cell a{}, b{};
	a.fg = (1u << 24) | (255 << 16);  // Red truecolor
	b.fg = (1u << 24) | (255 << 8);   // Green truecolor
	EXPECT_NE(a, b);
}

TEST(CellTest, DifferentBgColorsAreNotEqual) {
	Cell a{}, b{};
	a.bg = 5;  // 256-color index 4 (stored as N+1)
	EXPECT_NE(a, b);
}

TEST(CellTest, DifferentAttrsAreNotEqual) {
	Cell a{}, b{};
	a.attrs = 1;  // Bold
	EXPECT_NE(a, b);
}

TEST(CellTest, SameContentCellsAreEqual) {
	Cell a{}, b{};
	a.set_char('X');
	a.fg = (1u << 24) | (100 << 16) | (200 << 8) | 50;
	a.bg = 42;
	a.attrs = 0b00000101;  // Bold + italic

	b.set_char('X');
	b.fg = (1u << 24) | (100 << 16) | (200 << 8) | 50;
	b.bg = 42;
	b.attrs = 0b00000101;

	EXPECT_EQ(a, b);
}

// ===== ScreenBuffer tests =====

TEST(ScreenBufferTest, ResizeSetsWidthAndHeight) {
	ScreenBuffer buf;
	buf.resize(80, 24);
	EXPECT_EQ(buf.width(), 80);
	EXPECT_EQ(buf.height(), 24);
}

TEST(ScreenBufferTest, ResizeClearsToDefaults) {
	ScreenBuffer buf;
	buf.resize(10, 5);
	Cell default_cell{};
	for (int y = 0; y < 5; y++) {
		for (int x = 0; x < 10; x++) {
			EXPECT_EQ(buf.at(x, y), default_cell);
		}
	}
}

TEST(ScreenBufferTest, ResizeSetsForceFullTrue) {
	ScreenBuffer buf;
	buf.resize(80, 24);
	EXPECT_TRUE(buf.needs_full());
}

TEST(ScreenBufferTest, SwapExchangesBuffers) {
	ScreenBuffer buf;
	buf.resize(10, 5);

	// Write to current buffer
	buf.at(0, 0).set_char('A');
	EXPECT_EQ(buf.at(0, 0).ch[0], 'A');

	// Swap -- current becomes previous, old previous becomes current
	buf.swap();

	// Previous should now have 'A'
	EXPECT_EQ(buf.prev_at(0, 0).ch[0], 'A');

	// Current should be default (the old previous buffer)
	Cell default_cell{};
	EXPECT_EQ(buf.at(0, 0), default_cell);
}

TEST(ScreenBufferTest, ClearCurrentResetsAllCells) {
	ScreenBuffer buf;
	buf.resize(5, 3);

	// Write some content
	buf.at(1, 1).set_char('X');
	buf.at(2, 2).fg = 42;

	// Clear
	buf.clear_current();

	Cell default_cell{};
	for (int y = 0; y < 3; y++) {
		for (int x = 0; x < 5; x++) {
			EXPECT_EQ(buf.at(x, y), default_cell);
		}
	}
}

TEST(ScreenBufferTest, ForceFullFlagManagement) {
	ScreenBuffer buf;
	buf.resize(10, 5);

	EXPECT_TRUE(buf.needs_full());
	buf.clear_force_full();
	EXPECT_FALSE(buf.needs_full());
	buf.set_force_full();
	EXPECT_TRUE(buf.needs_full());
}

// ===== render_to_buffer tests =====

TEST(RenderToBufferTest, BasicCharacterAtPosition) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Mv::to(1,1) = "\x1b[1;1f", then 'A'
	std::string input = "\x1b[1;1fA";
	render_to_buffer(buf, input);

	EXPECT_EQ(buf.at(0, 0).ch[0], 'A');
	EXPECT_EQ(buf.at(0, 0).ch_len, 1);
}

TEST(RenderToBufferTest, CursorPositioning) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Move to row 5, col 10 (0-based: y=4, x=9), print 'X'
	std::string input = "\x1b[5;10fX";
	render_to_buffer(buf, input);

	EXPECT_EQ(buf.at(9, 4).ch[0], 'X');
}

TEST(RenderToBufferTest, TruecolorForeground) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Set truecolor red foreground: ESC[38;2;255;0;0m then 'R'
	std::string input = "\x1b[1;1f\x1b[38;2;255;0;0mR";
	render_to_buffer(buf, input);

	const auto& cell = buf.at(0, 0);
	EXPECT_EQ(cell.ch[0], 'R');
	// Truecolor: bit 24 set, R=255, G=0, B=0
	uint32_t expected_fg = (1u << 24) | (255u << 16) | (0u << 8) | 0u;
	EXPECT_EQ(cell.fg, expected_fg);
}

TEST(RenderToBufferTest, TruecolorBackground) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Set truecolor blue background: ESC[48;2;0;0;200m then 'B'
	std::string input = "\x1b[1;1f\x1b[48;2;0;0;200mB";
	render_to_buffer(buf, input);

	const auto& cell = buf.at(0, 0);
	uint32_t expected_bg = (1u << 24) | (0u << 16) | (0u << 8) | 200u;
	EXPECT_EQ(cell.bg, expected_bg);
}

TEST(RenderToBufferTest, Color256Foreground) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Set 256-color fg: ESC[38;5;196m then 'C'
	std::string input = "\x1b[1;1f\x1b[38;5;196mC";
	render_to_buffer(buf, input);

	const auto& cell = buf.at(0, 0);
	// 256-color stored as index+1
	EXPECT_EQ(cell.fg, 197u);
}

TEST(RenderToBufferTest, BoldAttribute) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Set bold: ESC[1m then 'B'
	std::string input = "\x1b[1;1f\x1b[1mB";
	render_to_buffer(buf, input);

	EXPECT_EQ(buf.at(0, 0).attrs & (1 << 0), (1 << 0));  // Bold bit set
}

TEST(RenderToBufferTest, ResetClearsAll) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Bold + color, then reset, then write
	std::string input = "\x1b[1;1f\x1b[1m\x1b[38;2;255;0;0mX\x1b[0mY";
	render_to_buffer(buf, input);

	// X should have bold + color
	EXPECT_NE(buf.at(0, 0).fg, 0u);
	EXPECT_NE(buf.at(0, 0).attrs, 0);

	// Y should be reset
	EXPECT_EQ(buf.at(1, 0).fg, 0u);
	EXPECT_EQ(buf.at(1, 0).attrs, 0);
}

TEST(RenderToBufferTest, CursorRelativeMoves) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Move to (1,1), print 'A', move right 3, print 'B'
	std::string input = "\x1b[1;1fA\x1b[3CB";
	render_to_buffer(buf, input);

	EXPECT_EQ(buf.at(0, 0).ch[0], 'A');  // (0,0)
	// After 'A', cursor at x=1. Move right 3 -> x=4. Print 'B' at x=4
	EXPECT_EQ(buf.at(4, 0).ch[0], 'B');
}

TEST(RenderToBufferTest, UTF8BrailleCharacters) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Braille character (3-byte UTF-8): U+2800 = 0xE2 0xA0 0x80
	std::string input = "\x1b[1;1f\xe2\xa0\x80";
	render_to_buffer(buf, input);

	const auto& cell = buf.at(0, 0);
	EXPECT_EQ(cell.ch_len, 3);
	EXPECT_EQ(static_cast<unsigned char>(cell.ch[0]), 0xE2);
	EXPECT_EQ(static_cast<unsigned char>(cell.ch[1]), 0xA0);
	EXPECT_EQ(static_cast<unsigned char>(cell.ch[2]), 0x80);
}

TEST(RenderToBufferTest, MultipleCharactersSequential) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Write "Hello" starting at position (1,1)
	std::string input = "\x1b[1;1fHello";
	render_to_buffer(buf, input);

	EXPECT_EQ(buf.at(0, 0).ch[0], 'H');
	EXPECT_EQ(buf.at(1, 0).ch[0], 'e');
	EXPECT_EQ(buf.at(2, 0).ch[0], 'l');
	EXPECT_EQ(buf.at(3, 0).ch[0], 'l');
	EXPECT_EQ(buf.at(4, 0).ch[0], 'o');
}

TEST(RenderToBufferTest, OutOfBoundsIgnored) {
	ScreenBuffer buf;
	buf.resize(5, 3);

	// Write at row 10, col 10 (out of bounds for 5x3 buffer)
	std::string input = "\x1b[10;10fX";
	render_to_buffer(buf, input);

	// Should not crash. All cells should remain default.
	Cell default_cell{};
	for (int y = 0; y < 3; y++) {
		for (int x = 0; x < 5; x++) {
			EXPECT_EQ(buf.at(x, y), default_cell);
		}
	}
}

TEST(RenderToBufferTest, StandardColors) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// ESC[31m = red fg (standard color 1), ESC[42m = green bg (standard color 2)
	std::string input = "\x1b[1;1f\x1b[31m\x1b[42mX";
	render_to_buffer(buf, input);

	const auto& cell = buf.at(0, 0);
	// Standard fg red: code 31 -> index 1, stored as index+1 = 2
	EXPECT_EQ(cell.fg, 2u);
	// Standard bg green: code 42 -> index 2, stored as index+1 = 3
	EXPECT_EQ(cell.bg, 3u);
}

TEST(RenderToBufferTest, BrightColors) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// ESC[91m = bright red fg (code 91), ESC[107m = bright white bg (code 107)
	std::string input = "\x1b[1;1f\x1b[91m\x1b[107mX";
	render_to_buffer(buf, input);

	const auto& cell = buf.at(0, 0);
	// Bright red: code 91 -> 91-90+8 = 9, stored as index+1 = 10
	EXPECT_EQ(cell.fg, 10u);
	// Bright white: code 107 -> 107-100+8 = 15, stored as index+1 = 16
	EXPECT_EQ(cell.bg, 16u);
}

// ===== diff_and_emit tests =====

TEST(DiffAndEmitTest, IdenticalBuffersProduceEmptyOutput) {
	ScreenBuffer buf;
	buf.resize(10, 5);

	// Both buffers are default -- no changes
	std::string out;
	diff_and_emit(buf, out);
	EXPECT_TRUE(out.empty());
}

TEST(DiffAndEmitTest, SingleCellChange) {
	ScreenBuffer buf;
	buf.resize(10, 5);

	// Change one cell in current
	buf.at(3, 2).set_char('X');

	std::string out;
	diff_and_emit(buf, out);

	// Output should contain cursor move to (3,2) -> ESC[3;4f and 'X'
	EXPECT_FALSE(out.empty());
	EXPECT_NE(out.find("\x1b[3;4f"), std::string::npos);  // Row 3, Col 4 (1-based)
	EXPECT_NE(out.find('X'), std::string::npos);
}

TEST(DiffAndEmitTest, AdjacentCellsNoCursorMoveBetween) {
	ScreenBuffer buf;
	buf.resize(10, 5);

	// Change two adjacent cells
	buf.at(3, 1).set_char('A');
	buf.at(4, 1).set_char('B');

	std::string out;
	diff_and_emit(buf, out);

	EXPECT_FALSE(out.empty());

	// Should have one cursor move to (3,1) -> ESC[2;4f
	// Then 'A' immediately followed by 'B' (no second cursor move)
	std::string cursor_move = "\x1b[2;4f";
	size_t first_pos = out.find(cursor_move);
	EXPECT_NE(first_pos, std::string::npos);

	// They may have color escapes between them if colors differ, but both are default
	// so 'A' and 'B' should be adjacent or separated only by attr/color resets
	EXPECT_NE(out.find('A'), std::string::npos);
	EXPECT_NE(out.find('B'), std::string::npos);

	// Count cursor moves -- should be exactly 1
	size_t count = 0;
	size_t pos = 0;
	while ((pos = out.find("\x1b[", pos)) != std::string::npos) {
		// Check if this is a cursor move (ends in 'f')
		size_t end = out.find('f', pos + 2);
		size_t m_end = out.find('m', pos + 2);
		if (end != std::string::npos && (m_end == std::string::npos || end < m_end)) {
			count++;
		}
		pos += 2;
	}
	EXPECT_EQ(count, 1u);
}

TEST(DiffAndEmitTest, NonAdjacentCellsGetSeparateCursorMoves) {
	ScreenBuffer buf;
	buf.resize(10, 5);

	// Change two non-adjacent cells
	buf.at(1, 0).set_char('A');
	buf.at(5, 0).set_char('B');

	std::string out;
	diff_and_emit(buf, out);

	// Should have two cursor moves
	EXPECT_FALSE(out.empty());
	EXPECT_NE(out.find('A'), std::string::npos);
	EXPECT_NE(out.find('B'), std::string::npos);
}

TEST(DiffAndEmitTest, EmitsColorChanges) {
	ScreenBuffer buf;
	buf.resize(10, 5);

	// Change one cell with color
	auto& cell = buf.at(0, 0);
	cell.set_char('R');
	cell.fg = (1u << 24) | (255u << 16);  // Truecolor red

	std::string out;
	diff_and_emit(buf, out);

	// Should contain truecolor fg escape
	EXPECT_NE(out.find("38;2;255;0;0"), std::string::npos);
	EXPECT_NE(out.find('R'), std::string::npos);
}

// ===== full_emit tests =====

TEST(FullEmitTest, EmitsNonDefaultCells) {
	ScreenBuffer buf;
	buf.resize(10, 5);

	buf.at(0, 0).set_char('A');
	buf.at(5, 2).set_char('B');

	std::string out;
	full_emit(buf, out);

	EXPECT_FALSE(out.empty());
	EXPECT_NE(out.find('A'), std::string::npos);
	EXPECT_NE(out.find('B'), std::string::npos);
}

TEST(FullEmitTest, EmptyBufferProducesEmptyOutput) {
	ScreenBuffer buf;
	buf.resize(10, 5);

	std::string out;
	full_emit(buf, out);

	EXPECT_TRUE(out.empty());
}

// ===== Round-trip test =====

TEST(RoundTripTest, RenderThenDiffProducesCorrectOutput) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Render a simple escape string to current buffer
	std::string input = "\x1b[1;1f\x1b[38;2;100;200;50mHello\x1b[3;10fWorld";
	render_to_buffer(buf, input);

	// Diff against empty previous should produce output for all non-default cells
	std::string diff_out;
	diff_and_emit(buf, diff_out);

	// Now render the diff output into a fresh buffer and verify it matches
	ScreenBuffer buf2;
	buf2.resize(80, 24);
	render_to_buffer(buf2, diff_out);

	// Verify the cells match for the positions that were written
	// "Hello" at (0,0)-(4,0)
	for (int x = 0; x < 5; x++) {
		EXPECT_EQ(buf.at(x, 0).ch[0], buf2.at(x, 0).ch[0])
			<< "Mismatch at (" << x << ",0)";
		EXPECT_EQ(buf.at(x, 0).fg, buf2.at(x, 0).fg)
			<< "FG mismatch at (" << x << ",0)";
	}

	// "World" at (9,2)-(13,2)
	const char* world = "World";
	for (int x = 0; x < 5; x++) {
		EXPECT_EQ(buf.at(9 + x, 2).ch[0], world[x])
			<< "Expected '" << world[x] << "' at (" << (9 + x) << ",2)";
		EXPECT_EQ(buf.at(9 + x, 2).ch[0], buf2.at(9 + x, 2).ch[0])
			<< "Mismatch at (" << (9 + x) << ",2)";
	}
}

TEST(RenderToBufferTest, PrivateModeSequencesIgnored) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Sync start/end should be ignored: ESC[?2026h and ESC[?2026l
	std::string input = "\x1b[?2026h\x1b[1;1fA\x1b[?2026l";
	render_to_buffer(buf, input);

	EXPECT_EQ(buf.at(0, 0).ch[0], 'A');
}

TEST(RenderToBufferTest, NewlineMovesToNextRow) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Write 'A' then newline then 'B'
	std::string input = "\x1b[1;1fA\nB";
	render_to_buffer(buf, input);

	EXPECT_EQ(buf.at(0, 0).ch[0], 'A');
	EXPECT_EQ(buf.at(0, 1).ch[0], 'B');  // Next row, column 0
}

TEST(RenderToBufferTest, MultipleAttributes) {
	ScreenBuffer buf;
	buf.resize(80, 24);

	// Bold + italic + underline: ESC[1;3;4m
	std::string input = "\x1b[1;1f\x1b[1;3;4mX";
	render_to_buffer(buf, input);

	const auto& cell = buf.at(0, 0);
	EXPECT_EQ(cell.attrs & (1 << 0), (1 << 0));  // Bold
	EXPECT_EQ(cell.attrs & (1 << 2), (1 << 2));  // Italic
	EXPECT_EQ(cell.attrs & (1 << 3), (1 << 3));  // Underline
}
