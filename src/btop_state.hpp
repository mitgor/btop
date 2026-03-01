// btop_state.hpp — Application lifecycle state definitions
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace Global {

	/// Application lifecycle states, ordered by check priority (highest first in main loop).
	/// The main loop checks: Error > Quitting > Sleeping > Reloading > Resizing > Running.
	/// WARNING: The if/else if chain ordering is load-bearing. Do NOT reorder checks.
	enum class AppStateTag : std::uint8_t {
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
	extern std::atomic<AppStateTag> app_state;

	/// Debug/logging helper — returns the name of the given state.
	constexpr std::string_view to_string(AppStateTag s) noexcept {
		switch (s) {
			case AppStateTag::Running:   return "Running";
			case AppStateTag::Resizing:  return "Resizing";
			case AppStateTag::Reloading: return "Reloading";
			case AppStateTag::Sleeping:  return "Sleeping";
			case AppStateTag::Quitting:  return "Quitting";
			case AppStateTag::Error:     return "Error";
		}
		return "Unknown";
	}

	/// Runner thread lifecycle states (independent from App FSM).
	enum class RunnerStateTag : std::uint8_t {
		Idle       = 0,  ///< Waiting for work (semaphore blocked)
		Collecting = 1,  ///< Data collection in progress
		Drawing    = 2,  ///< Rendering output
		Stopping   = 3,  ///< Cooperative cancellation
	};

	/// Shadow atomic for cross-thread queries (replaces Runner::active + Runner::stopping).
	extern std::atomic<RunnerStateTag> runner_state_tag;

	/// Debug/logging helper.
	constexpr std::string_view to_string(RunnerStateTag s) noexcept {
		switch (s) {
			case RunnerStateTag::Idle:       return "Idle";
			case RunnerStateTag::Collecting: return "Collecting";
			case RunnerStateTag::Drawing:    return "Drawing";
			case RunnerStateTag::Stopping:   return "Stopping";
		}
		return "Unknown";
	}

} // namespace Global

/// Typed state structs carrying per-state data.
/// Main-thread only — the authoritative application state.
namespace state {
	struct Running {
		uint64_t update_ms;    // Current update interval from config
		uint64_t future_time;  // Timestamp of next timer tick
	};

	struct Resizing {};   // Entry action does the work (calcSizes, etc.)
	struct Reloading {};  // Entry action does the work (init_config, etc.)
	struct Sleeping {};   // Entry action suspends process

	struct Quitting {
		int exit_code;    // Exit code for clean_quit()
	};

	struct Error {
		std::string message;  // Error description
	};
}

/// Runner thread state structs — owned by the runner thread.
namespace runner {
	struct Idle {};
	struct Collecting {};
	struct Drawing {
		bool force_redraw;
	};
	struct Stopping {};
}

/// Runner thread state variant — owned exclusively by the runner thread.
using RunnerStateVar = std::variant<
	runner::Idle,
	runner::Collecting,
	runner::Drawing,
	runner::Stopping
>;

/// Convert runner variant state to tag for shadow updates.
inline Global::RunnerStateTag to_runner_tag(const RunnerStateVar& s) noexcept {
	using Global::RunnerStateTag;
	return std::visit([](const auto& st) -> RunnerStateTag {
		using T = std::decay_t<decltype(st)>;
		if constexpr (std::is_same_v<T, runner::Idle>)       return RunnerStateTag::Idle;
		if constexpr (std::is_same_v<T, runner::Collecting>) return RunnerStateTag::Collecting;
		if constexpr (std::is_same_v<T, runner::Drawing>)    return RunnerStateTag::Drawing;
		if constexpr (std::is_same_v<T, runner::Stopping>)   return RunnerStateTag::Stopping;
	}, s);
}

/// The authoritative application state variant. Main-thread only.
/// Holds exactly one alternative — being in two states simultaneously is impossible.
using AppStateVar = std::variant<
	state::Running,
	state::Resizing,
	state::Reloading,
	state::Sleeping,
	state::Quitting,
	state::Error
>;

/// Convert variant state to tag enum for shadow updates.
inline Global::AppStateTag to_tag(const AppStateVar& s) noexcept {
	using Global::AppStateTag;
	return std::visit([](const auto& st) -> AppStateTag {
		using T = std::decay_t<decltype(st)>;
		if constexpr (std::is_same_v<T, state::Running>)   return AppStateTag::Running;
		if constexpr (std::is_same_v<T, state::Resizing>)  return AppStateTag::Resizing;
		if constexpr (std::is_same_v<T, state::Reloading>) return AppStateTag::Reloading;
		if constexpr (std::is_same_v<T, state::Sleeping>)  return AppStateTag::Sleeping;
		if constexpr (std::is_same_v<T, state::Quitting>)  return AppStateTag::Quitting;
		if constexpr (std::is_same_v<T, state::Error>)     return AppStateTag::Error;
	}, s);
}
