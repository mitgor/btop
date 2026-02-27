// SPDX-License-Identifier: Apache-2.0
//
// Micro-benchmarks comparing old (unordered_map) vs new (enum-indexed array)
// data structure access patterns for Phase 4: Data Structure Modernization.
//
// These benchmarks demonstrate that enum-indexed std::array lookups are faster
// than unordered_map string-keyed lookups for the fixed-key patterns used in
// btop's CPU fields, graph symbols, Config values, and ring buffers.
//
// Self-contained: does NOT require btop runtime initialization.

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <array>
#include <cstring>
#include <deque>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "btop_shared.hpp"

int main(int argc, char* argv[]) {
	bool json_output = false;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--json") == 0) {
			json_output = true;
		}
	}

	// =========================================================================
	// Group 1: CPU Field Lookup (11 keys)
	//   Old: unordered_map<string, long long> keyed by field name
	//   New: std::array<long long, 11> indexed by CpuField enum
	// =========================================================================

	ankerl::nanobench::Bench bench_cpu;
	bench_cpu.warmup(500);
	bench_cpu.minEpochIterations(5000);
	bench_cpu.relative(true);
	bench_cpu.title("CPU Field Lookup (11 keys)");

	if (json_output) {
		bench_cpu.output(nullptr);
	}

	// Set up old-style map
	std::unordered_map<std::string, long long> cpu_map_old;
	cpu_map_old["total"] = 95;
	cpu_map_old["user"] = 45;
	cpu_map_old["nice"] = 2;
	cpu_map_old["system"] = 30;
	cpu_map_old["idle"] = 5;
	cpu_map_old["iowait"] = 3;
	cpu_map_old["irq"] = 1;
	cpu_map_old["softirq"] = 2;
	cpu_map_old["steal"] = 0;
	cpu_map_old["guest"] = 5;
	cpu_map_old["guest_nice"] = 2;

	// Set up new-style enum-indexed array
	constexpr size_t CPU_COUNT = static_cast<size_t>(CpuField::COUNT);
	std::array<long long, CPU_COUNT> cpu_array_new{};
	cpu_array_new[static_cast<size_t>(CpuField::total)] = 95;
	cpu_array_new[static_cast<size_t>(CpuField::user)] = 45;
	cpu_array_new[static_cast<size_t>(CpuField::nice)] = 2;
	cpu_array_new[static_cast<size_t>(CpuField::system)] = 30;
	cpu_array_new[static_cast<size_t>(CpuField::idle)] = 5;
	cpu_array_new[static_cast<size_t>(CpuField::iowait)] = 3;
	cpu_array_new[static_cast<size_t>(CpuField::irq)] = 1;
	cpu_array_new[static_cast<size_t>(CpuField::softirq)] = 2;
	cpu_array_new[static_cast<size_t>(CpuField::steal)] = 0;
	cpu_array_new[static_cast<size_t>(CpuField::guest)] = 5;
	cpu_array_new[static_cast<size_t>(CpuField::guest_nice)] = 2;

	// String keys for old-style iteration
	const std::array<std::string, 11> cpu_keys = {
		"total", "user", "nice", "system", "idle",
		"iowait", "irq", "softirq", "steal", "guest", "guest_nice"
	};

	bench_cpu.run("unordered_map<string,ll> read all 11", [&] {
		long long sum = 0;
		for (const auto& key : cpu_keys) {
			sum += cpu_map_old[key];
		}
		ankerl::nanobench::doNotOptimizeAway(sum);
	});

	bench_cpu.run("array<ll,11>[CpuField] read all 11", [&] {
		long long sum = 0;
		for (size_t i = 0; i < CPU_COUNT; ++i) {
			sum += cpu_array_new[i];
		}
		ankerl::nanobench::doNotOptimizeAway(sum);
	});

	// =========================================================================
	// Group 2: Graph Symbol Lookup (6 keys)
	//   Old: unordered_map<string, vector<string>> keyed by symbol name
	//   New: std::array<vector<string>, 6> indexed by GraphSymbolType enum
	// =========================================================================

	ankerl::nanobench::Bench bench_graph;
	bench_graph.warmup(500);
	bench_graph.minEpochIterations(5000);
	bench_graph.relative(true);
	bench_graph.title("Graph Symbol Lookup (6 keys)");

	if (json_output) {
		bench_graph.output(nullptr);
	}

	// Set up old-style map
	std::unordered_map<std::string, std::vector<std::string>> graph_map_old;
	const std::array<std::string, 6> graph_keys = {
		"braille_up", "braille_down", "block_up", "block_down", "tty_up", "tty_down"
	};
	for (const auto& key : graph_keys) {
		graph_map_old[key] = {"sym0", "sym1", "sym2", "sym3"};
	}

	// Set up new-style enum-indexed array
	constexpr size_t GRAPH_COUNT = 6;
	std::array<std::vector<std::string>, GRAPH_COUNT> graph_array_new;
	for (size_t i = 0; i < GRAPH_COUNT; ++i) {
		graph_array_new[i] = {"sym0", "sym1", "sym2", "sym3"};
	}

	bench_graph.run("unordered_map<string,vec> lookup 6", [&] {
		size_t total_size = 0;
		for (const auto& key : graph_keys) {
			total_size += graph_map_old[key].size();
		}
		ankerl::nanobench::doNotOptimizeAway(total_size);
	});

	bench_graph.run("array<vec,6>[GraphSymbolType] lookup 6", [&] {
		size_t total_size = 0;
		for (size_t i = 0; i < GRAPH_COUNT; ++i) {
			total_size += graph_array_new[i].size();
		}
		ankerl::nanobench::doNotOptimizeAway(total_size);
	});

	// =========================================================================
	// Group 3: Config Bool Lookup (20 representative keys)
	//   Old: unordered_map<string_view, bool> with string key lookup
	//   New: std::array<bool, 61> indexed by BoolKey enum
	// =========================================================================

	ankerl::nanobench::Bench bench_config;
	bench_config.warmup(500);
	bench_config.minEpochIterations(5000);
	bench_config.relative(true);
	bench_config.title("Config Bool Lookup (20 keys)");

	if (json_output) {
		bench_config.output(nullptr);
	}

	// Set up old-style map with representative bool config keys
	const std::array<std::string_view, 20> config_keys = {
		"theme_background", "truecolor", "rounded_corners", "proc_reversed",
		"proc_tree", "proc_colors", "proc_gradient", "proc_per_core",
		"cpu_invert_lower", "cpu_single_graph", "show_uptime", "check_temp",
		"show_coretemp", "mem_graphs", "show_swap", "show_disks",
		"net_auto", "net_sync", "show_battery", "vim_keys"
	};

	std::unordered_map<std::string_view, bool> config_map_old;
	for (size_t i = 0; i < config_keys.size(); ++i) {
		config_map_old[config_keys[i]] = (i % 2 == 0);
	}

	// Set up new-style enum-indexed array (61 bools)
	constexpr size_t CONFIG_BOOL_COUNT = 61;
	std::array<bool, CONFIG_BOOL_COUNT> config_array_new{};
	// Representative enum indices for the 20 keys above
	const std::array<size_t, 20> config_enum_indices = {
		0, 1, 2, 3, 4, 5, 6, 7,      // theme_background..proc_per_core
		13, 14, 16, 18, 19, 22, 25, 27, // cpu_invert_lower..show_disks
		35, 36, 37, 39                   // net_auto..vim_keys
	};
	for (size_t i = 0; i < config_enum_indices.size(); ++i) {
		config_array_new[config_enum_indices[i]] = (i % 2 == 0);
	}

	bench_config.run("unordered_map<sv,bool> read 20 keys", [&] {
		int count = 0;
		for (const auto& key : config_keys) {
			if (config_map_old[key]) ++count;
		}
		ankerl::nanobench::doNotOptimizeAway(count);
	});

	bench_config.run("array<bool,61>[BoolKey] read 20 keys", [&] {
		int count = 0;
		for (size_t idx : config_enum_indices) {
			if (config_array_new[idx]) ++count;
		}
		ankerl::nanobench::doNotOptimizeAway(count);
	});

	// =========================================================================
	// Group 4: RingBuffer vs deque (400-capacity push cycle)
	//   Old: deque<long long> with push_back + conditional pop_front
	//   New: RingBuffer<long long> with capacity 400 (auto-overwrites)
	// =========================================================================

	ankerl::nanobench::Bench bench_ring;
	bench_ring.warmup(500);
	bench_ring.minEpochIterations(5000);
	bench_ring.relative(true);
	bench_ring.title("RingBuffer vs deque (400-cap cycle)");

	if (json_output) {
		bench_ring.output(nullptr);
	}

	// Pre-fill both to capacity so we measure steady-state behavior
	constexpr size_t RING_CAP = 400;

	std::deque<long long> deque_old;
	for (size_t i = 0; i < RING_CAP; ++i) {
		deque_old.push_back(static_cast<long long>(i));
	}

	RingBuffer<long long> ring_new(RING_CAP);
	for (size_t i = 0; i < RING_CAP; ++i) {
		ring_new.push_back(static_cast<long long>(i));
	}

	long long deque_val = 0;
	long long ring_val = 0;

	bench_ring.run("deque push_back+pop_front (at cap)", [&] {
		deque_old.push_back(deque_val++);
		if (deque_old.size() > RING_CAP) {
			deque_old.pop_front();
		}
		ankerl::nanobench::doNotOptimizeAway(deque_old.back());
	});

	bench_ring.run("RingBuffer push_back (at cap)", [&] {
		ring_new.push_back(ring_val++);
		ankerl::nanobench::doNotOptimizeAway(ring_new.back());
	});

	// =========================================================================
	// JSON output
	// =========================================================================

	if (json_output) {
		std::cout << "[";
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench_cpu, std::cout);
		std::cout << ",";
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench_graph, std::cout);
		std::cout << ",";
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench_config, std::cout);
		std::cout << ",";
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench_ring, std::cout);
		std::cout << "]" << std::endl;
	}

	return 0;
}
