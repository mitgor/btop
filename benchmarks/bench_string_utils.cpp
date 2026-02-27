// SPDX-License-Identifier: Apache-2.0
//
// Micro-benchmarks for btop string utility hot functions using nanobench.

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <cstring>
#include <iostream>
#include <string>

#include "btop_tools.hpp"

int main(int argc, char* argv[]) {
	bool json_output = false;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--json") == 0) {
			json_output = true;
		}
	}

	// Prepare realistic test data
	// A colored string with ANSI escape codes (~80+ chars with escape sequences)
	const std::string colored_line =
		"\033[38;5;196mCPU \033[38;5;208m100%\033[0m "
		"\033[38;5;46m||||||||||\033[0m "
		"\033[1;37mProcess Name Here\033[0m "
		"\033[38;5;214m1234 MB\033[0m "
		"\033[38;5;123muser\033[0m";

	// A plain UTF-8 string (~200 chars, no escape codes)
	const std::string plain_text =
		"The quick brown fox jumps over the lazy dog. "
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
		"Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
		"Ut enim ad minim veniam, quis nostrud exercitation.";

	ankerl::nanobench::Bench bench;
	bench.warmup(100);
	bench.relative(true);
	bench.title("String Utilities");

	if (json_output) {
		bench.output(nullptr);
	}

	bench.run("Fx::uncolor (colored line)", [&] {
		auto result = Fx::uncolor(colored_line);
		ankerl::nanobench::doNotOptimizeAway(result);
	});

	bench.run("Tools::ulen (plain)", [&] {
		auto result = Tools::ulen(plain_text);
		ankerl::nanobench::doNotOptimizeAway(result);
	});

	bench.run("Tools::uresize (to 50)", [&] {
		auto result = Tools::uresize(plain_text, 50);
		ankerl::nanobench::doNotOptimizeAway(result);
	});

	bench.run("Tools::ljust (pad to 100)", [&] {
		auto result = Tools::ljust(std::string(plain_text.substr(0, 40)), 100, true);
		ankerl::nanobench::doNotOptimizeAway(result);
	});

	bench.run("Tools::rjust (pad to 100)", [&] {
		auto result = Tools::rjust(std::string(plain_text.substr(0, 40)), 100, true);
		ankerl::nanobench::doNotOptimizeAway(result);
	});

	bench.run("Tools::cjust (pad to 100)", [&] {
		auto result = Tools::cjust(std::string(plain_text.substr(0, 40)), 100, true);
		ankerl::nanobench::doNotOptimizeAway(result);
	});

	bench.run("Mv::to(10, 50)", [&] {
		auto result = Mv::to(10, 50);
		ankerl::nanobench::doNotOptimizeAway(result);
	});

	if (json_output) {
		ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, std::cout);
	}

	return 0;
}
