// SPDX-License-Identifier: Apache-2.0
//
// Micro-benchmarks comparing stable_sort, partial_sort, and SoA key extraction
// for process list sorting at various sizes and display counts.
//
// Purpose: Quantify the speedup of SoA key extraction (cache-friendly sort)
// over AoS partial_sort and stable_sort when sorting large process lists
// by numeric keys.
//
// Self-contained: uses a local proc_info mirror to avoid btop runtime deps.

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace rng = std::ranges;

// ============================================================================
// SoA sort implementation mirror (matches src/btop_shared.cpp)
// ============================================================================

struct SortEntry {
	double key;
	uint32_t index;
};

/// SoA key extraction sort: extracts numeric sort keys into a contiguous array
/// for cache-friendly comparison (4 entries/cache line vs 1 with AoS ~200-byte structs).
template <typename Vec, typename Proj>
void soa_sort(Vec& proc_vec, Proj projection, bool ascending, int display_count) {
	thread_local std::vector<SortEntry> keys;
	keys.resize(proc_vec.size());

	for (uint32_t i = 0; i < static_cast<uint32_t>(proc_vec.size()); ++i)
		keys[i] = {static_cast<double>(projection(proc_vec[i])), i};

	const int n = (display_count > 0 && display_count < static_cast<int>(keys.size()))
		? display_count : static_cast<int>(keys.size());
	auto middle = keys.begin() + n;

	if (ascending)
		std::partial_sort(keys.begin(), middle, keys.end(),
			[](const SortEntry& a, const SortEntry& b) { return a.key < b.key; });
	else
		std::partial_sort(keys.begin(), middle, keys.end(),
			[](const SortEntry& a, const SortEntry& b) { return a.key > b.key; });

	using value_type = typename Vec::value_type;
	thread_local std::vector<value_type> sorted_front;
	sorted_front.clear();
	sorted_front.reserve(n);
	for (auto it = keys.begin(); it != middle; ++it)
		sorted_front.push_back(std::move(proc_vec[it->index]));

	for (size_t i = 0; i < sorted_front.size(); ++i)
		proc_vec[i] = std::move(sorted_front[i]);
}

/// Mirror of Proc::proc_info with the fields relevant to sorting.
/// Kept in sync with src/btop_shared.hpp — if the real struct changes,
/// update this mirror and the proc_sorter implementation together.
struct proc_info {
	size_t pid{};
	std::string name{};
	std::string cmd{};
	std::string short_cmd{};
	size_t threads{};
	int name_offset{};
	std::string user{};
	uint64_t mem{};
	double cpu_p{};
	double cpu_c{};
	char state = '0';
	int64_t p_nice{};
	uint64_t ppid{};
	uint64_t cpu_s{};
	uint64_t cpu_t{};
	uint64_t death_time{};
	std::string prefix{};
	size_t depth{};
	size_t tree_index{};
	bool collapsed{};
	bool filtered{};
};

/// Generate a vector of N synthetic proc_info entries with deterministic randomness.
static auto generate_procs(size_t n, unsigned seed = 42) -> std::vector<proc_info> {
	std::mt19937 gen(seed);
	std::uniform_real_distribution<double> cpu_dist(0.0, 100.0);
	std::uniform_int_distribution<uint64_t> mem_dist(0, 16ULL * 1024 * 1024 * 1024);
	std::uniform_int_distribution<size_t> thread_dist(1, 32);

	std::vector<proc_info> procs;
	procs.reserve(n);

	for (size_t i = 0; i < n; ++i) {
		proc_info p;
		p.pid = i + 1;
		p.name = "proc_" + std::to_string(i);
		p.cmd = "/usr/bin/proc_" + std::to_string(i);
		p.short_cmd = p.name;
		p.threads = thread_dist(gen);
		p.user = "user" + std::to_string(i % 10);
		p.mem = mem_dist(gen);
		p.cpu_p = cpu_dist(gen);
		p.cpu_c = cpu_dist(gen);
		p.state = 'R';
		p.ppid = (i > 0) ? (i / 2) : 0;
		procs.push_back(std::move(p));
	}
	return procs;
}

int main(int argc, char* argv[]) {
	bool json_output = false;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--json") == 0) {
			json_output = true;
		}
	}

	// =========================================================================
	// Group 1: stable_sort baseline at varying N (sort by cpu_p descending)
	// =========================================================================

	ankerl::nanobench::Bench bench_stable;
	bench_stable.warmup(100);
	bench_stable.minEpochIterations(500);
	bench_stable.relative(true);
	bench_stable.title("stable_sort by cpu_p (varying N)");

	if (json_output) {
		bench_stable.output(nullptr);
	}

	for (size_t n : {100, 500, 1000, 5000}) {
		auto base = generate_procs(n);
		bench_stable.run("stable_sort/cpu_p/N=" + std::to_string(n), [&] {
			auto vec = base;  // fresh copy each iteration
			rng::stable_sort(vec, rng::greater{}, &proc_info::cpu_p);
			ankerl::nanobench::doNotOptimizeAway(vec.front().cpu_p);
		});
	}

	// =========================================================================
	// Group 2: partial_sort with K=50 at varying N (sort by cpu_p descending)
	// =========================================================================

	ankerl::nanobench::Bench bench_partial;
	bench_partial.warmup(100);
	bench_partial.minEpochIterations(500);
	bench_partial.relative(true);
	bench_partial.title("partial_sort by cpu_p K=50 (varying N)");

	if (json_output) {
		bench_partial.output(nullptr);
	}

	constexpr size_t K = 50;

	for (size_t n : {100, 500, 1000, 5000}) {
		auto base = generate_procs(n);
		size_t k = std::min(K, n);
		bench_partial.run("partial_sort/cpu_p/N=" + std::to_string(n) + "/K=" + std::to_string(k), [&] {
			auto vec = base;  // fresh copy each iteration
			rng::partial_sort(vec.begin(), vec.begin() + static_cast<ptrdiff_t>(k), vec.end(),
				rng::greater{}, &proc_info::cpu_p);
			ankerl::nanobench::doNotOptimizeAway(vec.front().cpu_p);
		});
	}

	// =========================================================================
	// Group 3: Head-to-head comparison at N=1000 K=50
	// =========================================================================

	ankerl::nanobench::Bench bench_compare;
	bench_compare.warmup(100);
	bench_compare.minEpochIterations(500);
	bench_compare.relative(true);
	bench_compare.title("stable_sort vs partial_sort (N=1000, K=50)");

	if (json_output) {
		bench_compare.output(nullptr);
	}

	{
		auto base = generate_procs(1000);

		bench_compare.run("stable_sort/cpu_p/N=1000", [&] {
			auto vec = base;
			rng::stable_sort(vec, rng::greater{}, &proc_info::cpu_p);
			ankerl::nanobench::doNotOptimizeAway(vec.front().cpu_p);
		});

		bench_compare.run("partial_sort/cpu_p/N=1000/K=50", [&] {
			auto vec = base;
			rng::partial_sort(vec.begin(), vec.begin() + 50, vec.end(),
				rng::greater{}, &proc_info::cpu_p);
			ankerl::nanobench::doNotOptimizeAway(vec.front().cpu_p);
		});
	}

	// =========================================================================
	// Group 4: Sort by different key types at N=1000 (stable_sort only)
	// =========================================================================

	ankerl::nanobench::Bench bench_keys;
	bench_keys.warmup(100);
	bench_keys.minEpochIterations(500);
	bench_keys.relative(true);
	bench_keys.title("stable_sort by key type (N=1000)");

	if (json_output) {
		bench_keys.output(nullptr);
	}

	{
		auto base = generate_procs(1000);

		bench_keys.run("sort/pid/N=1000", [&] {
			auto vec = base;
			rng::stable_sort(vec, rng::greater{}, &proc_info::pid);
			ankerl::nanobench::doNotOptimizeAway(vec.front().pid);
		});

		bench_keys.run("sort/name/N=1000", [&] {
			auto vec = base;
			rng::stable_sort(vec, rng::less{}, &proc_info::name);
			ankerl::nanobench::doNotOptimizeAway(vec.front().name);
		});

		bench_keys.run("sort/mem/N=1000", [&] {
			auto vec = base;
			rng::stable_sort(vec, rng::greater{}, &proc_info::mem);
			ankerl::nanobench::doNotOptimizeAway(vec.front().mem);
		});

		bench_keys.run("sort/cpu_p/N=1000", [&] {
			auto vec = base;
			rng::stable_sort(vec, rng::greater{}, &proc_info::cpu_p);
			ankerl::nanobench::doNotOptimizeAway(vec.front().cpu_p);
		});
	}

	// =========================================================================
	// Group 5: SoA partial_sort at varying N with K=50 (sort by cpu_p descending)
	// =========================================================================

	ankerl::nanobench::Bench bench_soa;
	bench_soa.warmup(100);
	bench_soa.minEpochIterations(500);
	bench_soa.relative(true);
	bench_soa.title("SoA partial_sort by cpu_p K=50 (varying N)");

	if (json_output) {
		bench_soa.output(nullptr);
	}

	for (size_t n : {100, 500, 1000, 5000}) {
		auto base = generate_procs(n);
		bench_soa.run("sort/soa/cpu_p/N=" + std::to_string(n), [&] {
			auto vec = base;
			soa_sort(vec, [](const proc_info& p) { return p.cpu_p; }, false, 50);
			ankerl::nanobench::doNotOptimizeAway(vec.front().cpu_p);
		});
	}

	// =========================================================================
	// Group 6: AoS vs SoA head-to-head at N=1000 K=50 (cpu_p descending)
	// =========================================================================

	ankerl::nanobench::Bench bench_aos_vs_soa;
	bench_aos_vs_soa.warmup(100);
	bench_aos_vs_soa.minEpochIterations(500);
	bench_aos_vs_soa.relative(true);
	bench_aos_vs_soa.title("AoS vs SoA (N=1000, K=50, cpu_p)");

	if (json_output) {
		bench_aos_vs_soa.output(nullptr);
	}

	{
		auto base = generate_procs(1000);

		// AoS stable_sort baseline (full sort, no partial)
		bench_aos_vs_soa.run("sort/aos_vs_soa/stable_sort/N=1000", [&] {
			auto vec = base;
			rng::stable_sort(vec, rng::greater{}, &proc_info::cpu_p);
			ankerl::nanobench::doNotOptimizeAway(vec.front().cpu_p);
		});

		// AoS partial_sort
		bench_aos_vs_soa.run("sort/aos_vs_soa/partial_sort/N=1000/K=50", [&] {
			auto vec = base;
			rng::partial_sort(vec.begin(), vec.begin() + 50, vec.end(),
				rng::greater{}, &proc_info::cpu_p);
			ankerl::nanobench::doNotOptimizeAway(vec.front().cpu_p);
		});

		// SoA partial_sort
		bench_aos_vs_soa.run("sort/aos_vs_soa/soa_sort/N=1000/K=50", [&] {
			auto vec = base;
			soa_sort(vec, [](const proc_info& p) { return p.cpu_p; }, false, 50);
			ankerl::nanobench::doNotOptimizeAway(vec.front().cpu_p);
		});
	}

	// =========================================================================
	// Group 7: SoA threshold boundary (N=64, 128, 256) — validates threshold
	// =========================================================================

	ankerl::nanobench::Bench bench_threshold;
	bench_threshold.warmup(100);
	bench_threshold.minEpochIterations(500);
	bench_threshold.relative(true);
	bench_threshold.title("SoA threshold boundary (K=50, cpu_p)");

	if (json_output) {
		bench_threshold.output(nullptr);
	}

	for (size_t n : {64, 128, 256}) {
		auto base = generate_procs(n);
		size_t k = std::min(static_cast<size_t>(50), n);

		bench_threshold.run("sort/soa_threshold/partial_sort/N=" + std::to_string(n), [&] {
			auto vec = base;
			rng::partial_sort(vec.begin(), vec.begin() + static_cast<ptrdiff_t>(k), vec.end(),
				rng::greater{}, &proc_info::cpu_p);
			ankerl::nanobench::doNotOptimizeAway(vec.front().cpu_p);
		});

		bench_threshold.run("sort/soa_threshold/soa_sort/N=" + std::to_string(n), [&] {
			auto vec = base;
			soa_sort(vec, [](const proc_info& p) { return p.cpu_p; }, false, static_cast<int>(k));
			ankerl::nanobench::doNotOptimizeAway(vec.front().cpu_p);
		});
	}

	// =========================================================================
	// Correctness validation: SoA sort produces same top-50 as stable_sort
	// =========================================================================

	{
		std::cout << "\n=== Correctness Validation ===\n";
		auto procs_stable = generate_procs(1000, 99);  // different seed for variety
		auto procs_soa = procs_stable;  // identical copy

		// stable_sort: full sort descending by cpu_p
		rng::stable_sort(procs_stable, rng::greater{}, &proc_info::cpu_p);

		// SoA sort: partial sort top-50 descending by cpu_p
		soa_sort(procs_soa, [](const proc_info& p) { return p.cpu_p; }, false, 50);

		// Compare top-50 as sets (order may differ for equal values)
		std::set<size_t> stable_pids, soa_pids;
		for (int i = 0; i < 50; ++i) {
			stable_pids.insert(procs_stable[i].pid);
			soa_pids.insert(procs_soa[i].pid);
		}

		if (stable_pids == soa_pids) {
			std::cout << "PASS: SoA top-50 matches stable_sort top-50 (1000 procs, cpu_p desc)\n";
		} else {
			std::cout << "FAIL: SoA top-50 differs from stable_sort!\n";
			std::cout << "  stable_sort pids not in SoA: ";
			for (auto pid : stable_pids)
				if (!soa_pids.count(pid)) std::cout << pid << " ";
			std::cout << "\n  SoA pids not in stable_sort: ";
			for (auto pid : soa_pids)
				if (!stable_pids.count(pid)) std::cout << pid << " ";
			std::cout << "\n";
			return 1;
		}

		// Also verify ascending sort
		auto procs_asc_stable = generate_procs(1000, 77);
		auto procs_asc_soa = procs_asc_stable;

		rng::stable_sort(procs_asc_stable, rng::less{}, &proc_info::cpu_p);
		soa_sort(procs_asc_soa, [](const proc_info& p) { return p.cpu_p; }, true, 50);

		std::set<size_t> asc_stable_pids, asc_soa_pids;
		for (int i = 0; i < 50; ++i) {
			asc_stable_pids.insert(procs_asc_stable[i].pid);
			asc_soa_pids.insert(procs_asc_soa[i].pid);
		}

		if (asc_stable_pids == asc_soa_pids) {
			std::cout << "PASS: SoA top-50 matches stable_sort top-50 (1000 procs, cpu_p asc)\n";
		} else {
			std::cout << "FAIL: SoA ascending top-50 differs from stable_sort!\n";
			return 1;
		}

		std::cout << "=== All correctness checks passed ===\n\n";
	}

	// =========================================================================
	// JSON output
	// =========================================================================

	if (json_output) {
		std::cout << "[";
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench_stable, std::cout);
		std::cout << ",";
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench_partial, std::cout);
		std::cout << ",";
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench_compare, std::cout);
		std::cout << ",";
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench_keys, std::cout);
		std::cout << ",";
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench_soa, std::cout);
		std::cout << ",";
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench_aos_vs_soa, std::cout);
		std::cout << ",";
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench_threshold, std::cout);
		std::cout << "]" << std::endl;
	}

	return 0;
}
