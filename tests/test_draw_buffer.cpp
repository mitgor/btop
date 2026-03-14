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

#include <string>
#include <iterator>

#include <fmt/format.h>
#include <gtest/gtest.h>

namespace {

//? Mock draw-like function: appends to caller's buffer via fmt::format_to
void mock_draw_format_to(int value, const std::string& label, std::string& out) {
	fmt::format_to(std::back_inserter(out), "{}: {}\n", label, value);
}

//? Mock draw-like function: appends to caller's buffer via +=
void mock_draw_append(const std::string& content, std::string& out) {
	out += content;
}

TEST(DrawBuffer, FormatToAppendsExpectedContent) {
	std::string buf;
	mock_draw_format_to(42, "cpu_usage", buf);
	EXPECT_EQ(buf, "cpu_usage: 42\n");
}

TEST(DrawBuffer, MultipleAppendsAccumulate) {
	std::string buf;
	buf.reserve(4096);
	const auto initial_capacity = buf.capacity();

	mock_draw_format_to(10, "cpu", buf);
	mock_draw_append("mem: 50%\n", buf);
	mock_draw_format_to(100, "net_speed", buf);

	EXPECT_EQ(buf, "cpu: 10\nmem: 50%\nnet_speed: 100\n");

	//? Buffer should not have reallocated (capacity stays >= initial after small appends)
	EXPECT_GE(buf.capacity(), initial_capacity);
}

TEST(DrawBuffer, ClearPreservesCapacity) {
	std::string buf;
	buf.reserve(256 * 1024);
	const auto reserved_capacity = buf.capacity();

	//? Fill buffer with some content
	for (int i = 0; i < 100; ++i) {
		mock_draw_format_to(i, "item", buf);
	}
	EXPECT_GT(buf.size(), 0u);

	//? Clear should preserve capacity
	buf.clear();
	EXPECT_EQ(buf.size(), 0u);
	EXPECT_EQ(buf.capacity(), reserved_capacity);

	//? Second cycle: append again without reallocation
	const auto cap_before = buf.capacity();
	for (int i = 0; i < 100; ++i) {
		mock_draw_format_to(i, "item", buf);
	}
	EXPECT_EQ(buf.capacity(), cap_before);
}

} // namespace
