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
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using std::array;
using std::string;
using std::vector;

namespace Theme {
	extern std::filesystem::path theme_dir;
	extern std::filesystem::path user_theme_dir;
	extern std::filesystem::path custom_theme_dir;

	//* Contains "Default" and "TTY" at indices 0 and 1, otherwise full paths to theme files
	extern vector<string> themes;

	//----------------------------------------------------------
	// Enum keys for O(1) theme color access
	//----------------------------------------------------------

	enum class ColorKey : size_t {
		// UI chrome colors
		main_bg, main_fg, title, hi_fg,
		selected_bg, selected_fg, inactive_fg,
		graph_text, meter_bg, proc_misc,
		// Box outline colors
		cpu_box, mem_box, net_box, proc_box,
		div_line,
		// Gradient triplets: temp
		temp_start, temp_mid, temp_end,
		// Gradient triplets: cpu
		cpu_start, cpu_mid, cpu_end,
		// Gradient triplets: free
		free_start, free_mid, free_end,
		// Gradient triplets: cached
		cached_start, cached_mid, cached_end,
		// Gradient triplets: available
		available_start, available_mid, available_end,
		// Gradient triplets: used
		used_start, used_mid, used_end,
		// Gradient triplets: download
		download_start, download_mid, download_end,
		// Gradient triplets: upload
		upload_start, upload_mid, upload_end,
		// Gradient triplets: process
		process_start, process_mid, process_end,
		// Proc special colors
		proc_pause_bg, proc_follow_bg, proc_banner_bg, proc_banner_fg,
		followed_bg, followed_fg,
		// Synthetic entries added by generateGradients() — NOT in Default_theme/TTY_theme
		proc_start, proc_mid, proc_end,
		proc_color_start, proc_color_mid, proc_color_end,
		COUNT
	};

	enum class GradientKey : size_t {
		temp, cpu, free, cached, available, used,
		download, upload, process,
		proc, proc_color,
		COUNT
	};

	//----------------------------------------------------------
	// Constexpr name tables for string-to-enum lookup
	//----------------------------------------------------------

	inline constexpr std::array<std::string_view, static_cast<size_t>(ColorKey::COUNT)> color_key_names = {
		"main_bg", "main_fg", "title", "hi_fg",
		"selected_bg", "selected_fg", "inactive_fg",
		"graph_text", "meter_bg", "proc_misc",
		"cpu_box", "mem_box", "net_box", "proc_box",
		"div_line",
		"temp_start", "temp_mid", "temp_end",
		"cpu_start", "cpu_mid", "cpu_end",
		"free_start", "free_mid", "free_end",
		"cached_start", "cached_mid", "cached_end",
		"available_start", "available_mid", "available_end",
		"used_start", "used_mid", "used_end",
		"download_start", "download_mid", "download_end",
		"upload_start", "upload_mid", "upload_end",
		"process_start", "process_mid", "process_end",
		"proc_pause_bg", "proc_follow_bg", "proc_banner_bg", "proc_banner_fg",
		"followed_bg", "followed_fg",
		"proc_start", "proc_mid", "proc_end",
		"proc_color_start", "proc_color_mid", "proc_color_end",
	};

	inline constexpr std::array<std::string_view, static_cast<size_t>(GradientKey::COUNT)> gradient_key_names = {
		"temp", "cpu", "free", "cached", "available", "used",
		"download", "upload", "process",
		"proc", "proc_color",
	};

	//----------------------------------------------------------
	// Gradient-to-ColorKey mapping
	//----------------------------------------------------------

	//* Map a GradientKey to the ColorKey of its _start entry
	inline constexpr ColorKey gradient_start_key(GradientKey gk) {
		constexpr std::array<ColorKey, static_cast<size_t>(GradientKey::COUNT)> starts = {
			ColorKey::temp_start, ColorKey::cpu_start, ColorKey::free_start,
			ColorKey::cached_start, ColorKey::available_start, ColorKey::used_start,
			ColorKey::download_start, ColorKey::upload_start, ColorKey::process_start,
			ColorKey::proc_start, ColorKey::proc_color_start,
		};
		return starts[static_cast<size_t>(gk)];
	}

	//----------------------------------------------------------
	// String-to-enum lookup helpers
	//----------------------------------------------------------

	std::optional<ColorKey> colorKeyFromString(std::string_view name);
	std::optional<GradientKey> gradientKeyFromString(std::string_view name);

	//----------------------------------------------------------
	// Enum-indexed storage (replaces unordered_map)
	//----------------------------------------------------------

	extern std::array<string, static_cast<size_t>(ColorKey::COUNT)> colors;
	extern std::array<std::array<int, 3>, static_cast<size_t>(ColorKey::COUNT)> rgbs;
	extern std::array<std::array<string, 101>, static_cast<size_t>(GradientKey::COUNT)> gradients;

	//* Generate escape sequence for 24-bit or 256 color and return as a string
	//* Args	hexa: ["#000000"-"#ffffff"] for color, ["#00"-"#ff"] for greyscale
	//*			t_to_256: [true|false] convert 24bit value to 256 color value
	//* 		depth: ["fg"|"bg"] for either a foreground color or a background color
	string hex_to_color(string hexa, bool t_to_256=false, const string& depth="fg");

	//* Generate escape sequence for 24-bit or 256 color and return as a string
	//* Args	r: [0-255], g: [0-255], b: [0-255]
	//*			t_to_256: [true|false] convert 24bit value to 256 color value
	//* 		depth: ["fg"|"bg"] for either a foreground color or a background color
	string dec_to_color(int r, int g, int b, bool t_to_256=false, const string& depth="fg");

	//* Update list of paths for available themes
	void updateThemes();

	//* Set current theme from current "color_theme" value in config
	void setTheme();

	//* Return escape code for color <key>
	inline const string& c(ColorKey key) { return colors[static_cast<size_t>(key)]; }

	//* Return array of escape codes for color gradient <key>
	inline const array<string, 101>& g(GradientKey key) { return gradients[static_cast<size_t>(key)]; }

	//* Return array of red, green and blue in decimal for color <key>
	inline const std::array<int, 3>& dec(ColorKey key) { return rgbs[static_cast<size_t>(key)]; }

}
