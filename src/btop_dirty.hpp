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

#pragma once

#include <atomic>
#include <cstdint>

enum class DirtyBit : uint32_t {
	Cpu           = 1u << 0,
	Mem           = 1u << 1,
	Net           = 1u << 2,
	Proc          = 1u << 3,
	Gpu           = 1u << 4,
	ForceFullEmit = 1u << 5,
};

constexpr DirtyBit operator|(DirtyBit a, DirtyBit b) {
	return static_cast<DirtyBit>(
		static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr DirtyBit operator&(DirtyBit a, DirtyBit b) {
	return static_cast<DirtyBit>(
		static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr bool has_bit(DirtyBit mask, DirtyBit bit) {
	return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(bit)) != 0;
}

/// All per-box bits OR'd together (does NOT include ForceFullEmit)
constexpr DirtyBit DirtyAll = DirtyBit::Cpu | DirtyBit::Mem | DirtyBit::Net
                            | DirtyBit::Proc | DirtyBit::Gpu;

struct PendingDirty {
	/// Mark one or more bits as dirty (thread-safe, lock-free)
	void mark(DirtyBit bits) {
		bits_.fetch_or(static_cast<uint32_t>(bits), std::memory_order_release);
	}

	/// Atomically take all accumulated dirty bits and clear (single consumer)
	DirtyBit take() {
		return static_cast<DirtyBit>(
			bits_.exchange(0, std::memory_order_acquire));
	}

	/// Peek without clearing (for diagnostics/testing only)
	DirtyBit peek() const {
		return static_cast<DirtyBit>(
			bits_.load(std::memory_order_acquire));
	}

private:
	std::atomic<uint32_t> bits_{0};
};
