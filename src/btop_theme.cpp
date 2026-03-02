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

#include <cmath>
#include <fstream>
#include <unordered_map>
#include <unistd.h>

#include "btop_config.hpp"
#include "btop_log.hpp"
#include "btop_theme.hpp"
#include "btop_tools.hpp"

using std::round;
using std::stoi;
using std::to_string;
using std::vector;
using std::views::iota;

using namespace Tools;

using Config::BoolKey;
using Config::IntKey;
using Config::StringKey;

namespace fs = std::filesystem;

string Term::fg, Term::bg;
string Fx::reset = reset_base;

namespace Theme {

	fs::path theme_dir;
	fs::path user_theme_dir;
	fs::path custom_theme_dir;
	vector<string> themes;

	//----------------------------------------------------------
	// Enum-indexed storage
	//----------------------------------------------------------

	std::array<string, static_cast<size_t>(ColorKey::COUNT)> colors{};
	std::array<std::array<int, 3>, static_cast<size_t>(ColorKey::COUNT)> rgbs{};
	std::array<std::array<string, 101>, static_cast<size_t>(GradientKey::COUNT)> gradients{};

	//----------------------------------------------------------
	// String-to-enum lookup helpers
	//----------------------------------------------------------

	std::optional<ColorKey> colorKeyFromString(std::string_view name) {
		for (size_t i = 0; i < color_key_names.size(); ++i)
			if (color_key_names[i] == name) return static_cast<ColorKey>(i);
		return std::nullopt;
	}

	std::optional<GradientKey> gradientKeyFromString(std::string_view name) {
		for (size_t i = 0; i < gradient_key_names.size(); ++i)
			if (gradient_key_names[i] == name) return static_cast<GradientKey>(i);
		return std::nullopt;
	}

	//----------------------------------------------------------
	// Default and TTY theme data (ColorKey-indexed)
	//----------------------------------------------------------

	struct ThemeEntry {
		ColorKey key;
		const char* value;
	};

	//? Number of entries in Default_theme/TTY_theme (gradient triplet colors + UI chrome)
	static constexpr size_t kThemeEntryCount = 39;

	static constexpr std::array<ThemeEntry, kThemeEntryCount> Default_theme = {{
		{ ColorKey::main_bg, "#00" },
		{ ColorKey::main_fg, "#cc" },
		{ ColorKey::title, "#ee" },
		{ ColorKey::hi_fg, "#b54040" },
		{ ColorKey::selected_bg, "#6a2f2f" },
		{ ColorKey::selected_fg, "#ee" },
		{ ColorKey::inactive_fg, "#40" },
		{ ColorKey::graph_text, "#60" },
		{ ColorKey::meter_bg, "#40" },
		{ ColorKey::proc_misc, "#0de756" },
		{ ColorKey::cpu_box, "#556d59" },
		{ ColorKey::mem_box, "#6c6c4b" },
		{ ColorKey::net_box, "#5c588d" },
		{ ColorKey::proc_box, "#805252" },
		{ ColorKey::div_line, "#30" },
		{ ColorKey::temp_start, "#4897d4" },
		{ ColorKey::temp_mid, "#5474e8" },
		{ ColorKey::temp_end, "#ff40b6" },
		{ ColorKey::cpu_start, "#77ca9b" },
		{ ColorKey::cpu_mid, "#cbc06c" },
		{ ColorKey::cpu_end, "#dc4c4c" },
		{ ColorKey::free_start, "#384f21" },
		{ ColorKey::free_mid, "#b5e685" },
		{ ColorKey::free_end, "#dcff85" },
		{ ColorKey::cached_start, "#163350" },
		{ ColorKey::cached_mid, "#74e6fc" },
		{ ColorKey::cached_end, "#26c5ff" },
		{ ColorKey::available_start, "#4e3f0e" },
		{ ColorKey::available_mid, "#ffd77a" },
		{ ColorKey::available_end, "#ffb814" },
		{ ColorKey::used_start, "#592b26" },
		{ ColorKey::used_mid, "#d9626d" },
		{ ColorKey::used_end, "#ff4769" },
		{ ColorKey::download_start, "#291f75" },
		{ ColorKey::download_mid, "#4f43a3" },
		{ ColorKey::download_end, "#b0a9de" },
		{ ColorKey::upload_start, "#620665" },
		{ ColorKey::upload_mid, "#7d4180" },
		{ ColorKey::upload_end, "#dcafde" },
	}};

	//? Additional entries not in TTY_theme but present in Default_theme
	static constexpr size_t kExtraEntryCount = 12;
	static constexpr std::array<ThemeEntry, kExtraEntryCount> Default_theme_extras = {{
		{ ColorKey::process_start, "#80d0a3" },
		{ ColorKey::process_mid, "#dcd179" },
		{ ColorKey::process_end, "#d45454" },
		{ ColorKey::proc_pause_bg, "#b54040" },
		{ ColorKey::proc_follow_bg, "#4040b5" },
		{ ColorKey::proc_banner_bg, "#7b407b" },
		{ ColorKey::proc_banner_fg, "#ee" },
		{ ColorKey::followed_bg, "#4040b5" },
		{ ColorKey::followed_fg, "#ee" },
		// Sentinel/padding for completeness — not used
		{ ColorKey::proc_start, "" },
		{ ColorKey::proc_mid, "" },
		{ ColorKey::proc_end, "" },
	}};

	static constexpr std::array<ThemeEntry, kThemeEntryCount> TTY_theme = {{
		{ ColorKey::main_bg, "\x1b[0;40m" },
		{ ColorKey::main_fg, "\x1b[37m" },
		{ ColorKey::title, "\x1b[97m" },
		{ ColorKey::hi_fg, "\x1b[91m" },
		{ ColorKey::selected_bg, "\x1b[41m" },
		{ ColorKey::selected_fg, "\x1b[97m" },
		{ ColorKey::inactive_fg, "\x1b[90m" },
		{ ColorKey::graph_text, "\x1b[90m" },
		{ ColorKey::meter_bg, "\x1b[90m" },
		{ ColorKey::proc_misc, "\x1b[92m" },
		{ ColorKey::cpu_box, "\x1b[32m" },
		{ ColorKey::mem_box, "\x1b[33m" },
		{ ColorKey::net_box, "\x1b[35m" },
		{ ColorKey::proc_box, "\x1b[31m" },
		{ ColorKey::div_line, "\x1b[90m" },
		{ ColorKey::temp_start, "\x1b[94m" },
		{ ColorKey::temp_mid, "\x1b[96m" },
		{ ColorKey::temp_end, "\x1b[95m" },
		{ ColorKey::cpu_start, "\x1b[92m" },
		{ ColorKey::cpu_mid, "\x1b[93m" },
		{ ColorKey::cpu_end, "\x1b[91m" },
		{ ColorKey::free_start, "\x1b[32m" },
		{ ColorKey::free_mid, "" },
		{ ColorKey::free_end, "\x1b[92m" },
		{ ColorKey::cached_start, "\x1b[36m" },
		{ ColorKey::cached_mid, "" },
		{ ColorKey::cached_end, "\x1b[96m" },
		{ ColorKey::available_start, "\x1b[33m" },
		{ ColorKey::available_mid, "" },
		{ ColorKey::available_end, "\x1b[93m" },
		{ ColorKey::used_start, "\x1b[31m" },
		{ ColorKey::used_mid, "" },
		{ ColorKey::used_end, "\x1b[91m" },
		{ ColorKey::download_start, "\x1b[34m" },
		{ ColorKey::download_mid, "" },
		{ ColorKey::download_end, "\x1b[94m" },
		{ ColorKey::upload_start, "\x1b[35m" },
		{ ColorKey::upload_mid, "" },
		{ ColorKey::upload_end, "\x1b[95m" },
	}};

	static constexpr std::array<ThemeEntry, kExtraEntryCount> TTY_theme_extras = {{
		{ ColorKey::process_start, "\x1b[32m" },
		{ ColorKey::process_mid, "\x1b[33m" },
		{ ColorKey::process_end, "\x1b[31m" },
		{ ColorKey::proc_pause_bg, "\x1b[41m" },
		{ ColorKey::proc_follow_bg, "\x1b[44m" },
		{ ColorKey::proc_banner_bg, "\x1b[45m" },
		{ ColorKey::proc_banner_fg, "\x1b[97m" },
		{ ColorKey::followed_bg, "\x1b[44m" },
		{ ColorKey::followed_fg, "\x1b[97m" },
		// Sentinel/padding — not used
		{ ColorKey::proc_start, "" },
		{ ColorKey::proc_mid, "" },
		{ ColorKey::proc_end, "" },
	}};

	//? Set of ColorKeys that are valid in theme files (for loadFile validation)
	static bool isValidThemeKey(std::string_view name) {
		auto key = colorKeyFromString(name);
		if (not key.has_value()) return false;
		// Only the 38 base + 9 extra keys are valid in theme files (not synthetic proc/proc_color)
		const auto k = *key;
		return k < ColorKey::proc_start; // proc_start..proc_color_end are synthetic
	}

	namespace {
		//* Convert 24-bit colors to 256 colors
		int truecolor_to_256(const int& r, const int& g, const int& b) {
			//? Use upper 232-255 greyscale values if the downscaled red, green and blue are the same value
			if (const int red = round((double)r / 11); red == round((double)g / 11) and red == round((double)b / 11)) {
				return 232 + red;
			}
			//? Else use 6x6x6 color cube to calculate approximate colors
			else {
				return round((double)r / 51) * 36 + round((double)g / 51) * 6 + round((double)b / 51) + 16;
			}
		}
	}

	string hex_to_color(string hexa, bool t_to_256, const string& depth) {
		if (hexa.size() > 1) {
			hexa.erase(0, 1);
			for (auto& c : hexa) {
				if (not isxdigit(c)) {
					Logger::error("Invalid hex value: {}", hexa);
					return "";
				}
			}
			string pre = Fx::e + (depth == "fg" ? "38" : "48") + ";" + (t_to_256 ? "5;" : "2;");

			if (hexa.size() == 2) {
				int h_int = stoi(hexa, nullptr, 16);
				if (t_to_256) {
					return pre + to_string(truecolor_to_256(h_int, h_int, h_int)) + "m";
				} else {
					string h_str = to_string(h_int);
					return pre + h_str + ";" + h_str + ";" + h_str + "m";
				}
			}
			else if (hexa.size() == 6) {
				if (t_to_256) {
					return pre + to_string(truecolor_to_256(
						stoi(hexa.substr(0, 2), nullptr, 16),
						stoi(hexa.substr(2, 2), nullptr, 16),
						stoi(hexa.substr(4, 2), nullptr, 16))) + "m";
				} else {
					return pre +
						to_string(stoi(hexa.substr(0, 2), nullptr, 16)) + ";" +
						to_string(stoi(hexa.substr(2, 2), nullptr, 16)) + ";" +
						to_string(stoi(hexa.substr(4, 2), nullptr, 16)) + "m";
				}
			}
			else Logger::error("Invalid size of hex value: {}", hexa);
		}
		else Logger::error("Hex value missing: {}", hexa);
		return "";
	}

	string dec_to_color(int r, int g, int b, bool t_to_256, const string& depth) {
		string pre = Fx::e + (depth == "fg" ? "38" : "48") + ";" + (t_to_256 ? "5;" : "2;");
		r = std::clamp(r, 0, 255);
		g = std::clamp(g, 0, 255);
		b = std::clamp(b, 0, 255);
		if (t_to_256) return pre + to_string(truecolor_to_256(r, g, b)) + "m";
		else return pre + to_string(r) + ";" + to_string(g) + ";" + to_string(b) + "m";
	}

	namespace {
		//* Convert hex color to a array of decimals
		array<int, 3> hex_to_dec(string hexa) {
			if (hexa.size() > 1) {
				hexa.erase(0, 1);
				for (auto& c : hexa) {
					if (not isxdigit(c))
						return array{-1, -1, -1};
				}

				if (hexa.size() == 2) {
					int h_int = stoi(hexa, nullptr, 16);
					return array{h_int, h_int, h_int};
				}
				else if (hexa.size() == 6) {
						return array{
							stoi(hexa.substr(0, 2), nullptr, 16),
							stoi(hexa.substr(2, 2), nullptr, 16),
							stoi(hexa.substr(4, 2), nullptr, 16)
						};
				}
			}
			return {-1 ,-1 ,-1};
		}

		//? Helper: index into colors/rgbs arrays
		inline constexpr size_t ci(ColorKey k) { return static_cast<size_t>(k); }

		//* Generate colors and rgb decimal vectors for the theme
		//* source: string-keyed map from theme file parsing (or empty for Default)
		void generateColors(const std::unordered_map<string, string>& source) {
			vector<string> t_rgb;
			string depth;
			bool t_to_256 = Config::getB(BoolKey::lowcolor);

			//? Clear arrays
			colors.fill({});
			for (auto& rgb : rgbs) rgb = {0, 0, 0};

			//? Track which color keys have been set (for fallback logic)
			std::array<bool, static_cast<size_t>(ColorKey::COUNT)> color_set{};

			//? Process base theme entries (38 keys shared between Default and custom themes)
			auto processEntry = [&](ColorKey key, const char* default_value) {
				const auto name = color_key_names[ci(key)];
				const string name_str{name};

				if (key == ColorKey::main_bg and not Config::getB(BoolKey::theme_background)) {
					colors[ci(key)] = "\x1b[49m";
					rgbs[ci(key)] = {-1, -1, -1};
					color_set[ci(key)] = true;
					return;
				}
				depth = (name.ends_with("bg") and key != ColorKey::meter_bg) ? "bg" : "fg";
				if (source.contains(name_str)) {
					const auto& src_value = source.at(name_str);
					if (key == ColorKey::main_bg and src_value.empty()) {
						colors[ci(key)] = "\x1b[49m";
						rgbs[ci(key)] = {-1, -1, -1};
						color_set[ci(key)] = true;
						return;
					}
					else if (src_value.empty() and (name.ends_with("_mid") or name.ends_with("_end"))) {
						colors[ci(key)] = "";
						rgbs[ci(key)] = {-1, -1, -1};
						color_set[ci(key)] = true;
						return;
					}
					else if (src_value.starts_with('#')) {
						colors[ci(key)] = hex_to_color(string(src_value), t_to_256, depth);
						rgbs[ci(key)] = hex_to_dec(string(src_value));
						color_set[ci(key)] = true;
					}
					else if (not src_value.empty()) {
						t_rgb = ssplit(src_value);
						if (t_rgb.size() != 3) {
							Logger::error("Invalid RGB decimal value: \"{}\"", src_value);
						} else {
							colors[ci(key)] = dec_to_color(stoi(t_rgb[0]), stoi(t_rgb[1]), stoi(t_rgb[2]), t_to_256, depth);
							rgbs[ci(key)] = array{stoi(t_rgb[0]), stoi(t_rgb[1]), stoi(t_rgb[2])};
							color_set[ci(key)] = true;
						}
					}
				}
				//? Use default value if not set, unless it's an optional color
				if (not color_set[ci(key)] and
					key != ColorKey::meter_bg and
					key != ColorKey::process_start and key != ColorKey::process_mid and key != ColorKey::process_end and
					key != ColorKey::graph_text) {
					Logger::debug("Missing color value for \"{}\". Using value from default.", name);
					colors[ci(key)] = hex_to_color(string(default_value), t_to_256, depth);
					rgbs[ci(key)] = hex_to_dec(string(default_value));
					color_set[ci(key)] = true;
				}
			};

			//? Process base entries
			for (const auto& [key, value] : Default_theme) {
				processEntry(key, value);
			}

			//? Process extra entries (process colors, proc special colors, followed)
			for (const auto& [key, value] : Default_theme_extras) {
				if (key >= ColorKey::proc_start) break; // Skip synthetic entries
				processEntry(key, value);
			}

			//? Set fallback values for optional colors not defined in theme file
			if (not color_set[ci(ColorKey::meter_bg)]) {
				colors[ci(ColorKey::meter_bg)] = colors[ci(ColorKey::inactive_fg)];
				rgbs[ci(ColorKey::meter_bg)] = rgbs[ci(ColorKey::inactive_fg)];
			}
			if (not color_set[ci(ColorKey::process_start)]) {
				colors[ci(ColorKey::process_start)] = colors[ci(ColorKey::cpu_start)];
				colors[ci(ColorKey::process_mid)] = colors[ci(ColorKey::cpu_mid)];
				colors[ci(ColorKey::process_end)] = colors[ci(ColorKey::cpu_end)];
				rgbs[ci(ColorKey::process_start)] = rgbs[ci(ColorKey::cpu_start)];
				rgbs[ci(ColorKey::process_mid)] = rgbs[ci(ColorKey::cpu_mid)];
				rgbs[ci(ColorKey::process_end)] = rgbs[ci(ColorKey::cpu_end)];
			}
			if (not color_set[ci(ColorKey::graph_text)]) {
				colors[ci(ColorKey::graph_text)] = colors[ci(ColorKey::inactive_fg)];
				rgbs[ci(ColorKey::graph_text)] = rgbs[ci(ColorKey::inactive_fg)];
			}
		}

		//* Generate color gradients from two or three colors, 101 values indexed 0-100
		void generateGradients() {
			//? Clear gradient arrays
			for (auto& grad : gradients) grad.fill({});

			bool t_to_256 = Config::getB(BoolKey::lowcolor);

			//? Set synthetic proc/proc_color RGB values (derived from other colors)
			rgbs[ci(ColorKey::proc_start)] = rgbs[ci(ColorKey::main_fg)];
			rgbs[ci(ColorKey::proc_mid)] = {-1, -1, -1};
			rgbs[ci(ColorKey::proc_end)] = rgbs[ci(ColorKey::inactive_fg)];
			rgbs[ci(ColorKey::proc_color_start)] = rgbs[ci(ColorKey::inactive_fg)];
			rgbs[ci(ColorKey::proc_color_mid)] = {-1, -1, -1};
			rgbs[ci(ColorKey::proc_color_end)] = rgbs[ci(ColorKey::process_start)];

			//? Generate gradient for each GradientKey
			for (size_t gk = 0; gk < static_cast<size_t>(GradientKey::COUNT); ++gk) {
				const auto start_key = gradient_start_key(static_cast<GradientKey>(gk));
				const size_t start_idx = static_cast<size_t>(start_key);

				//? input_colors[start,mid,end][red,green,blue]
				const array<array<int, 3>, 3> input_colors = {
					rgbs[start_idx],
					rgbs[start_idx + 1],
					rgbs[start_idx + 2]
				};

				//? output_colors[red,green,blue][0-100]
				array<array<int, 3>, 101> output_colors;
				output_colors[0][0] = -1;

				//? Only start iteration if gradient has an end color defined
				if (input_colors[2][0] >= 0) {

					//? Split iteration in two passes of 50 + 51 instead of one pass of 101 if gradient has start, mid and end values defined
					int current_range = (input_colors[1][0] >= 0) ? 50 : 100;
					for (int rgb : iota(0, 3)) {
						int start = 0, offset = 0;
						int end = (current_range == 50) ? 1 : 2;
						for (int i : iota(0, 101)) {
							output_colors[i][rgb] = input_colors[start][rgb] + (i - offset) * (input_colors[end][rgb] - input_colors[start][rgb]) / current_range;

							//? Switch source arrays from start->mid to mid->end at 50 passes if mid is defined
							if (i == current_range) { ++start; ++end; offset = 50; }
						}
					}
				}
				//? Generate color escape codes for the generated rgb decimals
				array<string, 101> color_gradient;
				if (output_colors[0][0] != -1) {
					for (int y = 0; const auto& [red, green, blue] : output_colors)
						color_gradient[y++] = dec_to_color(red, green, blue, t_to_256);
				}
				else {
					//? If only start was defined fill array with start color
					color_gradient.fill(colors[start_idx]);
				}
				gradients[gk] = std::move(color_gradient);
			}
		}

		//* Set colors and generate gradients for the TTY theme
		void generateTTYColors() {
			//? Clear arrays
			colors.fill({});
			for (auto& rgb : rgbs) rgb = {0, 0, 0};
			for (auto& grad : gradients) grad.fill({});

			//? Populate colors from TTY_theme entries
			for (const auto& [key, value] : TTY_theme)
				colors[ci(key)] = value;
			for (const auto& [key, value] : TTY_theme_extras) {
				if (key >= ColorKey::proc_start) break;
				colors[ci(key)] = value;
			}

			if (not Config::getB(BoolKey::theme_background))
				colors[ci(ColorKey::main_bg)] = "\x1b[49m";

			//? Generate TTY gradients from color triplets
			for (size_t gk = 0; gk < static_cast<size_t>(GradientKey::COUNT); ++gk) {
				//? Only process gradients whose _start color is defined in TTY theme
				const auto gradient_key = static_cast<GradientKey>(gk);
				//? Skip synthetic proc/proc_color gradients — TTY doesn't define them
				if (gradient_key == GradientKey::proc or gradient_key == GradientKey::proc_color)
					continue;

				const auto start_key = gradient_start_key(gradient_key);
				const size_t start_idx = static_cast<size_t>(start_key);
				const auto& start_color = colors[start_idx];
				const auto& mid_color = colors[start_idx + 1];
				const auto& end_color = colors[start_idx + 2];

				string section_suffix = "_start";
				int split = mid_color.empty() ? 50 : 33;
				for (int i : iota(0, 101)) {
					if (section_suffix == "_start")
						gradients[gk][i] = start_color;
					else if (section_suffix == "_mid")
						gradients[gk][i] = mid_color;
					else
						gradients[gk][i] = end_color;

					if (i == split) {
						section_suffix = (split == 33) ? "_mid" : "_end";
						split *= 2;
					}
				}
			}
		}

		//* Load a .theme file from disk
		auto loadFile(const string& filename) {
			const fs::path filepath = filename;
			std::unordered_map<string, string> empty_map;
			if (not fs::exists(filepath))
				return empty_map;

			std::ifstream themefile(filepath);
			if (themefile.good()) {
				std::unordered_map<string, string> theme_out;
				Logger::debug("Loading theme file: {}", filename);
				while (not themefile.bad()) {
					if (themefile.peek() == '#') {
						themefile.ignore(SSmax, '\n');
						continue;
					}
					themefile.ignore(SSmax, '[');
					if (themefile.eof()) break;
					string name, value;
					getline(themefile, name, ']');
					if (not isValidThemeKey(name)) {
						themefile.ignore(SSmax, '\n');
						continue;
					}
					themefile.ignore(SSmax, '=');
					themefile >> std::ws;
					if (themefile.eof()) break;
					if (themefile.peek() == '"') {
						themefile.ignore(1);
						getline(themefile, value, '"');
						themefile.ignore(SSmax, '\n');
					}
					else getline(themefile, value, '\n');

					theme_out[name] = value;
				}
				return theme_out;
			}
			return empty_map;
		}
	}

	void updateThemes() {
		themes.clear();
		themes.push_back("Default");
		themes.push_back("TTY");

		//? Priority: custom_theme_dir -> user_theme_dir -> theme_dir
		for (const auto& path : { custom_theme_dir, user_theme_dir, theme_dir } ) {
			if (path.empty()) continue;
			for (auto& file : fs::directory_iterator(path)) {
				if (file.path().extension() == ".theme" and access(file.path().c_str(), R_OK) != -1 and not v_contains(themes, file.path().c_str())) {
					themes.push_back(file.path().c_str());
				}
			}
		}

	}

	void setTheme() {
		const auto& theme = Config::getS(StringKey::color_theme);
		fs::path theme_path;
		for (const fs::path p : themes) {
			if (p == theme or p.stem() == theme or p.filename() == theme) {
				theme_path = p;
				break;
			}
		}
		if (theme == "TTY" or Config::getB(BoolKey::tty_mode))
			generateTTYColors();
		else {
			const auto source = (theme == "Default" or theme_path.empty())
				? std::unordered_map<string, string>{}
				: loadFile(theme_path);
			generateColors(source);
			generateGradients();
		}
		Term::fg = colors[ci(ColorKey::main_fg)];
		Term::bg = colors[ci(ColorKey::main_bg)];
		Fx::reset = Fx::reset_base + Term::fg + Term::bg;
	}

}
