// SPDX-License-Identifier: Apache-2.0
//
// Micro-benchmarks for btop draw-related functions using nanobench.
//
// Note: Full Cpu::draw / Mem::draw / Net::draw / Proc::draw require extensive
// runtime initialization (Shared::init, platform collectors) which makes them
// unsuitable for isolated micro-benchmarks. Instead, this file benchmarks
// the core draw components: Graph, Meter, createBox, and the fmt::format
// string composition that dominates draw function cost.
//
// For full draw function benchmarking, use `btop --benchmark N` (Plan 01-02)
// which initializes the complete runtime.

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <fmt/format.h>

#include "btop_config.hpp"
#include "btop_draw.hpp"
#include "btop_shared.hpp"
#include "btop_theme.hpp"
#include "btop_tools.hpp"

using std::string;

int main(int argc, char* argv[]) {
	bool json_output = false;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--json") == 0) {
			json_output = true;
		}
	}

	// Initialize Config with defaults and set up Theme
	{
		// Create a temporary empty config to get defaults
		Config::conf_dir = std::filesystem::temp_directory_path();
		Config::conf_file = Config::conf_dir / "btop_bench_unused.conf";
		std::vector<std::string> load_warnings;
		Config::load(Config::conf_file, load_warnings);
	}

	// Set minimal terminal dimensions
	Term::width = 200;
	Term::height = 50;

	// Initialize theme with defaults (generates color gradients needed by Meter/Graph)
	Theme::updateThemes();
	Theme::setTheme();

	ankerl::nanobench::Bench bench;
	bench.warmup(100);
	bench.relative(true);
	bench.title("Draw Components");

	if (json_output) {
		bench.output(nullptr);
	}

	// Benchmark Meter creation and rendering using theme gradient name
	try {
		// "cpu" is a valid gradient name in the default theme
		Draw::Meter meter(50, "cpu", false);
		bench.run("Meter::operator() (50w, value=75)", [&] {
			auto result = meter(75);
			ankerl::nanobench::doNotOptimizeAway(result);
		});
	} catch (const std::exception& e) {
		std::cerr << "Meter benchmark skipped: " << e.what() << std::endl;
	}

	// Benchmark Graph construction with synthetic data
	try {
		RingBuffer<long long> data(200);
		for (size_t i = 0; i < 200; ++i) {
			data.push_back(static_cast<long long>(50 + 40 * std::sin(static_cast<double>(i) * 0.1)));
		}

		bench.run("Graph construction (100w x 5h)", [&] {
			Draw::Graph graph(100, 5, "cpu", data, "default", false, false, 100, 0);
			ankerl::nanobench::doNotOptimizeAway(graph);
		});
	} catch (const std::exception& e) {
		std::cerr << "Graph construction benchmark skipped: " << e.what() << std::endl;
	}

	// Benchmark Graph update (operator() with new data)
	try {
		RingBuffer<long long> data(200);
		for (size_t i = 0; i < 200; ++i) {
			data.push_back(static_cast<long long>(50 + 40 * std::sin(static_cast<double>(i) * 0.1)));
		}
		Draw::Graph graph(100, 5, "cpu", data, "default", false, false, 100, 0);

		bench.run("Graph::operator() (update)", [&] {
			data.push_back(data.front());
			auto& result = graph(data, false);
			ankerl::nanobench::doNotOptimizeAway(result);
		});
	} catch (const std::exception& e) {
		std::cerr << "Graph update benchmark skipped: " << e.what() << std::endl;
	}

	// Benchmark Graph cache: unchanged data (last_data_back hit)
	try {
		RingBuffer<long long> data(200);
		for (size_t i = 0; i < 200; ++i) {
			data.push_back(static_cast<long long>(50 + 40 * std::sin(static_cast<double>(i) * 0.1)));
		}
		Draw::Graph graph(100, 5, "cpu", data, "default", false, false, 100, 0);
		// Prime the graph with one call
		graph(data, false);

		bench.run("Graph cache hit (data unchanged)", [&] {
			auto& result = graph(data, false);
			ankerl::nanobench::doNotOptimizeAway(result);
		});
	} catch (const std::exception& e) {
		std::cerr << "Graph cache hit benchmark skipped: " << e.what() << std::endl;
	}

	// Benchmark Graph cache: data_same=true (caller-controlled skip)
	try {
		RingBuffer<long long> data(200);
		for (size_t i = 0; i < 200; ++i) {
			data.push_back(static_cast<long long>(50 + 40 * std::sin(static_cast<double>(i) * 0.1)));
		}
		Draw::Graph graph(100, 5, "cpu", data, "default", false, false, 100, 0);
		graph(data, false);

		bench.run("Graph data_same=true (skip)", [&] {
			auto& result = graph(data, true);
			ankerl::nanobench::doNotOptimizeAway(result);
		});
	} catch (const std::exception& e) {
		std::cerr << "Graph data_same benchmark skipped: " << e.what() << std::endl;
	}

	// Benchmark createBox (box outline generation)
	try {
		bench.run("Draw::createBox (80x24)", [&] {
			auto result = Draw::createBox(1, 1, 80, 24, "", false, "CPU", "100%", 0);
			ankerl::nanobench::doNotOptimizeAway(result);
		});
	} catch (const std::exception& e) {
		std::cerr << "createBox benchmark skipped: " << e.what() << std::endl;
	}

	// Benchmark fmt::format composition (simulates draw output assembly)
	{
		string fg_color = "\033[38;5;196m";
		string reset = "\033[0m";

		bench.run("fmt::format draw composition", [&] {
			auto result = fmt::format(
				"{mv}{fg}CPU {usage:>3}%{reset} {bar}{reset}",
				fmt::arg("mv", Mv::to(5, 10)),
				fmt::arg("fg", fg_color),
				fmt::arg("usage", 95),
				fmt::arg("reset", reset),
				fmt::arg("bar", "||||||||||||||||||||")
			);
			ankerl::nanobench::doNotOptimizeAway(result);
		});
	}

	// Benchmark Mv::to calls (used extensively in draw functions)
	bench.run("Mv::to (100 calls)", [&] {
		string result;
		result.reserve(1000);
		for (int i = 0; i < 100; ++i) {
			result += Mv::to(i % 50, i % 200);
		}
		ankerl::nanobench::doNotOptimizeAway(result);
	});

	// Benchmark Fx::uncolor on draw output (used for overlay rendering)
	{
		string draw_output;
		for (int i = 0; i < 50; ++i) {
			draw_output += fmt::format("\033[38;5;{}m{}\033[0m ", i % 256, std::string(8, '#'));
		}
		bench.run("Fx::uncolor (draw output, 50 segments)", [&] {
			auto result = Fx::uncolor(draw_output);
			ankerl::nanobench::doNotOptimizeAway(result);
		});
	}

	if (json_output) {
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, std::cout);
	}

	return 0;
}
