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

#include <numeric>

#include <gtest/gtest.h>
#include "btop_shared.hpp"

TEST(RingBuffer, DefaultConstructor) {
	RingBuffer<long long> buf;
	EXPECT_EQ(buf.size(), 0u);
	EXPECT_TRUE(buf.empty());
	EXPECT_EQ(buf.capacity(), 0u);
}

TEST(RingBuffer, ExplicitCapacityConstructor) {
	RingBuffer<long long> buf(8);
	EXPECT_EQ(buf.size(), 0u);
	EXPECT_TRUE(buf.empty());
	EXPECT_EQ(buf.capacity(), 8u);
}

TEST(RingBuffer, PushBackUpToCapacity) {
	RingBuffer<long long> buf(4);
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);
	EXPECT_EQ(buf.size(), 3u);
	EXPECT_EQ(buf[0], 10);
	EXPECT_EQ(buf[1], 20);
	EXPECT_EQ(buf[2], 30);
}

TEST(RingBuffer, PushBackWrapAround) {
	RingBuffer<long long> buf(3);
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);
	EXPECT_EQ(buf.size(), 3u);

	// Push beyond capacity -- oldest (10) is overwritten
	buf.push_back(40);
	EXPECT_EQ(buf.size(), 3u);
	EXPECT_EQ(buf[0], 20);  // oldest is now 20
	EXPECT_EQ(buf[1], 30);
	EXPECT_EQ(buf[2], 40);  // newest is 40
}

TEST(RingBuffer, PopFront) {
	RingBuffer<long long> buf(4);
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);

	buf.pop_front();
	EXPECT_EQ(buf.size(), 2u);
	EXPECT_EQ(buf[0], 20);
	EXPECT_EQ(buf[1], 30);

	buf.pop_front();
	EXPECT_EQ(buf.size(), 1u);
	EXPECT_EQ(buf[0], 30);

	buf.pop_front();
	EXPECT_TRUE(buf.empty());
}

TEST(RingBuffer, FrontAndBack) {
	RingBuffer<long long> buf(4);
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);

	EXPECT_EQ(buf.front(), 10);
	EXPECT_EQ(buf.back(), 30);

	// After wrap-around
	buf.push_back(40);
	buf.push_back(50);
	EXPECT_EQ(buf.front(), 20);  // oldest after 2 overwrites
	EXPECT_EQ(buf.back(), 50);   // newest
}

TEST(RingBuffer, OperatorBracketDequeSemantics) {
	// After push_back sequence {10, 20, 30, 40} with capacity 3:
	// result should be {20, 30, 40} with [0]=20, [2]=40
	RingBuffer<long long> buf(3);
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);
	buf.push_back(40);

	EXPECT_EQ(buf[0], 20);
	EXPECT_EQ(buf[1], 30);
	EXPECT_EQ(buf[2], 40);
}

TEST(RingBuffer, Clear) {
	RingBuffer<long long> buf(4);
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);

	buf.clear();
	EXPECT_EQ(buf.size(), 0u);
	EXPECT_TRUE(buf.empty());
	EXPECT_EQ(buf.capacity(), 4u);  // capacity unchanged

	// Can reuse after clear
	buf.push_back(100);
	EXPECT_EQ(buf.size(), 1u);
	EXPECT_EQ(buf[0], 100);
}

TEST(RingBuffer, ResizeLarger) {
	RingBuffer<long long> buf(3);
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);

	buf.resize(6);
	EXPECT_EQ(buf.capacity(), 6u);
	EXPECT_EQ(buf.size(), 3u);
	EXPECT_EQ(buf[0], 10);
	EXPECT_EQ(buf[1], 20);
	EXPECT_EQ(buf[2], 30);
}

TEST(RingBuffer, ResizeSmaller) {
	RingBuffer<long long> buf(5);
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);
	buf.push_back(40);
	buf.push_back(50);

	// Resize to 3: keeps most recent 3 elements {30, 40, 50}
	buf.resize(3);
	EXPECT_EQ(buf.capacity(), 3u);
	EXPECT_EQ(buf.size(), 3u);
	EXPECT_EQ(buf[0], 30);
	EXPECT_EQ(buf[1], 40);
	EXPECT_EQ(buf[2], 50);
}

TEST(RingBuffer, ResizeSmallerAfterWrap) {
	RingBuffer<long long> buf(3);
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);
	buf.push_back(40);  // wraps: {20, 30, 40}
	buf.push_back(50);  // wraps: {30, 40, 50}

	buf.resize(2);  // keeps most recent 2: {40, 50}
	EXPECT_EQ(buf.size(), 2u);
	EXPECT_EQ(buf[0], 40);
	EXPECT_EQ(buf[1], 50);
}

TEST(RingBuffer, Iteration) {
	RingBuffer<long long> buf(5);
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);
	buf.push_back(40);

	long long sum = std::accumulate(buf.begin(), buf.end(), 0LL);
	EXPECT_EQ(sum, 100LL);
}

TEST(RingBuffer, IterationAfterWrap) {
	RingBuffer<long long> buf(3);
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);
	buf.push_back(40);  // {20, 30, 40}

	long long sum = std::accumulate(buf.begin(), buf.end(), 0LL);
	EXPECT_EQ(sum, 90LL);
}

TEST(RingBuffer, ConstIteration) {
	RingBuffer<long long> buf(3);
	buf.push_back(5);
	buf.push_back(15);
	buf.push_back(25);

	const auto& cbuf = buf;
	long long sum = std::accumulate(cbuf.begin(), cbuf.end(), 0LL);
	EXPECT_EQ(sum, 45LL);
}

TEST(RingBuffer, CopyConstructor) {
	RingBuffer<long long> buf(3);
	buf.push_back(10);
	buf.push_back(20);

	RingBuffer<long long> copy(buf);
	EXPECT_EQ(copy.size(), 2u);
	EXPECT_EQ(copy[0], 10);
	EXPECT_EQ(copy[1], 20);

	// Independent -- modifying copy doesn't affect original
	copy.push_back(99);
	EXPECT_EQ(copy[2], 99);
	EXPECT_EQ(buf.size(), 2u);
}

TEST(RingBuffer, CopyAssignment) {
	RingBuffer<long long> buf(3);
	buf.push_back(10);
	buf.push_back(20);

	RingBuffer<long long> other(5);
	other.push_back(100);
	other = buf;

	EXPECT_EQ(other.size(), 2u);
	EXPECT_EQ(other.capacity(), 3u);
	EXPECT_EQ(other[0], 10);
	EXPECT_EQ(other[1], 20);
}

TEST(RingBuffer, MoveConstructor) {
	RingBuffer<long long> buf(3);
	buf.push_back(10);
	buf.push_back(20);

	RingBuffer<long long> moved(std::move(buf));
	EXPECT_EQ(moved.size(), 2u);
	EXPECT_EQ(moved[0], 10);
	EXPECT_EQ(moved[1], 20);

	// Source becomes empty
	EXPECT_EQ(buf.size(), 0u);
	EXPECT_EQ(buf.capacity(), 0u);
}

TEST(RingBuffer, MoveAssignment) {
	RingBuffer<long long> buf(3);
	buf.push_back(10);
	buf.push_back(20);

	RingBuffer<long long> other;
	other = std::move(buf);
	EXPECT_EQ(other.size(), 2u);
	EXPECT_EQ(other[0], 10);

	EXPECT_EQ(buf.size(), 0u);
	EXPECT_EQ(buf.capacity(), 0u);
}

TEST(RingBuffer, PushBackOnZeroCapacity) {
	RingBuffer<long long> buf;
	buf.push_back(10);  // should be a no-op
	EXPECT_TRUE(buf.empty());
}

TEST(RingBuffer, PopFrontOnEmpty) {
	RingBuffer<long long> buf(3);
	buf.pop_front();  // should be a no-op
	EXPECT_TRUE(buf.empty());
}

TEST(RingBuffer, ResizeToZero) {
	RingBuffer<long long> buf(3);
	buf.push_back(10);
	buf.resize(0);
	EXPECT_EQ(buf.capacity(), 0u);
	EXPECT_EQ(buf.size(), 0u);
}

TEST(RingBuffer, UsableInVector) {
	std::vector<RingBuffer<long long>> vec;
	vec.emplace_back(4);
	vec[0].push_back(42);
	EXPECT_EQ(vec[0][0], 42);

	// Copy into vector
	vec.push_back(vec[0]);
	EXPECT_EQ(vec[1][0], 42);
}
