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

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>

#include "btop_state.hpp"

namespace event {
	struct TimerTick   {};                  // Main loop timer expired (Phase 12)
	struct Resize      {};                  // SIGWINCH
	struct Quit        { int exit_code; };  // SIGINT / SIGTERM
	struct Sleep       {};                  // SIGTSTP
	struct Resume      {};                  // SIGCONT
	struct Reload      {};                  // SIGUSR2
	struct ThreadError {};                  // Runner thread exception

	struct KeyInput {                       // Keyboard/mouse input (Phase 12)
		std::array<char, 32> key_data{};
		std::uint8_t key_len{0};

		KeyInput() noexcept = default;

		explicit KeyInput(std::string_view k) noexcept
			: key_len(static_cast<std::uint8_t>(std::min(k.size(), std::size_t{31}))) {
			std::copy_n(k.data(), key_len, key_data.data());
		}

		std::string_view key() const noexcept {
			return {key_data.data(), key_len};
		}
	};
}

using AppEvent = std::variant<
	event::TimerTick,
	event::Resize,
	event::Quit,
	event::Sleep,
	event::Resume,
	event::Reload,
	event::ThreadError,
	event::KeyInput
>;

static_assert(std::is_trivially_copyable_v<AppEvent>,
	"AppEvent must be trivially copyable for signal-safe queue operations");

/// Dispatch a single event against current state via two-variant visit.
/// Defined in btop.cpp where on_event overloads are visible.
AppStateVar dispatch_event(const AppStateVar& current, const AppEvent& ev);

/// Lock-free SPSC ring buffer, safe for use in signal handlers (push side).
/// Producer: signal handler context (async-signal-safe push).
/// Consumer: main loop thread (try_pop).
template<typename T, std::size_t Capacity>
	requires (Capacity > 0 && (Capacity & (Capacity - 1)) == 0)  // power of 2
class EventQueue {
	static_assert(std::is_trivially_copyable_v<T>,
		"EventQueue elements must be trivially copyable for signal safety");

	// Cache-line aligned to avoid false sharing between producer and consumer
	alignas(64) std::atomic<std::uint32_t> head_{0};   // written by producer
	alignas(64) std::atomic<std::uint32_t> tail_{0};   // written by consumer
	std::array<T, Capacity> slots_{};

	static constexpr std::uint32_t mask = Capacity - 1;

public:
	/// Push an event. Returns true if pushed, false if queue is full (event dropped).
	/// ASYNC-SIGNAL-SAFE: uses only lock-free atomic load/store + trivial copy.
	bool push(const T& item) noexcept {
		const auto h = head_.load(std::memory_order_relaxed);
		const auto t = tail_.load(std::memory_order_acquire);
		if (h - t >= Capacity) return false;  // full
		slots_[h & mask] = item;
		head_.store(h + 1, std::memory_order_release);
		return true;
	}

	/// Pop an event. Returns nullopt if queue is empty.
	/// NOT signal-safe (main thread only).
	std::optional<T> try_pop() noexcept {
		const auto t = tail_.load(std::memory_order_relaxed);
		const auto h = head_.load(std::memory_order_acquire);
		if (t == h) return std::nullopt;  // empty
		T item = slots_[t & mask];
		tail_.store(t + 1, std::memory_order_release);
		return item;
	}

	/// Check if queue has events without consuming them.
	bool empty() const noexcept {
		return head_.load(std::memory_order_acquire) ==
			   tail_.load(std::memory_order_acquire);
	}
};

namespace Global {
	/// Signal-to-main-loop event queue.
	/// Signal handlers push events; main loop pops and processes.
	inline constexpr std::size_t event_queue_capacity = 32;
	extern EventQueue<AppEvent, event_queue_capacity> event_queue;
}
