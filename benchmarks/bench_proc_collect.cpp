// SPDX-License-Identifier: Apache-2.0
//
// Micro-benchmark for btop Proc::collect (Linux only) using nanobench.
//
// On non-Linux platforms, this file compiles to a stub that prints a skip message.

#ifdef __linux__

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <cstring>
#include <iostream>
#include <string>

#include "btop_shared.hpp"
#include "btop_tools.hpp"
#include "btop_config.hpp"

int main(int argc, char* argv[]) {
	bool json_output = false;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--json") == 0) {
			json_output = true;
		}
	}

	// Set minimal terminal dimensions
	Term::width = 200;
	Term::height = 50;

	ankerl::nanobench::Bench bench;
	bench.warmup(10);
	bench.relative(true);
	bench.title("Proc Collect");

	if (json_output) {
		bench.output(nullptr);
	}

	// Benchmark /proc/self/stat parsing (lightweight, no init needed)
	{
		bench.run("/proc/self/stat read+parse", [&] {
			char buf[4096];
			ssize_t n = Tools::read_proc_file("/proc/self/stat", buf, sizeof(buf));
			ankerl::nanobench::doNotOptimizeAway(n);
		});
	}

	// Benchmark /proc/stat parsing (CPU usage data)
	{
		bench.run("/proc/stat read", [&] {
			char buf[16384];
			ssize_t n = Tools::read_proc_file("/proc/stat", buf, sizeof(buf));
			ankerl::nanobench::doNotOptimizeAway(n);
		});
	}

	// Benchmark /proc directory listing (process enumeration)
	{
		namespace fs = std::filesystem;
		bench.run("/proc directory scan", [&] {
			int count = 0;
			for (const auto& entry : fs::directory_iterator("/proc")) {
				auto name = entry.path().filename().string();
				if (!name.empty() && name[0] >= '0' && name[0] <= '9') {
					count++;
				}
			}
			ankerl::nanobench::doNotOptimizeAway(count);
		});
	}

	// Try full Proc::collect if initialization is feasible
	{
		bool init_success = false;
		try {
			std::vector<std::string> load_warnings;
			Config::load("", load_warnings);
			Shared::init();
			init_success = true;
		} catch (const std::exception& e) {
			std::cerr << "WARNING: Shared::init() failed: " << e.what() << std::endl;
			std::cerr << "Proc::collect full benchmark will be SKIPPED." << std::endl;
		}

		if (init_success) {
			bench.minEpochIterations(3);
			bench.run("Proc::collect (full)", [&] {
				auto& result = Proc::collect(false);
				ankerl::nanobench::doNotOptimizeAway(result);
			});
		} else {
			std::cout << "SKIP: Proc::collect (full) -- Shared::init() failed" << std::endl;
		}
	}

	if (json_output) {
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, std::cout);
	}

	return 0;
}

#else // !__linux__

#include <iostream>

int main(int, char*[]) {
	std::cout << "Proc::collect benchmark is Linux-only. Skipping." << std::endl;
	return 0;
}

#endif // __linux__
