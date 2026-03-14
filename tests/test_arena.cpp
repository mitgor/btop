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

#include <gtest/gtest.h>
#include <memory_resource>
#include <vector>
#include <cstddef>

// Test 1: pmr::monotonic_buffer_resource backed by stack buffer can allocate pmr::vector elements
TEST(Arena, Basic) {
	alignas(64) std::byte buffer[4096];
	std::pmr::monotonic_buffer_resource arena{buffer, sizeof(buffer), std::pmr::null_memory_resource()};

	std::pmr::vector<int> vec{&arena};
	for (int i = 0; i < 100; ++i) {
		vec.push_back(i);
	}

	ASSERT_EQ(vec.size(), 100u);
	for (int i = 0; i < 100; ++i) {
		EXPECT_EQ(vec[i], i);
	}
}

// Test 2: After release(), the arena reuses memory (allocation address falls within original buffer range)
TEST(Arena, Release) {
	alignas(64) std::byte buffer[4096];
	std::pmr::monotonic_buffer_resource arena{buffer, sizeof(buffer), std::pmr::null_memory_resource()};

	// First allocation
	void* first = arena.allocate(64, 8);
	ASSERT_NE(first, nullptr);

	// Release and re-allocate
	arena.release();
	void* second = arena.allocate(64, 8);
	ASSERT_NE(second, nullptr);

	// Both pointers should fall within the original buffer range
	auto buf_start = reinterpret_cast<std::uintptr_t>(buffer);
	auto buf_end = buf_start + sizeof(buffer);
	auto first_addr = reinterpret_cast<std::uintptr_t>(first);
	auto second_addr = reinterpret_cast<std::uintptr_t>(second);

	EXPECT_GE(first_addr, buf_start);
	EXPECT_LT(first_addr, buf_end);
	EXPECT_GE(second_addr, buf_start);
	EXPECT_LT(second_addr, buf_end);
}

// Test 3: Multiple release()/reuse cycles work without corruption
TEST(Arena, MultipleCycles) {
	alignas(64) std::byte buffer[4096];
	std::pmr::monotonic_buffer_resource arena{buffer, sizeof(buffer), std::pmr::null_memory_resource()};

	for (int cycle = 0; cycle < 10; ++cycle) {
		arena.release();

		std::pmr::vector<long long> vec{&arena};
		for (int i = 0; i < 10; ++i) {
			vec.push_back(static_cast<long long>(cycle * 100 + i));
		}

		ASSERT_EQ(vec.size(), 10u);
		for (int i = 0; i < 10; ++i) {
			EXPECT_EQ(vec[i], static_cast<long long>(cycle * 100 + i));
		}
	}
}

// Test 4: Tiny arena with new_delete_resource() upstream falls back to heap gracefully
TEST(Arena, Fallback) {
	alignas(64) std::byte tiny_buffer[64];
	std::pmr::monotonic_buffer_resource arena{tiny_buffer, sizeof(tiny_buffer), std::pmr::new_delete_resource()};

	// Allocate more than 64 bytes -- should fall back to heap via new_delete_resource
	std::pmr::vector<int> vec{&arena};
	for (int i = 0; i < 100; ++i) {
		vec.push_back(i);
	}

	ASSERT_EQ(vec.size(), 100u);
	for (int i = 0; i < 100; ++i) {
		EXPECT_EQ(vec[i], i);
	}
}
