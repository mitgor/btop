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
#include "btop_dirty.hpp"
#include <thread>
#include <vector>

TEST(DirtyBit, MarkSingleBit) {
	PendingDirty pd;
	pd.mark(DirtyBit::Cpu);
	auto taken = pd.take();
	EXPECT_TRUE(has_bit(taken, DirtyBit::Cpu));
	EXPECT_FALSE(has_bit(taken, DirtyBit::Mem));
}

TEST(DirtyBit, MarkMultipleBits) {
	PendingDirty pd;
	pd.mark(DirtyBit::Cpu);
	pd.mark(DirtyBit::Mem);
	auto taken = pd.take();
	EXPECT_TRUE(has_bit(taken, DirtyBit::Cpu));
	EXPECT_TRUE(has_bit(taken, DirtyBit::Mem));
}

TEST(DirtyBit, TakeClears) {
	PendingDirty pd;
	pd.mark(DirtyBit::Proc);
	pd.take();
	auto second = pd.take();
	EXPECT_EQ(static_cast<uint32_t>(second), 0u);
}

TEST(DirtyBit, MarkAll) {
	PendingDirty pd;
	pd.mark(DirtyAll);
	auto taken = pd.take();
	EXPECT_TRUE(has_bit(taken, DirtyBit::Cpu));
	EXPECT_TRUE(has_bit(taken, DirtyBit::Mem));
	EXPECT_TRUE(has_bit(taken, DirtyBit::Net));
	EXPECT_TRUE(has_bit(taken, DirtyBit::Proc));
	EXPECT_TRUE(has_bit(taken, DirtyBit::Gpu));
}

TEST(DirtyBit, ForceFullEmitSeparateFromBoxBits) {
	PendingDirty pd;
	pd.mark(DirtyBit::ForceFullEmit);
	auto taken = pd.take();
	EXPECT_TRUE(has_bit(taken, DirtyBit::ForceFullEmit));
	EXPECT_FALSE(has_bit(taken, DirtyBit::Cpu));
}

TEST(DirtyBit, ConcurrentMarkAndTake) {
	PendingDirty pd;
	constexpr int iterations = 10000;

	std::thread producer([&] {
		for (int i = 0; i < iterations; ++i) {
			pd.mark(DirtyBit::Cpu | DirtyBit::Mem);
		}
	});

	uint32_t total_taken = 0;
	for (int i = 0; i < iterations; ++i) {
		total_taken |= static_cast<uint32_t>(pd.take());
	}
	producer.join();
	// Drain any remaining
	total_taken |= static_cast<uint32_t>(pd.take());

	// At least some marks should have been observed
	EXPECT_TRUE(total_taken & static_cast<uint32_t>(DirtyBit::Cpu));
	EXPECT_TRUE(total_taken & static_cast<uint32_t>(DirtyBit::Mem));
}
