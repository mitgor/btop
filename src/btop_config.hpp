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
#include <utility>
#include <vector>

using std::string;
using std::vector;

//* Functions and variables for reading and writing the btop config file
namespace Config {

	//----------------------------------------------------------
	// Enum keys for type-safe, O(1) config access
	//----------------------------------------------------------

	enum class BoolKey : size_t {
		theme_background,
		truecolor,
		rounded_corners,
		proc_reversed,
		proc_tree,
		proc_colors,
		proc_gradient,
		proc_per_core,
		proc_mem_bytes,
		proc_cpu_graphs,
		proc_info_smaps,
		proc_left,
		proc_filter_kernel,
		cpu_invert_lower,
		cpu_single_graph,
		cpu_bottom,
		show_uptime,
		show_cpu_watts,
		check_temp,
		show_coretemp,
		show_cpu_freq,
		background_update,
		mem_graphs,
		mem_below_net,
		zfs_arc_cached,
		show_swap,
		swap_disk,
		show_disks,
		only_physical,
		use_fstab,
		zfs_hide_datasets,
		show_io_stat,
		io_mode,
		swap_upload_download,
		base_10_sizes,
		io_graph_combined,
		net_auto,
		net_sync,
		show_battery,
		show_battery_watts,
		vim_keys,
		tty_mode,
		disk_free_priv,
		force_tty,
		lowcolor,
		show_detailed,
		proc_filtering,
		proc_aggregate,
		pause_proc_list,
		keep_dead_proc_usage,
		proc_banner_shown,
		proc_follow_detailed,
		follow_process,
		update_following,
		should_selection_return_to_followed,
	#ifdef GPU_SUPPORT
		nvml_measure_pcie_speeds,
		rsmi_measure_pcie_speeds,
		gpu_mirror_graph,
	#endif
		terminal_sync,
		save_config_on_exit,
		disable_mouse,
		COUNT
	};

	enum class IntKey : size_t {
		update_ms,
		net_download,
		net_upload,
		detailed_pid,
		restore_detailed_pid,
		selected_pid,
		followed_pid,
		selected_depth,
		proc_start,
		proc_selected,
		proc_last_selected,
		proc_followed,
		COUNT
	};

	enum class StringKey : size_t {
		color_theme,
		shown_boxes,
		graph_symbol,
		disable_presets,
		presets,
		graph_symbol_cpu,
		graph_symbol_gpu,
		graph_symbol_mem,
		graph_symbol_net,
		graph_symbol_proc,
		proc_sorting,
		cpu_graph_upper,
		cpu_graph_lower,
		cpu_sensor,
		selected_battery,
		cpu_core_map,
		temp_scale,
	#ifdef __linux__
		freq_mode,
	#endif
		clock_format,
		custom_cpu_name,
		disks_filter,
		io_graph_speeds,
		net_iface,
		base_10_bitrate,
		log_level,
		proc_filter,
		proc_command,
		selected_name,
	#ifdef GPU_SUPPORT
		custom_gpu_name0,
		custom_gpu_name1,
		custom_gpu_name2,
		custom_gpu_name3,
		custom_gpu_name4,
		custom_gpu_name5,
		show_gpu_info,
		shown_gpus,
	#endif
		COUNT
	};

	//----------------------------------------------------------
	// Constexpr name tables for config file serialization
	//----------------------------------------------------------

	inline constexpr std::array<std::string_view, static_cast<size_t>(BoolKey::COUNT)> bool_key_names = {
		"theme_background",
		"truecolor",
		"rounded_corners",
		"proc_reversed",
		"proc_tree",
		"proc_colors",
		"proc_gradient",
		"proc_per_core",
		"proc_mem_bytes",
		"proc_cpu_graphs",
		"proc_info_smaps",
		"proc_left",
		"proc_filter_kernel",
		"cpu_invert_lower",
		"cpu_single_graph",
		"cpu_bottom",
		"show_uptime",
		"show_cpu_watts",
		"check_temp",
		"show_coretemp",
		"show_cpu_freq",
		"background_update",
		"mem_graphs",
		"mem_below_net",
		"zfs_arc_cached",
		"show_swap",
		"swap_disk",
		"show_disks",
		"only_physical",
		"use_fstab",
		"zfs_hide_datasets",
		"show_io_stat",
		"io_mode",
		"swap_upload_download",
		"base_10_sizes",
		"io_graph_combined",
		"net_auto",
		"net_sync",
		"show_battery",
		"show_battery_watts",
		"vim_keys",
		"tty_mode",
		"disk_free_priv",
		"force_tty",
		"lowcolor",
		"show_detailed",
		"proc_filtering",
		"proc_aggregate",
		"pause_proc_list",
		"keep_dead_proc_usage",
		"proc_banner_shown",
		"proc_follow_detailed",
		"follow_process",
		"update_following",
		"should_selection_return_to_followed",
	#ifdef GPU_SUPPORT
		"nvml_measure_pcie_speeds",
		"rsmi_measure_pcie_speeds",
		"gpu_mirror_graph",
	#endif
		"terminal_sync",
		"save_config_on_exit",
		"disable_mouse",
	};

	inline constexpr std::array<std::string_view, static_cast<size_t>(IntKey::COUNT)> int_key_names = {
		"update_ms",
		"net_download",
		"net_upload",
		"detailed_pid",
		"restore_detailed_pid",
		"selected_pid",
		"followed_pid",
		"selected_depth",
		"proc_start",
		"proc_selected",
		"proc_last_selected",
		"proc_followed",
	};

	inline constexpr std::array<std::string_view, static_cast<size_t>(StringKey::COUNT)> string_key_names = {
		"color_theme",
		"shown_boxes",
		"graph_symbol",
		"disable_presets",
		"presets",
		"graph_symbol_cpu",
		"graph_symbol_gpu",
		"graph_symbol_mem",
		"graph_symbol_net",
		"graph_symbol_proc",
		"proc_sorting",
		"cpu_graph_upper",
		"cpu_graph_lower",
		"cpu_sensor",
		"selected_battery",
		"cpu_core_map",
		"temp_scale",
	#ifdef __linux__
		"freq_mode",
	#endif
		"clock_format",
		"custom_cpu_name",
		"disks_filter",
		"io_graph_speeds",
		"net_iface",
		"base_10_bitrate",
		"log_level",
		"proc_filter",
		"proc_command",
		"selected_name",
	#ifdef GPU_SUPPORT
		"custom_gpu_name0",
		"custom_gpu_name1",
		"custom_gpu_name2",
		"custom_gpu_name3",
		"custom_gpu_name4",
		"custom_gpu_name5",
		"show_gpu_info",
		"shown_gpus",
	#endif
	};

	//----------------------------------------------------------
	// String-to-enum lookup functions (for config file parsing & menu)
	//----------------------------------------------------------

	inline std::optional<BoolKey> bool_key_from_name(std::string_view name) {
		for (size_t i = 0; i < bool_key_names.size(); ++i)
			if (bool_key_names[i] == name) return static_cast<BoolKey>(i);
		return std::nullopt;
	}

	inline std::optional<IntKey> int_key_from_name(std::string_view name) {
		for (size_t i = 0; i < int_key_names.size(); ++i)
			if (int_key_names[i] == name) return static_cast<IntKey>(i);
		return std::nullopt;
	}

	inline std::optional<StringKey> string_key_from_name(std::string_view name) {
		for (size_t i = 0; i < string_key_names.size(); ++i)
			if (string_key_names[i] == name) return static_cast<StringKey>(i);
		return std::nullopt;
	}

	//----------------------------------------------------------
	// Storage: enum-indexed arrays
	//----------------------------------------------------------

	extern std::filesystem::path conf_dir;
	extern std::filesystem::path conf_file;

	extern std::array<bool, static_cast<size_t>(BoolKey::COUNT)> bools;
	extern std::array<std::optional<bool>, static_cast<size_t>(BoolKey::COUNT)> boolsTmp;
	extern std::array<int, static_cast<size_t>(IntKey::COUNT)> ints;
	extern std::array<std::optional<int>, static_cast<size_t>(IntKey::COUNT)> intsTmp;
	extern std::array<string, static_cast<size_t>(StringKey::COUNT)> strings;
	extern std::array<std::optional<string>, static_cast<size_t>(StringKey::COUNT)> stringsTmp;

	const vector<string> valid_graph_symbols = { "braille", "block", "tty" };
	const vector<string> valid_graph_symbols_def = { "default", "braille", "block", "tty" };
	const vector<string> valid_boxes = {
		"cpu", "mem", "net", "proc"
#ifdef GPU_SUPPORT
		,"gpu0", "gpu1", "gpu2", "gpu3", "gpu4", "gpu5"
#endif
		};
	const vector<string> temp_scales = { "celsius", "fahrenheit", "kelvin", "rankine" };
#ifdef __linux__
	const vector<string> freq_modes = { "first", "range", "lowest", "highest", "average" };
#endif
#ifdef GPU_SUPPORT
	const vector<string> show_gpu_values = { "Auto", "On", "Off" };
#endif
    const vector<string> base_10_bitrate_values = { "Auto", "True", "False" };
	extern vector<string> current_boxes;
	extern vector<string> preset_list;
	const vector<string> disable_preset_options = { "Off", "Default", "Custom", "All" };
	extern vector<string> available_batteries;
	extern std::optional<int> current_preset;

	extern bool write_new;

	constexpr int ONE_DAY_MILLIS = 1000 * 60 * 60 * 24;

	[[nodiscard]] std::optional<std::filesystem::path> get_config_dir() noexcept;

	//* Check if string only contains space separated valid names for boxes and set current_boxes
	bool set_boxes(const string& boxes);

	bool validBoxSizes(const string& boxes);

	//* Toggle box and update config string shown_boxes
	bool toggle_box(const string& box);

	//* Parse and setup config value presets
	bool presetsValid(const string& presets);

	//* Apply selected preset
	bool apply_preset(const string& preset);

	bool _locked(const std::string_view name);

	//----------------------------------------------------------
	// Key-type query helpers (for menu runtime dispatch)
	//----------------------------------------------------------

	inline bool is_bool_key(std::string_view name) { return bool_key_from_name(name).has_value(); }
	inline bool is_int_key(std::string_view name) { return int_key_from_name(name).has_value(); }
	inline bool is_string_key(std::string_view name) { return string_key_from_name(name).has_value(); }

	//----------------------------------------------------------
	// Enum-based getters (primary API -- compile-time safe)
	//----------------------------------------------------------

	//* Return bool for config key
	inline bool getB(BoolKey key) { return bools[std::to_underlying(key)]; }

	//* Return integer for config key
	inline const int& getI(IntKey key) { return ints[std::to_underlying(key)]; }

	//* Return string for config key
	inline const string& getS(StringKey key) { return strings[std::to_underlying(key)]; }

	//----------------------------------------------------------
	// String-based getters (for menu/runtime key resolution)
	//----------------------------------------------------------

	//* Return bool for config key by name (runtime lookup)
	inline bool getB(const std::string_view name) {
		return bools[std::to_underlying(*bool_key_from_name(name))];
	}

	//* Return integer for config key by name (runtime lookup)
	inline const int& getI(const std::string_view name) {
		return ints[std::to_underlying(*int_key_from_name(name))];
	}

	//* Return string for config key by name (runtime lookup)
	inline const string& getS(const std::string_view name) {
		return strings[std::to_underlying(*string_key_from_name(name))];
	}

	string getAsString(const std::string_view name);

	extern string validError;

	bool intValid(const std::string_view name, const string& value);
	bool stringValid(const std::string_view name, const string& value);

	//----------------------------------------------------------
	// Enum-based setters (primary API -- compile-time safe)
	//----------------------------------------------------------

	void set(BoolKey key, bool value);
	void set(IntKey key, int value);
	void set(StringKey key, const string& value);

	//----------------------------------------------------------
	// String-based setters (for menu/runtime key resolution)
	//----------------------------------------------------------

	//* Set config key <name> to bool <value>
	void set(const std::string_view name, bool value);

	//* Set config key <name> to int <value>
	void set(const std::string_view name, const int value);

	//* Set config key <name> to string <value>
	void set(const std::string_view name, const string& value);

	//* Flip config key bool
	void flip(BoolKey key);

	//* Flip config key bool by name (runtime lookup)
	void flip(const std::string_view name);

	//* Lock config and cache changes until unlocked
	void lock();

	//* Unlock config and write any cached values to config
	void unlock();

	//* Load the config file from disk
	void load(const std::filesystem::path& conf_file, vector<string>& load_warnings);

	//* Write the config file to disk
	void write();

	auto get_log_file() -> std::optional<std::filesystem::path>;

	// Write default config to an in-memory buffer
	[[nodiscard]] auto current_config() -> std::string;
}
