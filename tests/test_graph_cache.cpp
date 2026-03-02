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

// Unit tests for Graph last_data_back caching optimization (Phase 05 Plan 02)

#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "btop_config.hpp"
#include "btop_draw.hpp"
#include "btop_shared.hpp"
#include "btop_theme.hpp"
#include "btop_tools.hpp"

using Theme::GradientKey;

class GraphCacheTest : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		// Initialize Config with defaults (same pattern as bench_draw.cpp)
		Config::conf_dir = std::filesystem::temp_directory_path();
		Config::conf_file = Config::conf_dir / "btop_test_unused.conf";
		std::vector<std::string> load_warnings;
		Config::load(Config::conf_file, load_warnings);

		// Set minimal terminal dimensions
		Term::width = 200;
		Term::height = 50;

		// Initialize theme (generates color gradients needed by Graph)
		Theme::updateThemes();
		Theme::setTheme();
	}
};

TEST_F(GraphCacheTest, SameDataReturnsIdenticalOutput) {
	RingBuffer<long long> data(20);
	for (int i = 0; i < 20; i++) data.push_back(i * 5);

	Draw::Graph g(10, 1, GradientKey::cpu, data);
	std::string first = g(data, false);

	// Call again with same back() value -- should return cached output
	std::string second = g(data, false);
	EXPECT_EQ(first, second);
}

TEST_F(GraphCacheTest, DataSameReturnsSameRef) {
	RingBuffer<long long> data(20);
	for (int i = 0; i < 20; i++) data.push_back(50);

	Draw::Graph g(10, 1, GradientKey::cpu, data);
	std::string& first = g(data, false);
	std::string& same = g(data, true);
	EXPECT_EQ(&first, &same);  // Same reference via data_same early return
}

TEST_F(GraphCacheTest, ChangedDataProducesDifferentOutput) {
	RingBuffer<long long> data(20);
	for (int i = 0; i < 20; i++) data.push_back(50);

	Draw::Graph g(10, 1, GradientKey::cpu, data);
	std::string first = g(data, false);

	data.push_back(90);
	std::string second = g(data, false);
	EXPECT_NE(first, second);
}

TEST_F(GraphCacheTest, MultiHeightCacheWorks) {
	RingBuffer<long long> data(20);
	for (int i = 0; i < 20; i++) data.push_back(i * 5);

	Draw::Graph g(10, 3, GradientKey::cpu, data);
	std::string first = g(data, false);

	// Same data.back(), should return cached
	std::string second = g(data, false);
	EXPECT_EQ(first, second);
}

TEST_F(GraphCacheTest, CacheHitReturnsSameReference) {
	RingBuffer<long long> data(20);
	for (int i = 0; i < 20; i++) data.push_back(42);

	Draw::Graph g(10, 1, GradientKey::cpu, data);
	std::string& first = g(data, false);

	// Second call with unchanged data.back() should hit last_data_back cache
	std::string& second = g(data, false);
	EXPECT_EQ(&first, &second);  // Same reference (cached early return)
}

TEST_F(GraphCacheTest, ConstructorInitializesLastDataBack) {
	// After construction, the first operator() call with same data should cache-hit
	RingBuffer<long long> data(20);
	for (int i = 0; i < 20; i++) data.push_back(75);

	Draw::Graph g(10, 1, GradientKey::cpu, data);
	// First call after constructor -- data.back() matches constructor's last value
	// Should hit the last_data_back cache since constructor initializes it
	std::string& result = g(data, false);
	EXPECT_FALSE(result.empty());

	// Second call should also cache-hit
	std::string& cached = g(data, false);
	EXPECT_EQ(&result, &cached);
}

TEST_F(GraphCacheTest, OutputNotEmptyAfterConstruction) {
	RingBuffer<long long> data(20);
	for (int i = 0; i < 20; i++) data.push_back(i * 5);

	Draw::Graph g(10, 1, GradientKey::cpu, data);
	std::string& out = g();
	EXPECT_FALSE(out.empty());
}

TEST_F(GraphCacheTest, RepeatedUpdatesWithSameValueAreCached) {
	RingBuffer<long long> data(20);
	for (int i = 0; i < 20; i++) data.push_back(60);

	Draw::Graph g(10, 1, GradientKey::cpu, data);
	std::string first = g(data, false);

	// Push same value multiple times -- each call should return cached
	for (int i = 0; i < 5; i++) {
		data.push_back(60);
		std::string result = g(data, false);
		EXPECT_EQ(first, result);
	}
}
