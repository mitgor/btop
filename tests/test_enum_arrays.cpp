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

// Depends on Task 2 (graph_symbols + Graph::graphs migration)

#include <gtest/gtest.h>
#include "btop_draw.hpp"
#include "btop_shared.hpp"

// --- GraphSymbolType enum tests ---

TEST(GraphSymbolType, EnumCount) {
	// GraphSymbolType has exactly 6 entries + COUNT
	EXPECT_EQ(static_cast<size_t>(Draw::GraphSymbolType::COUNT), 6u);
}

TEST(GraphSymbolType, ToGraphSymbolTypeBraille) {
	EXPECT_EQ(Draw::to_graph_symbol_type("braille", false), Draw::GraphSymbolType::braille_up);
	EXPECT_EQ(Draw::to_graph_symbol_type("braille", true), Draw::GraphSymbolType::braille_down);
}

TEST(GraphSymbolType, ToGraphSymbolTypeBlock) {
	EXPECT_EQ(Draw::to_graph_symbol_type("block", false), Draw::GraphSymbolType::block_up);
	EXPECT_EQ(Draw::to_graph_symbol_type("block", true), Draw::GraphSymbolType::block_down);
}

TEST(GraphSymbolType, ToGraphSymbolTypeTty) {
	EXPECT_EQ(Draw::to_graph_symbol_type("tty", false), Draw::GraphSymbolType::tty_up);
	EXPECT_EQ(Draw::to_graph_symbol_type("tty", true), Draw::GraphSymbolType::tty_down);
}

TEST(GraphSymbolType, DefaultMapsToBraille) {
	EXPECT_EQ(Draw::to_graph_symbol_type("default", false), Draw::GraphSymbolType::braille_up);
	EXPECT_EQ(Draw::to_graph_symbol_type("default", true), Draw::GraphSymbolType::braille_down);
}

// --- graph_symbols array tests ---

TEST(GraphSymbols, ArrayHasSixEntries) {
	EXPECT_EQ(Draw::graph_symbols.size(), 6u);
}

TEST(GraphSymbols, BrailleUpFirstIsSpace) {
	EXPECT_EQ(Draw::graph_symbols[std::to_underlying(Draw::GraphSymbolType::braille_up)].at(0), " ");
}

TEST(GraphSymbols, BrailleUpLastIsFullBlock) {
	const auto& braille_up = Draw::graph_symbols[std::to_underlying(Draw::GraphSymbolType::braille_up)];
	EXPECT_EQ(braille_up.back(), "\xe2\xa3\xbf");  // U+28FF BRAILLE PATTERN DOTS-12345678
}

TEST(GraphSymbols, BrailleDownFirstIsSpace) {
	EXPECT_EQ(Draw::graph_symbols[std::to_underlying(Draw::GraphSymbolType::braille_down)].at(0), " ");
}

TEST(GraphSymbols, BlockUpHas25Entries) {
	EXPECT_EQ(Draw::graph_symbols[std::to_underlying(Draw::GraphSymbolType::block_up)].size(), 25u);
}

TEST(GraphSymbols, BlockDownHas25Entries) {
	EXPECT_EQ(Draw::graph_symbols[std::to_underlying(Draw::GraphSymbolType::block_down)].size(), 25u);
}

TEST(GraphSymbols, TtyUpHas25Entries) {
	EXPECT_EQ(Draw::graph_symbols[std::to_underlying(Draw::GraphSymbolType::tty_up)].size(), 25u);
}

TEST(GraphSymbols, TtyDownHas25Entries) {
	EXPECT_EQ(Draw::graph_symbols[std::to_underlying(Draw::GraphSymbolType::tty_down)].size(), 25u);
}

// --- Graph::graphs array tests ---
// Graph::graphs is private, so we verify behavior through Graph construction.
// Default-constructed Graph should have empty, independently usable internal arrays.

TEST(GraphGraphs, DefaultConstructedGraphIsValid) {
	Draw::Graph g;
	// Default constructor should succeed -- internal std::array<vector<string>, 2> is value-initialized
	// Calling operator()() on a default Graph returns its output string (empty)
	auto& out = g();
	EXPECT_TRUE(out.empty());
}
