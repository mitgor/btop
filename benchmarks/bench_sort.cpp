// SPDX-License-Identifier: Apache-2.0
//
// Micro-benchmarks comparing stable_sort vs partial_sort for process list
// sorting at various sizes and display counts.
//
// Purpose: Quantify the speedup of partial_sort (O(N log K)) vs stable_sort
// (O(N log N)) when only K rows are displayed from N total processes.
//
// Self-contained: uses a local proc_info mirror to avoid btop runtime deps.

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace rng = std::ranges;

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
		std::cout << "]" << std::endl;
	}

	return 0;
}
