// btop_state.hpp — Application lifecycle state definitions
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

namespace Global {

	/// Application lifecycle states, ordered by check priority (highest first in main loop).
	/// The main loop checks: Error > Quitting > Sleeping > Reloading > Resizing > Running.
	/// WARNING: The if/else if chain ordering is load-bearing. Do NOT reorder checks.
	enum class AppState : std::uint8_t {
		Running   = 0,  ///< Normal operation
		Resizing  = 1,  ///< Terminal resize detected
		Reloading = 2,  ///< Config reload requested (SIGUSR2 / Ctrl+R)
		Sleeping  = 3,  ///< SIGTSTP received, about to suspend
		Quitting  = 4,  ///< Clean shutdown requested
		Error     = 5,  ///< Thread exception, will exit with error
	};

	/// Thread-safe application state. Replaces former atomic<bool> flags:
	/// resized, quitting, should_quit, should_sleep, reload_conf, thread_exception.
	/// NOTE: init_conf and _runner_started remain as separate atomic<bool>
	/// because they serve as a lock and lifecycle marker respectively.
	extern std::atomic<AppState> app_state;

	/// Debug/logging helper — returns the name of the given state.
	constexpr std::string_view to_string(AppState s) noexcept {
		switch (s) {
			case AppState::Running:   return "Running";
			case AppState::Resizing:  return "Resizing";
			case AppState::Reloading: return "Reloading";
			case AppState::Sleeping:  return "Sleeping";
			case AppState::Quitting:  return "Quitting";
			case AppState::Error:     return "Error";
		}
		return "Unknown";
	}

} // namespace Global
