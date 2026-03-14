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

#include <sys/resource.h>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <regex>
#include <string>

#include "btop_config.hpp"
#include "btop_shared.hpp"
#include "btop_tools.hpp"

namespace fs = std::filesystem;
namespace rng = std::ranges;

using Config::BoolKey;
using Config::IntKey;
using Config::StringKey;
using namespace Tools;

namespace Cpu {
    std::optional<std::string> container_engine;

	string trim_name(string name) {
		auto name_vec = ssplit(name);

		if ((name.contains("Xeon") or v_contains(name_vec, "Duo"s)) and v_contains(name_vec, "CPU"s)) {
			auto cpu_pos = v_index(name_vec, "CPU"s);
			if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')'))
				name = name_vec.at(cpu_pos + 1);
			else
				name.clear();
		} else if (v_contains(name_vec, "Ryzen"s)) {
			auto ryz_pos = v_index(name_vec, "Ryzen"s);
			name = "Ryzen";
			int tokens = 0;
			for (auto i = ryz_pos + 1; i < name_vec.size() && tokens < 2; i++) {
				const std::string& p = name_vec.at(i);
				if (p != "AI" && p != "PRO" && p != "H" && p != "HX")
					tokens++;
				name += " " + p;
			}
		} else if (name.contains("Intel") and v_contains(name_vec, "CPU"s)) {
			auto cpu_pos = v_index(name_vec, "CPU"s);
			if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')') and name_vec.at(cpu_pos + 1) != "@")
				name = name_vec.at(cpu_pos + 1);
			else
				name.clear();
		} else
			name.clear();

		if (name.empty() and not name_vec.empty()) {
			for (const auto &n : name_vec) {
				if (n == "@") break;
				name += n + ' ';
			}
			name.pop_back();
			for (const auto& replace : {"Processor", "CPU", "(R)", "(TM)", "Intel", "AMD", "Apple", "Core"}) {
				name = s_replace(name, replace, "");
				name = s_replace(name, "  ", " ");
			}
			name = trim(name);
		}

		return name;
	}
}

#ifdef GPU_SUPPORT
namespace Gpu {
	vector<string> gpu_names;
	vector<int> gpu_b_height_offsets;
	std::array<RingBuffer<long long>, static_cast<size_t>(SharedGpuField::COUNT)> shared_gpu_percent{};
	long long gpu_pwr_total_max = 0;
}
#endif

namespace Proc {

namespace {
	struct SortEntry {
		double key;        // unified key type (cast integers to double for comparison)
		uint32_t index;    // index into original proc_vec
	};

	constexpr int SOA_THRESHOLD = 128;  // Below this, AoS sort is fine

	//? SoA key extraction sort: extracts numeric sort keys into a contiguous array
	//? for cache-friendly comparison (4 entries/cache line vs 1 with AoS).
	//? Composes with partial_sort for display-limited views.
	void soa_sort(vector<proc_info>& proc_vec, auto projection, bool ascending, int display_count) {
		thread_local vector<SortEntry> keys;
		keys.resize(proc_vec.size());

		// Extract keys into contiguous array
		for (uint32_t i = 0; i < static_cast<uint32_t>(proc_vec.size()); ++i)
			keys[i] = {static_cast<double>(projection(proc_vec[i])), i};

		// Determine sort range
		const int n = (display_count > 0 && display_count < static_cast<int>(keys.size()))
			? display_count : static_cast<int>(keys.size());
		auto middle = keys.begin() + n;

		// Sort the compact key array
		if (ascending)
			std::partial_sort(keys.begin(), middle, keys.end(),
				[](const SortEntry& a, const SortEntry& b) { return a.key < b.key; });
		else
			std::partial_sort(keys.begin(), middle, keys.end(),
				[](const SortEntry& a, const SortEntry& b) { return a.key > b.key; });

		// Permute: build sorted front from index array
		thread_local vector<proc_info> sorted_front;
		sorted_front.clear();
		sorted_front.reserve(n);
		for (auto it = keys.begin(); it != middle; ++it)
			sorted_front.push_back(std::move(proc_vec[it->index]));

		// Move sorted entries to front of proc_vec
		for (size_t i = 0; i < sorted_front.size(); ++i)
			proc_vec[i] = std::move(sorted_front[i]);
	}
}
bool set_priority(pid_t pid, int priority) {
  if (setpriority(PRIO_PROCESS, pid, priority) == 0) {
    return true;
  }
  return false;
}

	void proc_sorter(vector<proc_info>& proc_vec, const string& sorting, bool reverse, bool tree, int display_count) {
		//? Use partial_sort when we only need the top display_count entries (O(N log K) vs O(N log N)).
		//? Falls back to stable_sort for tree mode (needs full ordering) and first cycle (display_count=0).
		const bool use_partial = (display_count > 0
								 && display_count < static_cast<int>(proc_vec.size())
								 && !tree);
		auto middle = use_partial
			? proc_vec.begin() + display_count
			: proc_vec.end();

		//? SoA sort path: for numeric keys with large process lists (>= SOA_THRESHOLD),
		//? extract keys into contiguous array for cache-friendly comparison.
		//? String keys (name, cmd, user) and tree mode bypass SoA since comparison
		//? still chases string pointers regardless of layout.
		const bool use_soa = (!tree && static_cast<int>(proc_vec.size()) >= SOA_THRESHOLD);

		if (reverse) {
			switch (v_index(sort_vector, sorting)) {
			case 0: // pid (numeric) — reverse=true means ascending
				if (use_soa) soa_sort(proc_vec, [](const proc_info& p) { return p.pid; }, true, display_count);
				else if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::less{}, &proc_info::pid);
				else rng::stable_sort(proc_vec, rng::less{}, &proc_info::pid);
				break;
			case 1: // name (string) — no SoA
				if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::greater{}, &proc_info::name);
				else rng::stable_sort(proc_vec, rng::greater{}, &proc_info::name);
				break;
			case 2: // cmd (string) — no SoA
				if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::greater{}, &proc_info::cmd);
				else rng::stable_sort(proc_vec, rng::greater{}, &proc_info::cmd);
				break;
			case 3: // threads (numeric) — reverse=true means ascending
				if (use_soa) soa_sort(proc_vec, [](const proc_info& p) { return p.threads; }, true, display_count);
				else if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::less{}, &proc_info::threads);
				else rng::stable_sort(proc_vec, rng::less{}, &proc_info::threads);
				break;
			case 4: // user (string) — no SoA
				if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::greater{}, &proc_info::user);
				else rng::stable_sort(proc_vec, rng::greater{}, &proc_info::user);
				break;
			case 5: // mem (numeric) — reverse=true means ascending
				if (use_soa) soa_sort(proc_vec, [](const proc_info& p) { return p.mem; }, true, display_count);
				else if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::less{}, &proc_info::mem);
				else rng::stable_sort(proc_vec, rng::less{}, &proc_info::mem);
				break;
			case 6: // cpu_p (numeric) — reverse=true means ascending
				if (use_soa) soa_sort(proc_vec, [](const proc_info& p) { return p.cpu_p; }, true, display_count);
				else if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::less{}, &proc_info::cpu_p);
				else rng::stable_sort(proc_vec, rng::less{}, &proc_info::cpu_p);
				break;
			case 7: // cpu_c (numeric) — reverse=true means ascending
				if (use_soa) soa_sort(proc_vec, [](const proc_info& p) { return p.cpu_c; }, true, display_count);
				else if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::less{}, &proc_info::cpu_c);
				else rng::stable_sort(proc_vec, rng::less{}, &proc_info::cpu_c);
				break;
			}
		}
		else {
			switch (v_index(sort_vector, sorting)) {
			case 0: // pid (numeric) — reverse=false means descending
				if (use_soa) soa_sort(proc_vec, [](const proc_info& p) { return p.pid; }, false, display_count);
				else if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::greater{}, &proc_info::pid);
				else rng::stable_sort(proc_vec, rng::greater{}, &proc_info::pid);
				break;
			case 1: // name (string) — no SoA
				if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::less{}, &proc_info::name);
				else rng::stable_sort(proc_vec, rng::less{}, &proc_info::name);
				break;
			case 2: // cmd (string) — no SoA
				if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::less{}, &proc_info::cmd);
				else rng::stable_sort(proc_vec, rng::less{}, &proc_info::cmd);
				break;
			case 3: // threads (numeric) — reverse=false means descending
				if (use_soa) soa_sort(proc_vec, [](const proc_info& p) { return p.threads; }, false, display_count);
				else if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::greater{}, &proc_info::threads);
				else rng::stable_sort(proc_vec, rng::greater{}, &proc_info::threads);
				break;
			case 4: // user (string) — no SoA
				if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::less{}, &proc_info::user);
				else rng::stable_sort(proc_vec, rng::less{}, &proc_info::user);
				break;
			case 5: // mem (numeric) — reverse=false means descending
				if (use_soa) soa_sort(proc_vec, [](const proc_info& p) { return p.mem; }, false, display_count);
				else if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::greater{}, &proc_info::mem);
				else rng::stable_sort(proc_vec, rng::greater{}, &proc_info::mem);
				break;
			case 6: // cpu_p (numeric) — reverse=false means descending
				if (use_soa) soa_sort(proc_vec, [](const proc_info& p) { return p.cpu_p; }, false, display_count);
				else if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::greater{}, &proc_info::cpu_p);
				else rng::stable_sort(proc_vec, rng::greater{}, &proc_info::cpu_p);
				break;
			case 7: // cpu_c (numeric) — reverse=false means descending
				if (use_soa) soa_sort(proc_vec, [](const proc_info& p) { return p.cpu_c; }, false, display_count);
				else if (use_partial) rng::partial_sort(proc_vec.begin(), middle, proc_vec.end(), rng::greater{}, &proc_info::cpu_c);
				else rng::stable_sort(proc_vec, rng::greater{}, &proc_info::cpu_c);
				break;
			}
		}

		//* When sorting with "cpu lazy" push processes over threshold cpu usage to the front regardless of cumulative usage
		if (not tree and not reverse and sorting == "cpu lazy") {
			double max = 10.0, target = 30.0;
			for (size_t i = 0, x = 0, offset = 0; i < proc_vec.size(); i++) {
				if (i <= 5 and proc_vec.at(i).cpu_p > max)
					max = proc_vec.at(i).cpu_p;
				else if (i == 6)
					target = (max > 30.0) ? max : 10.0;
				if (i == offset and proc_vec.at(i).cpu_p > 30.0)
					offset++;
				else if (proc_vec.at(i).cpu_p > target) {
					rotate(proc_vec.begin() + offset, proc_vec.begin() + i, proc_vec.begin() + i + 1);
					if (++x > 10) break;
				}
			}
		}
	}

	void tree_sort(vector<tree_proc>& proc_vec, const string& sorting, bool reverse, bool paused, int& c_index, const int index_max, bool collapsed) {
		if (proc_vec.size() > 1 and not paused) {
			if (reverse) {
				switch (v_index(sort_vector, sorting)) {
				case 3: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().threads < b.entry.get().threads; });	break;
				case 5: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().mem < b.entry.get().mem; });	break;
				case 6: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().cpu_p < b.entry.get().cpu_p; });	break;
				case 7: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().cpu_c < b.entry.get().cpu_c; });	break;
				}
			}
			else {
				switch (v_index(sort_vector, sorting)) {
				case 3: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().threads > b.entry.get().threads; });	break;
				case 5: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().mem > b.entry.get().mem; });	break;
				case 6: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().cpu_p > b.entry.get().cpu_p; });	break;
				case 7: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().cpu_c > b.entry.get().cpu_c; });	break;
				}
			}
		}

		for (auto& r : proc_vec) {
			r.entry.get().tree_index = (collapsed or r.entry.get().filtered ? index_max : c_index++);
			if (not r.children.empty()) {
				tree_sort(r.children, sorting, reverse, paused, c_index, (collapsed or r.entry.get().collapsed or r.entry.get().tree_index == (size_t)index_max));
			}
		}
	}

	auto matches_filter(const proc_info& proc, const std::string& filter) -> bool {
		if (filter.starts_with("!")) {
			if (filter.size() == 1) {
				return true;
			}

			// An incomplete regex throws, see issue https://github.com/aristocratos/btop/issues/1133
			try {
				std::regex regex { filter.substr(1), std::regex::extended };
				return std::regex_search(std::to_string(proc.pid), regex) || std::regex_search(proc.name, regex) ||
							 std::regex_match(proc.cmd, regex) || std::regex_search(proc.user, regex);
			} catch (std::regex_error& /* unused */) {
				return false;
			}
		}

		return std::to_string(proc.pid).contains(filter) || s_contains_ic(proc.name, filter) ||
					 s_contains_ic(proc.cmd, filter) || s_contains_ic(proc.user, filter);
	}

	void _tree_gen(proc_info& cur_proc, vector<proc_info>& in_procs, vector<tree_proc>& out_procs,
		int cur_depth, bool collapsed, const string& filter, bool found, bool no_update, bool should_filter) {
		bool filtering = false;

		//? If filtering, include children of matching processes
		if (not found and (should_filter or not filter.empty())) {
			if (!matches_filter(cur_proc, filter)) {
				filtering = true;
				cur_proc.filtered = true;
				filter_found++;
			}
			else {
				found = true;
				cur_depth = 0;
			}
		}
		else if (cur_proc.filtered) cur_proc.filtered = false;

		cur_proc.depth = cur_depth;

		//? Set tree index position for process if not filtered out or currently in a collapsed sub-tree
		out_procs.push_back({ cur_proc, {} });
		if (not collapsed and not filtering) {
			cur_proc.tree_index = out_procs.size() - 1;

			//? Try to find name of the binary file and append to program name if not the same
			if (cur_proc.short_cmd.empty() and not cur_proc.cmd.empty()) {
				std::string_view cmd_view = cur_proc.cmd;
				cmd_view = cmd_view.substr((size_t)0, std::min(cmd_view.find(' '), cmd_view.size()));
				cmd_view = cmd_view.substr(std::min(cmd_view.find_last_of('/') + 1, cmd_view.size()));
				cur_proc.short_cmd = string{cmd_view};
			}
		}
		else {
			cur_proc.tree_index = in_procs.size();
		}

		//? Recursive iteration over all children
		for (auto& p : rng::equal_range(in_procs, cur_proc.pid, rng::less{}, &proc_info::ppid)) {
			if (collapsed and not filtering) {
				cur_proc.filtered = true;
			}

			_tree_gen(p, in_procs, out_procs.back().children, cur_depth + 1, (collapsed or cur_proc.collapsed), filter, found, no_update, should_filter);

			if (not no_update and not filtering and (collapsed or cur_proc.collapsed)) {
				//auto& parent = cur_proc;
				if (p.state != 'X') {
					cur_proc.cpu_p += p.cpu_p;
					cur_proc.cpu_c += p.cpu_c;
					cur_proc.mem += p.mem;
					cur_proc.threads += p.threads;
				}
				filter_found++;
				p.filtered = true;
			}
			else if (Config::getB(BoolKey::proc_aggregate) and p.state != 'X') {
				cur_proc.cpu_p += p.cpu_p;
				cur_proc.cpu_c += p.cpu_c;
				cur_proc.mem += p.mem;
				cur_proc.threads += p.threads;
			}
		}
	}

	void _collect_prefixes(tree_proc &t, const bool is_last, const string &header) {
		const bool is_filtered = t.entry.get().filtered;
		if (is_filtered) t.entry.get().depth = 0;

		if (!t.children.empty()) t.entry.get().prefix = header + (t.entry.get().collapsed ? "[+]─": "[-]─");
		else t.entry.get().prefix = header + (is_last ? " └─": " ├─");

		for (auto child = t.children.begin(); child != t.children.end(); ++child) {
			_collect_prefixes(*child, child == (t.children.end() - 1),
				is_filtered ? "": header + (is_last ? "   ": " │ "));
		}
	}
}

auto detect_container() -> std::optional<std::string> {
    std::error_code err;

    if (fs::exists(fs::path("/run/.containerenv"), err)) {
        return std::make_optional(std::string { "podman" });
    }
    if (fs::exists(fs::path("/.dockerenv"), err)) {
        return std::make_optional(std::string { "docker" });
    }
    auto systemd_container = fs::path("/run/systemd/container");
    if (fs::exists(systemd_container, err)) {
        auto stream = std::ifstream { systemd_container };
        auto buf = std::string {};
        stream >> buf;
        return std::make_optional(buf);
    }

    return std::nullopt;
}
