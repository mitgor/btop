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

#pragma once

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "btop_shared.hpp"

using std::array;
using std::string;
using std::vector;

namespace Symbols {
	const string h_line				= "─";
	const string v_line				= "│";
	const string dotted_v_line		= "╎";
	const string left_up			= "┌";
	const string right_up			= "┐";
	const string left_down			= "└";
	const string right_down			= "┘";
	const string round_left_up		= "╭";
	const string round_right_up		= "╮";
	const string round_left_down	= "╰";
	const string round_right_down	= "╯";
	const string title_left_down	= "┘";
	const string title_right_down	= "└";
	const string title_left			= "┐";
	const string title_right		= "┌";
	const string div_right			= "┤";
	const string div_left			= "├";
	const string div_up				= "┬";
	const string div_down			= "┴";


	const string up = "↑";
	const string down = "↓";
	const string left = "←";
	const string right = "→";
	const string enter = "↵";
}

namespace Draw {

	// Graph symbol type enum -- indexes into graph_symbols array (replaces string-keyed unordered_map)
	enum class GraphSymbolType : size_t {
		braille_up, braille_down,
		block_up, block_down,
		tty_up, tty_down,
		COUNT  // = 6
	};

	// Convert runtime symbol name + invert flag to GraphSymbolType enum
	inline GraphSymbolType to_graph_symbol_type(const string& symbol, bool invert) {
		if (symbol == "braille" || symbol == "default")
			return invert ? GraphSymbolType::braille_down : GraphSymbolType::braille_up;
		if (symbol == "block")
			return invert ? GraphSymbolType::block_down : GraphSymbolType::block_up;
		// tty
		return invert ? GraphSymbolType::tty_down : GraphSymbolType::tty_up;
	}

	// Enum-indexed array of graph symbol vectors (defined in btop_draw.cpp)
	extern const std::array<vector<string>, static_cast<size_t>(GraphSymbolType::COUNT)> graph_symbols;

	// Helper: get the background fill symbol (index 6) for a given symbol name
	inline const string& graph_bg_symbol(const string& symbol_name) {
		return graph_symbols[std::to_underlying(to_graph_symbol_type(
			symbol_name == "default" ? "braille" : symbol_name, false))].at(6);
	}

	//* Generate if needed and return the btop++ banner
	string banner_gen(int y=0, int x=0, bool centered=false, bool redraw=false);

	//* An editable text field
	class TextEdit {
		size_t pos{};
		size_t upos{};
		bool numeric = false;
	public:
		string text;
		TextEdit();
		explicit TextEdit(string text, bool numeric=false);
		bool command(const std::string_view key);
		string operator()(const size_t limit=0);
		void clear();
	};

	//* Create a box and return as a string
	string createBox(
			const int x, const int y, const int width, const int height, string line_color = "", bool fill = false,
			const std::string_view title = "", const std::string_view title2 = "", const int num = 0
	);

	bool update_clock(bool force = false);

	//* Class holding a percentage meter
	class Meter {
		int width;
		string color_gradient;
		bool invert;
		array<string, 101> cache;
	public:
		Meter();
		Meter(const int width, string color_gradient, bool invert = false);

		//* Return a string representation of the meter with given value
		string operator()(int value);
	};

	//* Class holding a percentage graph
	class Graph {
		int width, height;
		string color_gradient;
		string out, symbol = "default";
		bool invert, no_zero;
		long long offset;
		long long last = 0, max_value = 0;
		bool current = true, tty_mode = false;
		std::array<vector<string>, 2> graphs{};

		//* Create two representations of the graph to switch between to represent two values for each braille character
		void _create(const RingBuffer<long long>& data, int data_offset);

	public:
		Graph();
		Graph(int width, int height,
			const string& color_gradient,
			const RingBuffer<long long>& data,
			const string& symbol="default",
			bool invert=false, bool no_zero=false,
			long long max_value=0, long long offset=0);

		//* Add last value from back of <data> and return string representation of graph
		string& operator()(const RingBuffer<long long>& data, bool data_same=false);

		//* Return string representation of graph
		string& operator()();
	};

	//* Calculate sizes of boxes, draw outlines and save to enabled boxes namespaces
	void calcSizes();
}

namespace Proc {
	extern Draw::TextEdit filter;
	extern std::unordered_map<size_t, Draw::Graph> p_graphs;
	extern std::unordered_map<size_t, int> p_counters;
}
