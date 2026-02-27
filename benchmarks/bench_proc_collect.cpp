// SPDX-License-Identifier: Apache-2.0
//
// Micro-benchmark for btop Proc::collect (Linux only) using nanobench.
//
// On non-Linux platforms, this file compiles to a stub that prints a skip message.

#ifdef __linux__

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
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
		std::string stat_content;
		{
			std::ifstream f("/proc/self/stat");
			std::stringstream ss;
			ss << f.rdbuf();
			stat_content = ss.str();
		}

		bench.run("/proc/self/stat read+parse", [&] {
			std::ifstream f("/proc/self/stat");
			std::string content;
			std::getline(f, content);
			ankerl::nanobench::doNotOptimizeAway(content);
		});
	}

	// Benchmark /proc/stat parsing (CPU usage data)
	{
		bench.run("/proc/stat read", [&] {
			std::ifstream f("/proc/stat");
			std::string line;
			std::vector<std::string> lines;
			while (std::getline(f, line)) {
				lines.push_back(line);
			}
			ankerl::nanobench::doNotOptimizeAway(lines);
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
		try {
			// Minimal config initialization
			std::vector<std::string> load_warnings;
			Config::load("", load_warnings);

			Shared::init();

			bench.minEpochIterations(3);
			bench.run("Proc::collect (full)", [&] {
				auto& result = Proc::collect(false);
				ankerl::nanobench::doNotOptimizeAway(result);
			});
		} catch (const std::exception& e) {
			std::cerr << "Note: Full Proc::collect benchmark skipped: " << e.what() << std::endl;
			std::cerr << "      /proc parsing sub-benchmarks above still provide useful data." << std::endl;
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
