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

#include <array>
#include <atomic>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iterator>
#include <locale>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

#include <fmt/base.h>
#include <fmt/core.h>
#include <sys/statvfs.h>

#include "btop_config.hpp"
#include "btop_log.hpp"
#include "btop_shared.hpp"
#include "btop_tools.hpp"

using std::array;
using std::atomic;
using std::string_view;

namespace fs = std::filesystem;
namespace rng = std::ranges;

using namespace std::literals;
using namespace Tools;

//* Functions and variables for reading and writing the btop config file
namespace Config {

	atomic<bool> locked (false);
	atomic<bool> writelock (false);
	bool write_new;

	const vector<array<string, 2>> descriptions = {
		{"color_theme", 		"#* Name of a btop++/bpytop/bashtop formatted \".theme\" file, \"Default\" and \"TTY\" for builtin themes.\n"
								"#* Themes should be placed in \"../share/btop/themes\" relative to binary or \"$HOME/.config/btop/themes\""},

		{"theme_background", 	"#* If the theme set background should be shown, set to False if you want terminal background transparency."},

		{"truecolor", 			"#* Sets if 24-bit truecolor should be used, will convert 24-bit colors to 256 color (6x6x6 color cube) if false."},

		{"force_tty", 			"#* Set to true to force tty mode regardless if a real tty has been detected or not.\n"
								"#* Will force 16-color mode and TTY theme, set all graph symbols to \"tty\" and swap out other non tty friendly symbols."},

		{"disable_presets",		"#* Option to disable presets. Either the default preset, custom presets, or all presets.\n"
								"#* \"Off\" All presets are enabled.\n"
								"#* \"Default\" preset is disabled."
								"#* \"Custom\" presets are disabled."
								"#* \"All\" presets are disabled."},

		{"presets",				"#* Define presets for the layout of the boxes. Preset 0 is always all boxes shown with default settings. Max 9 presets.\n"
								"#* Format: \"box_name:P:G,box_name:P:G\" P=(0 or 1) for alternate positions, G=graph symbol to use for box.\n"
								"#* Use whitespace \" \" as separator between different presets.\n"
								"#* Example: \"cpu:0:default,mem:0:tty,proc:1:default cpu:0:braille,proc:0:tty\""},

		{"vim_keys",			"#* Set to True to enable \"h,j,k,l,g,G\" keys for directional control in lists.\n"
								"#* Conflicting keys for h:\"help\" and k:\"kill\" is accessible while holding shift."},

		{"disable_mouse", "#* Disable all mouse events."},
		{"rounded_corners",		"#* Rounded corners on boxes, is ignored if TTY mode is ON."},

		{"terminal_sync", 		"#* Use terminal synchronized output sequences to reduce flickering on supported terminals."},

		{"graph_symbol", 		"#* Default symbols to use for graph creation, \"braille\", \"block\" or \"tty\".\n"
								"#* \"braille\" offers the highest resolution but might not be included in all fonts.\n"
								"#* \"block\" has half the resolution of braille but uses more common characters.\n"
								"#* \"tty\" uses only 3 different symbols but will work with most fonts and should work in a real TTY.\n"
								"#* Note that \"tty\" only has half the horizontal resolution of the other two, so will show a shorter historical view."},

		{"graph_symbol_cpu", 	"# Graph symbol to use for graphs in cpu box, \"default\", \"braille\", \"block\" or \"tty\"."},
#ifdef GPU_SUPPORT
		{"graph_symbol_gpu", 	"# Graph symbol to use for graphs in gpu box, \"default\", \"braille\", \"block\" or \"tty\"."},
#endif
		{"graph_symbol_mem", 	"# Graph symbol to use for graphs in cpu box, \"default\", \"braille\", \"block\" or \"tty\"."},

		{"graph_symbol_net", 	"# Graph symbol to use for graphs in cpu box, \"default\", \"braille\", \"block\" or \"tty\"."},

		{"graph_symbol_proc", 	"# Graph symbol to use for graphs in cpu box, \"default\", \"braille\", \"block\" or \"tty\"."},

		{"shown_boxes", 		"#* Manually set which boxes to show. Available values are \"cpu mem net proc\" and \"gpu0\" through \"gpu5\", separate values with whitespace."},

		{"update_ms", 			"#* Update time in milliseconds, recommended 2000 ms or above for better sample times for graphs."},

		{"proc_sorting",		"#* Processes sorting, \"pid\" \"program\" \"arguments\" \"threads\" \"user\" \"memory\" \"cpu lazy\" \"cpu direct\",\n"
								"#* \"cpu lazy\" sorts top process over time (easier to follow), \"cpu direct\" updates top process directly."},

		{"proc_reversed",		"#* Reverse sorting order, True or False."},

		{"proc_tree",			"#* Show processes as a tree."},

		{"proc_colors", 		"#* Use the cpu graph colors in the process list."},

		{"proc_gradient", 		"#* Use a darkening gradient in the process list."},

		{"proc_per_core", 		"#* If process cpu usage should be of the core it's running on or usage of the total available cpu power."},

		{"proc_mem_bytes", 		"#* Show process memory as bytes instead of percent."},

		{"proc_cpu_graphs",     "#* Show cpu graph for each process."},

		{"proc_info_smaps",		"#* Use /proc/[pid]/smaps for memory information in the process info box (very slow but more accurate)"},

		{"proc_left",			"#* Show proc box on left side of screen instead of right."},

		{"proc_filter_kernel",  "#* (Linux) Filter processes tied to the Linux kernel(similar behavior to htop)."},

		{"proc_follow_detailed",	"#* Should the process list follow the selected process when detailed view is open."},

		{"proc_aggregate",		"#* In tree-view, always accumulate child process resources in the parent process."},

		{"keep_dead_proc_usage", "#* Should cpu and memory usage display be preserved for dead processes when paused."},

		{"cpu_graph_upper", 	"#* Sets the CPU stat shown in upper half of the CPU graph, \"total\" is always available.\n"
								"#* Select from a list of detected attributes from the options menu."},

		{"cpu_graph_lower", 	"#* Sets the CPU stat shown in lower half of the CPU graph, \"total\" is always available.\n"
								"#* Select from a list of detected attributes from the options menu."},
	#ifdef GPU_SUPPORT
		{"show_gpu_info",		"#* If gpu info should be shown in the cpu box. Available values = \"Auto\", \"On\" and \"Off\"."},
	#endif
		{"cpu_invert_lower", 	"#* Toggles if the lower CPU graph should be inverted."},

		{"cpu_single_graph", 	"#* Set to True to completely disable the lower CPU graph."},

		{"cpu_bottom",			"#* Show cpu box at bottom of screen instead of top."},

		{"show_uptime", 		"#* Shows the system uptime in the CPU box."},

		{"show_cpu_watts",		"#* Shows the CPU package current power consumption in watts. Requires running `make setcap` or `make setuid` or running with sudo."},

		{"check_temp", 			"#* Show cpu temperature."},

		{"cpu_sensor", 			"#* Which sensor to use for cpu temperature, use options menu to select from list of available sensors."},

		{"show_coretemp", 		"#* Show temperatures for cpu cores also if check_temp is True and sensors has been found."},

		{"cpu_core_map",		"#* Set a custom mapping between core and coretemp, can be needed on certain cpus to get correct temperature for correct core.\n"
								"#* Use lm-sensors or similar to see which cores are reporting temperatures on your machine.\n"
								"#* Format \"x:y\" x=core with wrong temp, y=core with correct temp, use space as separator between multiple entries.\n"
								"#* Example: \"4:0 5:1 6:3\""},

		{"temp_scale", 			"#* Which temperature scale to use, available values: \"celsius\", \"fahrenheit\", \"kelvin\" and \"rankine\"."},

		{"base_10_sizes",		"#* Use base 10 for bits/bytes sizes, KB = 1000 instead of KiB = 1024."},

		{"show_cpu_freq", 		"#* Show CPU frequency."},
	#ifdef __linux__
		{"freq_mode",				"#* How to calculate CPU frequency, available values: \"first\", \"range\", \"lowest\", \"highest\" and \"average\"."},
	#endif
		{"clock_format", 		"#* Draw a clock at top of screen, formatting according to strftime, empty string to disable.\n"
								"#* Special formatting: /host = hostname | /user = username | /uptime = system uptime"},

		{"background_update", 	"#* Update main ui in background when menus are showing, set this to false if the menus is flickering too much for comfort."},

		{"custom_cpu_name", 	"#* Custom cpu model name, empty string to disable."},

		{"disks_filter", 		"#* Optional filter for shown disks, should be full path of a mountpoint, separate multiple values with whitespace \" \".\n"
									"#* Only disks matching the filter will be shown. Prepend exclude= to only show disks not matching the filter. Examples: disk_filter=\"/boot /home/user\", disks_filter=\"exclude=/boot /home/user\""},

		{"mem_graphs", 			"#* Show graphs instead of meters for memory values."},

		{"mem_below_net",		"#* Show mem box below net box instead of above."},

		{"zfs_arc_cached",		"#* Count ZFS ARC in cached and available memory."},

		{"show_swap", 			"#* If swap memory should be shown in memory box."},

		{"swap_disk", 			"#* Show swap as a disk, ignores show_swap value above, inserts itself after first disk."},

		{"show_disks", 			"#* If mem box should be split to also show disks info."},

		{"only_physical", 		"#* Filter out non physical disks. Set this to False to include network disks, RAM disks and similar."},

		{"use_fstab", 			"#* Read disks list from /etc/fstab. This also disables only_physical."},

		{"zfs_hide_datasets",		"#* Setting this to True will hide all datasets, and only show ZFS pools. (IO stats will be calculated per-pool)"},

		{"disk_free_priv",		"#* Set to true to show available disk space for privileged users."},

		{"show_io_stat", 		"#* Toggles if io activity % (disk busy time) should be shown in regular disk usage view."},

		{"io_mode", 			"#* Toggles io mode for disks, showing big graphs for disk read/write speeds."},

		{"io_graph_combined", 	"#* Set to True to show combined read/write io graphs in io mode."},

		{"io_graph_speeds", 	"#* Set the top speed for the io graphs in MiB/s (100 by default), use format \"mountpoint:speed\" separate disks with whitespace \" \".\n"
								"#* Example: \"/mnt/media:100 /:20 /boot:1\"."},

		{"swap_upload_download", "#* Swap the positions of the upload and download speed graphs. When true, upload will be on top."},

		{"net_download", 		"#* Set fixed values for network graphs in Mebibits. Is only used if net_auto is also set to False."},

		{"net_upload", ""},

		{"net_auto", 			"#* Use network graphs auto rescaling mode, ignores any values set above and rescales down to 10 Kibibytes at the lowest."},

		{"net_sync", 			"#* Sync the auto scaling for download and upload to whichever currently has the highest scale."},

		{"net_iface", 			"#* Starts with the Network Interface specified here."},

	    {"base_10_bitrate",     "#* \"True\" shows bitrates in base 10 (Kbps, Mbps). \"False\" shows bitrates in binary sizes (Kibps, Mibps, etc.). \"Auto\" uses base_10_sizes."},

		{"show_battery", 		"#* Show battery stats in top right if battery is present."},

		{"selected_battery",	"#* Which battery to use if multiple are present. \"Auto\" for auto detection."},

		{"show_battery_watts",	"#* Show power stats of battery next to charge indicator."},

		{"log_level", 			"#* Set loglevel for \"~/.local/state/btop.log\" levels are: \"ERROR\" \"WARNING\" \"INFO\" \"DEBUG\".\n"
								"#* The level set includes all lower levels, i.e. \"DEBUG\" will show all logging info."},
		{"save_config_on_exit",  "#* Automatically save current settings to config file on exit."},
	#ifdef GPU_SUPPORT

		{"nvml_measure_pcie_speeds",
								"#* Measure PCIe throughput on NVIDIA cards, may impact performance on certain cards."},
		{"rsmi_measure_pcie_speeds",
								"#* Measure PCIe throughput on AMD cards, may impact performance on certain cards."},
		{"gpu_mirror_graph",	"#* Horizontally mirror the GPU graph."},
		{"shown_gpus",			"#* Set which GPU vendors to show. Available values are \"nvidia amd intel apple\""},
		{"custom_gpu_name0",	"#* Custom gpu0 model name, empty string to disable."},
		{"custom_gpu_name1",	"#* Custom gpu1 model name, empty string to disable."},
		{"custom_gpu_name2",	"#* Custom gpu2 model name, empty string to disable."},
		{"custom_gpu_name3",	"#* Custom gpu3 model name, empty string to disable."},
		{"custom_gpu_name4",	"#* Custom gpu4 model name, empty string to disable."},
		{"custom_gpu_name5",	"#* Custom gpu5 model name, empty string to disable."},
	#endif
	};

	//----------------------------------------------------------
	// Array-based storage initialization
	//----------------------------------------------------------

	std::array<string, static_cast<size_t>(StringKey::COUNT)> strings = []() {
		std::array<string, static_cast<size_t>(StringKey::COUNT)> a{};
		a[std::to_underlying(StringKey::color_theme)] = "Default";
		a[std::to_underlying(StringKey::shown_boxes)] = "cpu mem net proc";
		a[std::to_underlying(StringKey::graph_symbol)] = "braille";
		a[std::to_underlying(StringKey::disable_presets)] = "Off";
		a[std::to_underlying(StringKey::presets)] = "cpu:1:default,proc:0:default cpu:0:default,mem:0:default,net:0:default cpu:0:block,net:0:tty";
		a[std::to_underlying(StringKey::graph_symbol_cpu)] = "default";
		a[std::to_underlying(StringKey::graph_symbol_gpu)] = "default";
		a[std::to_underlying(StringKey::graph_symbol_mem)] = "default";
		a[std::to_underlying(StringKey::graph_symbol_net)] = "default";
		a[std::to_underlying(StringKey::graph_symbol_proc)] = "default";
		a[std::to_underlying(StringKey::proc_sorting)] = "cpu lazy";
		a[std::to_underlying(StringKey::cpu_graph_upper)] = "Auto";
		a[std::to_underlying(StringKey::cpu_graph_lower)] = "Auto";
		a[std::to_underlying(StringKey::cpu_sensor)] = "Auto";
		a[std::to_underlying(StringKey::selected_battery)] = "Auto";
		a[std::to_underlying(StringKey::cpu_core_map)] = "";
		a[std::to_underlying(StringKey::temp_scale)] = "celsius";
	#ifdef __linux__
		a[std::to_underlying(StringKey::freq_mode)] = "first";
	#endif
		a[std::to_underlying(StringKey::clock_format)] = "%X";
		a[std::to_underlying(StringKey::custom_cpu_name)] = "";
		a[std::to_underlying(StringKey::disks_filter)] = "";
		a[std::to_underlying(StringKey::io_graph_speeds)] = "";
		a[std::to_underlying(StringKey::net_iface)] = "";
		a[std::to_underlying(StringKey::base_10_bitrate)] = "Auto";
		a[std::to_underlying(StringKey::log_level)] = "WARNING";
		a[std::to_underlying(StringKey::proc_filter)] = "";
		a[std::to_underlying(StringKey::proc_command)] = "";
		a[std::to_underlying(StringKey::selected_name)] = "";
	#ifdef GPU_SUPPORT
		a[std::to_underlying(StringKey::custom_gpu_name0)] = "";
		a[std::to_underlying(StringKey::custom_gpu_name1)] = "";
		a[std::to_underlying(StringKey::custom_gpu_name2)] = "";
		a[std::to_underlying(StringKey::custom_gpu_name3)] = "";
		a[std::to_underlying(StringKey::custom_gpu_name4)] = "";
		a[std::to_underlying(StringKey::custom_gpu_name5)] = "";
		a[std::to_underlying(StringKey::show_gpu_info)] = "Auto";
		a[std::to_underlying(StringKey::shown_gpus)] = "nvidia amd intel apple";
	#endif
		return a;
	}();
	std::array<std::optional<string>, static_cast<size_t>(StringKey::COUNT)> stringsTmp{};

	std::array<bool, static_cast<size_t>(BoolKey::COUNT)> bools = []() {
		std::array<bool, static_cast<size_t>(BoolKey::COUNT)> a{};
		a[std::to_underlying(BoolKey::theme_background)] = true;
		a[std::to_underlying(BoolKey::truecolor)] = true;
		a[std::to_underlying(BoolKey::rounded_corners)] = true;
		a[std::to_underlying(BoolKey::proc_reversed)] = false;
		a[std::to_underlying(BoolKey::proc_tree)] = false;
		a[std::to_underlying(BoolKey::proc_colors)] = true;
		a[std::to_underlying(BoolKey::proc_gradient)] = true;
		a[std::to_underlying(BoolKey::proc_per_core)] = false;
		a[std::to_underlying(BoolKey::proc_mem_bytes)] = true;
		a[std::to_underlying(BoolKey::proc_cpu_graphs)] = true;
		a[std::to_underlying(BoolKey::proc_info_smaps)] = false;
		a[std::to_underlying(BoolKey::proc_left)] = false;
		a[std::to_underlying(BoolKey::proc_filter_kernel)] = false;
		a[std::to_underlying(BoolKey::cpu_invert_lower)] = true;
		a[std::to_underlying(BoolKey::cpu_single_graph)] = false;
		a[std::to_underlying(BoolKey::cpu_bottom)] = false;
		a[std::to_underlying(BoolKey::show_uptime)] = true;
		a[std::to_underlying(BoolKey::show_cpu_watts)] = true;
		a[std::to_underlying(BoolKey::check_temp)] = true;
		a[std::to_underlying(BoolKey::show_coretemp)] = true;
		a[std::to_underlying(BoolKey::show_cpu_freq)] = true;
		a[std::to_underlying(BoolKey::background_update)] = true;
		a[std::to_underlying(BoolKey::mem_graphs)] = true;
		a[std::to_underlying(BoolKey::mem_below_net)] = false;
		a[std::to_underlying(BoolKey::zfs_arc_cached)] = true;
		a[std::to_underlying(BoolKey::show_swap)] = true;
		a[std::to_underlying(BoolKey::swap_disk)] = true;
		a[std::to_underlying(BoolKey::show_disks)] = true;
		a[std::to_underlying(BoolKey::only_physical)] = true;
		a[std::to_underlying(BoolKey::use_fstab)] = true;
		a[std::to_underlying(BoolKey::zfs_hide_datasets)] = false;
		a[std::to_underlying(BoolKey::show_io_stat)] = true;
		a[std::to_underlying(BoolKey::io_mode)] = false;
		a[std::to_underlying(BoolKey::swap_upload_download)] = false;
		a[std::to_underlying(BoolKey::base_10_sizes)] = false;
		a[std::to_underlying(BoolKey::io_graph_combined)] = false;
		a[std::to_underlying(BoolKey::net_auto)] = true;
		a[std::to_underlying(BoolKey::net_sync)] = true;
		a[std::to_underlying(BoolKey::show_battery)] = true;
		a[std::to_underlying(BoolKey::show_battery_watts)] = true;
		a[std::to_underlying(BoolKey::vim_keys)] = false;
		a[std::to_underlying(BoolKey::tty_mode)] = false;
		a[std::to_underlying(BoolKey::disk_free_priv)] = false;
		a[std::to_underlying(BoolKey::force_tty)] = false;
		a[std::to_underlying(BoolKey::lowcolor)] = false;
		a[std::to_underlying(BoolKey::show_detailed)] = false;
		a[std::to_underlying(BoolKey::proc_filtering)] = false;
		a[std::to_underlying(BoolKey::proc_aggregate)] = false;
		a[std::to_underlying(BoolKey::pause_proc_list)] = false;
		a[std::to_underlying(BoolKey::keep_dead_proc_usage)] = false;
		a[std::to_underlying(BoolKey::proc_banner_shown)] = false;
		a[std::to_underlying(BoolKey::proc_follow_detailed)] = true;
		a[std::to_underlying(BoolKey::follow_process)] = false;
		a[std::to_underlying(BoolKey::update_following)] = false;
		a[std::to_underlying(BoolKey::should_selection_return_to_followed)] = false;
	#ifdef GPU_SUPPORT
		a[std::to_underlying(BoolKey::nvml_measure_pcie_speeds)] = true;
		a[std::to_underlying(BoolKey::rsmi_measure_pcie_speeds)] = true;
		a[std::to_underlying(BoolKey::gpu_mirror_graph)] = true;
	#endif
		a[std::to_underlying(BoolKey::terminal_sync)] = true;
		a[std::to_underlying(BoolKey::save_config_on_exit)] = true;
		a[std::to_underlying(BoolKey::disable_mouse)] = false;
		return a;
	}();
	std::array<std::optional<bool>, static_cast<size_t>(BoolKey::COUNT)> boolsTmp{};

	std::array<int, static_cast<size_t>(IntKey::COUNT)> ints = []() {
		std::array<int, static_cast<size_t>(IntKey::COUNT)> a{};
		a[std::to_underlying(IntKey::update_ms)] = 2000;
		a[std::to_underlying(IntKey::net_download)] = 100;
		a[std::to_underlying(IntKey::net_upload)] = 100;
		a[std::to_underlying(IntKey::detailed_pid)] = 0;
		a[std::to_underlying(IntKey::restore_detailed_pid)] = 0;
		a[std::to_underlying(IntKey::selected_pid)] = 0;
		a[std::to_underlying(IntKey::followed_pid)] = 0;
		a[std::to_underlying(IntKey::selected_depth)] = 0;
		a[std::to_underlying(IntKey::proc_start)] = 0;
		a[std::to_underlying(IntKey::proc_selected)] = 0;
		a[std::to_underlying(IntKey::proc_last_selected)] = 0;
		a[std::to_underlying(IntKey::proc_followed)] = 0;
		return a;
	}();
	std::array<std::optional<int>, static_cast<size_t>(IntKey::COUNT)> intsTmp{};

	// Returns a valid config dir or an empty optional
	// The config dir might be read only, a warning is printed, but a path is returned anyway
	[[nodiscard]] std::optional<fs::path> get_config_dir() noexcept {
		fs::path config_dir;
		{
			std::error_code error;
			if (const auto xdg_config_home = std::getenv("XDG_CONFIG_HOME"); xdg_config_home != nullptr) {
				if (fs::exists(xdg_config_home, error)) {
					config_dir = fs::path(xdg_config_home) / "btop";
				}
			} else if (const auto home = std::getenv("HOME"); home != nullptr) {
				error.clear();
				if (fs::exists(home, error)) {
					config_dir = fs::path(home) / ".config" / "btop";
				}
				if (error) {
					fmt::print(stderr, "\033[0;31mWarning: \033[0m{} could not be accessed: {}\n", config_dir.string(), error.message());
					config_dir = "";
				}
			}
		}

		// FIXME: This warnings can be noisy if the user deliberately has a non-writable config dir
		//  offer an alternative | disable messages by default | disable messages if config dir is not writable | disable messages with a flag
		// FIXME: Make happy path not branch
		if (not config_dir.empty()) {
			std::error_code error;
			if (fs::exists(config_dir, error)) {
				if (fs::is_directory(config_dir, error)) {
					struct statvfs stats {};
					if ((fs::status(config_dir, error).permissions() & fs::perms::owner_write) == fs::perms::owner_write and
						statvfs(config_dir.c_str(), &stats) == 0 and (stats.f_flag & ST_RDONLY) == 0) {
						return config_dir;
					} else {
						fmt::print(stderr, "\033[0;31mWarning: \033[0m`{}` is not writable\n", fs::absolute(config_dir).string());
						// If the config is readable we can still use the provided config, but changes will not be persistent
						if ((fs::status(config_dir, error).permissions() & fs::perms::owner_read) == fs::perms::owner_read) {
							fmt::print(stderr, "\033[0;31mWarning: \033[0mLogging is disabled, config changes are not persistent\n");
							return config_dir;
						}
					}
				} else {
					fmt::print(stderr, "\033[0;31mWarning: \033[0m`{}` is not a directory\n", fs::absolute(config_dir).string());
				}
			} else {
				// Doesn't exist
				if (fs::create_directories(config_dir, error)) {
					return config_dir;
				} else {
					fmt::print(stderr, "\033[0;31mWarning: \033[0m`{}` could not be created: {}\n", fs::absolute(config_dir).string(), error.message());
				}
			}
		} else {
			fmt::print(stderr, "\033[0;31mWarning: \033[0mCould not determine config path: Make sure `$XDG_CONFIG_HOME` or `$HOME` is set\n");
		}
		fmt::print(stderr, "\033[0;31mWarning: \033[0mLogging is disabled, config changes are not persistent\n");
		return {};
	}

	bool _locked(const std::string_view name) {
		atomic_wait(writelock, true);
		if (not write_new and rng::find_if(descriptions, [&name](const auto& a) { return a.at(0) == name; }) != descriptions.end())
			write_new = true;
		return locked.load();
	}

	fs::path conf_dir;
	fs::path conf_file;

	vector<string> available_batteries = {"Auto"};

	vector<string> current_boxes;
	vector<string> preset_list = {"cpu:0:default,mem:0:default,net:0:default,proc:0:default"};
	std::optional<int> current_preset;

	bool presetsValid(const string& presets) {
		vector<string> new_presets = {preset_list.at(0)};

		for (int x = 0; const auto& preset : ssplit(presets)) {
			if (++x > 9) {
				validError = "Too many presets entered!";
				return false;
			}
			for (int y = 0; const auto& box : ssplit(preset, ',')) {
				if (++y > 4) {
					validError = "Too many boxes entered for preset!";
					return false;
				}
				const auto& vals = ssplit(box, ':');
				if (vals.size() != 3) {
					validError = "Malformatted preset in config value presets!";
					return false;
				}
				if (not is_in(vals.at(0), "cpu", "mem", "net", "proc", "gpu0", "gpu1", "gpu2", "gpu3", "gpu4", "gpu5")) {
					validError = "Invalid box name in config value presets!";
					return false;
				}
				if (not is_in(vals.at(1), "0", "1")) {
					validError = "Invalid position value in config value presets!";
					return false;
				}
				if (not v_contains(valid_graph_symbols_def, vals.at(2))) {
					validError = "Invalid graph name in config value presets!";
					return false;
				}
			}
			new_presets.push_back(preset);
		}

		preset_list = std::move(new_presets);
		return true;
	}

	//* Apply selected preset
	bool apply_preset(const string& preset) {
		string boxes;

		for (const auto& box : ssplit(preset, ',')) {
			const auto& vals = ssplit(box, ':');
			boxes += vals.at(0) + ' ';
		}
		if (not boxes.empty()) boxes.pop_back();

		auto min_size = Term::get_min_size(boxes);
		if (Term::width < min_size.at(0) or Term::height < min_size.at(1)) {
			return false;
		}

		for (const auto& box : ssplit(preset, ',')) {
			const auto& vals = ssplit(box, ':');
			if (vals.at(0) == "cpu") {
				set(BoolKey::cpu_bottom, (vals.at(1) != "0"));
			} else if (vals.at(0) == "mem") {
				set(BoolKey::mem_below_net, (vals.at(1) != "0"));
			} else if (vals.at(0) == "proc") {
				set(BoolKey::proc_left, (vals.at(1) != "0"));
			}
			if (vals.at(0).starts_with("gpu")) {
				set(StringKey::graph_symbol_gpu, vals.at(2));
			} else {
				// Runtime string-to-enum lookup for graph_symbol_<boxname>
				const string key_name = "graph_symbol_" + vals.at(0);
				if (auto sk = string_key_from_name(key_name)) {
					set(*sk, vals.at(2));
				}
			}
		}

		if (set_boxes(boxes)) {
			set(StringKey::shown_boxes, boxes);
			return true;
		}
		return false;
	}

	void lock() {
		atomic_wait(writelock);
		locked = true;
	}

	string validError;

	bool intValid(const std::string_view name, const string& value) {
		int i_value;
		try {
			i_value = stoi(value);
		}
		catch (const std::invalid_argument&) {
			validError = "Invalid numerical value!";
			return false;
		}
		catch (const std::out_of_range&) {
			validError = "Value out of range!";
			return false;
		}
		catch (const std::exception& e) {
			validError = string{e.what()};
			return false;
		}

		if (name == "update_ms" and i_value < 100)
			validError = "Config value update_ms set too low (<100).";

		else if (name == "update_ms" and i_value > ONE_DAY_MILLIS)
			validError = fmt::format("Config value update_ms set too high (>{}).", ONE_DAY_MILLIS);

		else
			return true;

		return false;
	}

	bool validBoxSizes(const string& boxes) {
		auto min_size = Term::get_min_size(boxes);
		return (Term::width >= min_size.at(0) and Term::height >= min_size.at(1));
	}

	bool stringValid(const std::string_view name, const string& value) {
		if (name == "log_level" and not v_contains(Logger::log_levels, value))
			validError = "Invalid log_level: " + value;

		else if (name == "graph_symbol" and not v_contains(valid_graph_symbols, value))
			validError = "Invalid graph symbol identifier: " + value;

		else if (name.starts_with("graph_symbol_") and (value != "default" and not v_contains(valid_graph_symbols, value)))
			validError = fmt::format("Invalid graph symbol identifier for {}: {}", name, value);

		else if (name == "shown_boxes" and not Global::init_conf) {
			if (value.empty())
				validError = "No boxes selected!";
			else if (not validBoxSizes(value))
				validError = "Terminal too small to display entered boxes!";
			else if (not set_boxes(value))
				validError = "Invalid box name(s) in shown_boxes!";
			else
				return true;
		}

	#ifdef GPU_SUPPORT
		else if (name == "show_gpu_info" and not v_contains(show_gpu_values, value))
			validError = "Invalid value for show_gpu_info: " + value;
	#endif

		else if (name == "presets" and not presetsValid(value))
			return false;

		else if (name == "cpu_core_map") {
			const auto maps = ssplit(value);
			bool all_good = true;
			for (const auto& map : maps) {
				const auto map_split = ssplit(map, ':');
				if (map_split.size() != 2)
					all_good = false;
				else if (not isint(map_split.at(0)) or not isint(map_split.at(1)))
					all_good = false;

				if (not all_good) {
					validError = "Invalid formatting of cpu_core_map!";
					return false;
				}
			}
			return true;
		}
		else if (name == "io_graph_speeds") {
			const auto maps = ssplit(value);
			bool all_good = true;
			for (const auto& map : maps) {
				const auto map_split = ssplit(map, ':');
				if (map_split.size() != 2)
					all_good = false;
				else if (map_split.at(0).empty() or not isint(map_split.at(1)))
					all_good = false;

				if (not all_good) {
					validError = "Invalid formatting of io_graph_speeds!";
					return false;
				}
			}
			return true;
		}

		else
			return true;

		return false;
	}

	string getAsString(const std::string_view name) {
		if (auto bk = bool_key_from_name(name))
			return bools[std::to_underlying(*bk)] ? "True" : "False";
		if (auto ik = int_key_from_name(name))
			return to_string(ints[std::to_underlying(*ik)]);
		if (auto sk = string_key_from_name(name))
			return strings[std::to_underlying(*sk)];
		return "";
	}

	//----------------------------------------------------------
	// Enum-based set/flip implementations
	//----------------------------------------------------------

	void set(BoolKey key, bool value) {
		const auto name = bool_key_names[std::to_underlying(key)];
		if (_locked(name)) boolsTmp[std::to_underlying(key)] = value;
		else bools[std::to_underlying(key)] = value;
	}

	void set(IntKey key, int value) {
		const auto name = int_key_names[std::to_underlying(key)];
		if (_locked(name)) intsTmp[std::to_underlying(key)] = value;
		else ints[std::to_underlying(key)] = value;
	}

	void set(StringKey key, const string& value) {
		const auto name = string_key_names[std::to_underlying(key)];
		if (_locked(name)) stringsTmp[std::to_underlying(key)] = value;
		else strings[std::to_underlying(key)] = value;
	}

	void flip(BoolKey key) {
		const auto name = bool_key_names[std::to_underlying(key)];
		if (_locked(name)) {
			if (boolsTmp[std::to_underlying(key)].has_value())
				boolsTmp[std::to_underlying(key)] = not *boolsTmp[std::to_underlying(key)];
			else
				boolsTmp[std::to_underlying(key)] = not bools[std::to_underlying(key)];
		}
		else bools[std::to_underlying(key)] = not bools[std::to_underlying(key)];
	}

	//----------------------------------------------------------
	// String-based set/flip implementations (runtime lookup for menu)
	//----------------------------------------------------------

	void set(const std::string_view name, bool value) {
		if (auto bk = bool_key_from_name(name))
			set(*bk, value);
	}

	void set(const std::string_view name, const int value) {
		if (auto ik = int_key_from_name(name))
			set(*ik, value);
	}

	void set(const std::string_view name, const string& value) {
		if (auto sk = string_key_from_name(name))
			set(*sk, value);
	}

	void flip(const std::string_view name) {
		if (auto bk = bool_key_from_name(name))
			flip(*bk);
	}

	void unlock() {
		if (not locked) return;
		atomic_wait(Runner::active);
		atomic_lock lck(writelock, true);
		try {
			if (Proc::shown) {
				ints[std::to_underlying(IntKey::selected_pid)] = Proc::selected_pid;
				strings[std::to_underlying(StringKey::selected_name)] = Proc::selected_name;
				ints[std::to_underlying(IntKey::proc_start)] = Proc::start;
				ints[std::to_underlying(IntKey::proc_selected)] = Proc::selected;
				ints[std::to_underlying(IntKey::selected_depth)] = Proc::selected_depth;
			}

			for (size_t i = 0; i < stringsTmp.size(); ++i) {
				if (stringsTmp[i].has_value()) {
					strings[i] = std::move(*stringsTmp[i]);
					stringsTmp[i].reset();
				}
			}

			for (size_t i = 0; i < intsTmp.size(); ++i) {
				if (intsTmp[i].has_value()) {
					ints[i] = *intsTmp[i];
					intsTmp[i].reset();
				}
			}

			for (size_t i = 0; i < boolsTmp.size(); ++i) {
				if (boolsTmp[i].has_value()) {
					bools[i] = *boolsTmp[i];
					boolsTmp[i].reset();
				}
			}
		}
		catch (const std::exception& e) {
			Global::exit_error_msg = fmt::format("Exception during Config::unlock() : {}", e.what());
			clean_quit(1);
		}

		locked = false;
	}

	bool set_boxes(const string& boxes) {
		auto new_boxes = ssplit(boxes);
		for (auto& box : new_boxes) {
			if (not v_contains(valid_boxes, box)) return false;
		#ifdef GPU_SUPPORT
			if (box.starts_with("gpu")) {
				int gpu_num = stoi(box.substr(3)) + 1;
				if (gpu_num > Gpu::count) return false;
			}
		#endif
		}
		current_boxes = std::move(new_boxes);
		return true;
	}

	bool toggle_box(const string& box) {
		auto old_boxes = current_boxes;
		auto box_pos = rng::find(current_boxes, box);
		if (box_pos == current_boxes.end())
			current_boxes.push_back(box);
		else
			current_boxes.erase(box_pos);

		string new_boxes;
		if (not current_boxes.empty()) {
			for (const auto& b : current_boxes) new_boxes += b + ' ';
			new_boxes.pop_back();
		}

		auto min_size = Term::get_min_size(new_boxes);

		if (Term::width < min_size.at(0) or Term::height < min_size.at(1)) {
			current_boxes = old_boxes;
			return false;
		}

		Config::set(StringKey::shown_boxes, new_boxes);
		return true;
	}

	void load(const fs::path& conf_file, vector<string>& load_warnings) {
		std::error_code error;
		if (conf_file.empty())
			return;
		else if (not fs::exists(conf_file, error)) {
			write_new = true;
			return;
		}
		if (error) {
			return;
		}

		std::ifstream cread(conf_file);
		if (cread.good()) {
			vector<string> valid_names;
			valid_names.reserve(descriptions.size());
			for (const auto &n : descriptions)
				valid_names.push_back(n[0]);
			if (string v_string; cread.peek() != '#' or (getline(cread, v_string, '\n') and not v_string.contains(Global::Version)))
				write_new = true;
			while (not cread.eof()) {
				cread >> std::ws;
				if (cread.peek() == '#') {
					cread.ignore(SSmax, '\n');
					continue;
				}
				string name, value;
				getline(cread, name, '=');
				if (name.ends_with(' ')) name = trim(name);
				if (not v_contains(valid_names, name)) {
					cread.ignore(SSmax, '\n');
					continue;
				}
				cread >> std::ws;

				if (auto bk = bool_key_from_name(name)) {
					cread >> value;
					if (not isbool(value))
						load_warnings.push_back("Got an invalid bool value for config name: " + name);
					else
						bools[std::to_underlying(*bk)] = stobool(value);
				}
				else if (auto ik = int_key_from_name(name)) {
					cread >> value;
					if (not isint(value))
						load_warnings.push_back("Got an invalid integer value for config name: " + name);
					else if (not intValid(name, value)) {
						load_warnings.push_back(validError);
					}
					else
						ints[std::to_underlying(*ik)] = stoi(value);
				}
				else if (auto sk = string_key_from_name(name)) {
					if (cread.peek() == '"') {
						cread.ignore(1);
						getline(cread, value, '"');
					}
					else cread >> value;

					if (not stringValid(name, value))
						load_warnings.push_back(validError);
					else
						strings[std::to_underlying(*sk)] = value;
				}

				cread.ignore(SSmax, '\n');
			}

			if (not load_warnings.empty()) write_new = true;
		}
	}

	void write() {
		if (conf_file.empty() or not write_new) return;
		Logger::debug("Writing new config file");
		if (geteuid() != Global::real_uid and seteuid(Global::real_uid) != 0) return;
		std::ofstream cwrite(conf_file, std::ios::trunc);
		// TODO: Report error when stream is in a bad state.
		if (cwrite.good()) {
			cwrite << current_config();
		}
	}

	static constexpr auto get_xdg_state_dir() -> std::optional<fs::path> {
		std::optional<fs::path> xdg_state_home;

		{
			const auto* xdg_state_home_ptr = std::getenv("XDG_STATE_HOME");
			if (xdg_state_home_ptr != nullptr) {
				xdg_state_home = std::make_optional(fs::path(xdg_state_home_ptr));
			} else {
				const auto* home_ptr = std::getenv("HOME");
				if (home_ptr != nullptr) {
					xdg_state_home = std::make_optional(fs::path(home_ptr) / ".local" / "state");
				}
			}
		}

		if (xdg_state_home.has_value()) {
			std::error_code err;
			fs::create_directories(xdg_state_home.value(), err);
			if (err) {
				return std::nullopt;
			}
			return xdg_state_home;
		}
		return std::nullopt;
	}

	auto get_log_file() -> std::optional<fs::path> {
		return get_xdg_state_dir().transform([](auto&& state_home) -> auto { return state_home / "btop.log"; });
	}

	auto current_config() -> std::string {
		auto buffer = std::string {};
		fmt::format_to(std::back_inserter(buffer), "#? Config file for btop v.{}\n", Global::Version);

		for (const auto& [name, description] : descriptions) {
			// Write a description comment if available.
			fmt::format_to(std::back_inserter(buffer), "\n");
			if (!description.empty()) {
				fmt::format_to(std::back_inserter(buffer), "{}\n", description);
			}

			fmt::format_to(std::back_inserter(buffer), "{} = ", name);
			// Lookup value by name using enum tables.
			if (auto sk = string_key_from_name(name)) {
				fmt::format_to(std::back_inserter(buffer), R"("{}")", strings[std::to_underlying(*sk)]);
			} else if (auto ik = int_key_from_name(name)) {
				fmt::format_to(std::back_inserter(buffer), std::locale::classic(), "{:L}", ints[std::to_underlying(*ik)]);
			} else if (auto bk = bool_key_from_name(name)) {
				fmt::format_to(std::back_inserter(buffer), "{}", bools[std::to_underlying(*bk)] ? "true" : "false");
			}
			fmt::format_to(std::back_inserter(buffer), "\n");
		}
		return buffer;
	}
}
