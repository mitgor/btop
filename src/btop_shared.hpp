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
#include <deque>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <unistd.h>

#include "btop_events.hpp"
#include "btop_tools.hpp"

// From `man 3 getifaddrs`: <net/if.h> must be included before <ifaddrs.h>
// clang-format off
#include <net/if.h>
#include <ifaddrs.h>
// clang-format on

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
# include <kvm.h>
#endif

using std::array;
using std::atomic;
using std::deque;
using std::string;
using std::tuple;
using std::vector;

using namespace std::literals; // for operator""s

//---------------------------------------------
// RingBuffer<T> -- fixed-capacity circular buffer
// Replaces deque<long long> for time-series data with zero steady-state allocation.
// Provides deque-compatible interface: push_back, pop_front, operator[], iterators.
//---------------------------------------------
template<typename T>
class RingBuffer {
	std::unique_ptr<T[]> data_;
	size_t capacity_ = 0;
	size_t head_ = 0;  // index of first (oldest) element
	size_t size_ = 0;

public:
	//-- Iterator -----------------------------------------------------------------
	class iterator {
	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type        = T;
		using difference_type   = std::ptrdiff_t;
		using pointer           = T*;
		using reference         = T&;

		iterator() = default;
		iterator(RingBuffer* buf, size_t logical)
			: buf_(buf), logical_(logical) {}

		reference operator*() const { return (*buf_)[logical_]; }
		pointer   operator->() const { return &(*buf_)[logical_]; }

		iterator& operator++() { ++logical_; return *this; }
		iterator  operator++(int) { auto tmp = *this; ++logical_; return tmp; }
		iterator& operator--() { --logical_; return *this; }
		iterator  operator--(int) { auto tmp = *this; --logical_; return tmp; }

		iterator& operator+=(difference_type n) { logical_ += n; return *this; }
		iterator& operator-=(difference_type n) { logical_ -= n; return *this; }
		iterator  operator+(difference_type n) const { return {buf_, logical_ + static_cast<size_t>(static_cast<difference_type>(logical_) + n - static_cast<difference_type>(logical_))}; }
		iterator  operator-(difference_type n) const { return {buf_, logical_ - n}; }
		difference_type operator-(const iterator& o) const { return static_cast<difference_type>(logical_) - static_cast<difference_type>(o.logical_); }

		reference operator[](difference_type n) const { return (*buf_)[logical_ + n]; }

		bool operator==(const iterator& o) const { return logical_ == o.logical_; }
		bool operator!=(const iterator& o) const { return logical_ != o.logical_; }
		bool operator< (const iterator& o) const { return logical_ <  o.logical_; }
		bool operator<=(const iterator& o) const { return logical_ <= o.logical_; }
		bool operator> (const iterator& o) const { return logical_ >  o.logical_; }
		bool operator>=(const iterator& o) const { return logical_ >= o.logical_; }

		friend iterator operator+(difference_type n, const iterator& it) { return it + n; }

	private:
		RingBuffer* buf_ = nullptr;
		size_t logical_ = 0;
	};

	class const_iterator {
	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type        = T;
		using difference_type   = std::ptrdiff_t;
		using pointer           = const T*;
		using reference         = const T&;

		const_iterator() = default;
		const_iterator(const RingBuffer* buf, size_t logical)
			: buf_(buf), logical_(logical) {}

		reference operator*() const { return (*buf_)[logical_]; }
		pointer   operator->() const { return &(*buf_)[logical_]; }

		const_iterator& operator++() { ++logical_; return *this; }
		const_iterator  operator++(int) { auto tmp = *this; ++logical_; return tmp; }
		const_iterator& operator--() { --logical_; return *this; }
		const_iterator  operator--(int) { auto tmp = *this; --logical_; return tmp; }

		const_iterator& operator+=(difference_type n) { logical_ += n; return *this; }
		const_iterator& operator-=(difference_type n) { logical_ -= n; return *this; }
		const_iterator  operator+(difference_type n) const { return {buf_, logical_ + static_cast<size_t>(static_cast<difference_type>(logical_) + n - static_cast<difference_type>(logical_))}; }
		const_iterator  operator-(difference_type n) const { return {buf_, logical_ - n}; }
		difference_type operator-(const const_iterator& o) const { return static_cast<difference_type>(logical_) - static_cast<difference_type>(o.logical_); }

		reference operator[](difference_type n) const { return (*buf_)[logical_ + n]; }

		bool operator==(const const_iterator& o) const { return logical_ == o.logical_; }
		bool operator!=(const const_iterator& o) const { return logical_ != o.logical_; }
		bool operator< (const const_iterator& o) const { return logical_ <  o.logical_; }
		bool operator<=(const const_iterator& o) const { return logical_ <= o.logical_; }
		bool operator> (const const_iterator& o) const { return logical_ >  o.logical_; }
		bool operator>=(const const_iterator& o) const { return logical_ >= o.logical_; }

		friend const_iterator operator+(difference_type n, const const_iterator& it) { return it + n; }

	private:
		const RingBuffer* buf_ = nullptr;
		size_t logical_ = 0;
	};

	//-- Constructors & assignment ------------------------------------------------

	RingBuffer() = default;

	explicit RingBuffer(size_t capacity)
		: data_(capacity > 0 ? std::make_unique<T[]>(capacity) : nullptr)
		, capacity_(capacity) {}

	// Copy
	RingBuffer(const RingBuffer& other)
		: data_(other.capacity_ > 0 ? std::make_unique<T[]>(other.capacity_) : nullptr)
		, capacity_(other.capacity_)
		, head_(other.head_)
		, size_(other.size_) {
		if (data_ && other.data_) {
			std::copy_n(other.data_.get(), capacity_, data_.get());
		}
	}

	RingBuffer& operator=(const RingBuffer& other) {
		if (this != &other) {
			capacity_ = other.capacity_;
			head_ = other.head_;
			size_ = other.size_;
			data_ = capacity_ > 0 ? std::make_unique<T[]>(capacity_) : nullptr;
			if (data_ && other.data_) {
				std::copy_n(other.data_.get(), capacity_, data_.get());
			}
		}
		return *this;
	}

	// Move
	RingBuffer(RingBuffer&& other) noexcept
		: data_(std::move(other.data_))
		, capacity_(other.capacity_)
		, head_(other.head_)
		, size_(other.size_) {
		other.capacity_ = 0;
		other.head_ = 0;
		other.size_ = 0;
	}

	RingBuffer& operator=(RingBuffer&& other) noexcept {
		if (this != &other) {
			data_ = std::move(other.data_);
			capacity_ = other.capacity_;
			head_ = other.head_;
			size_ = other.size_;
			other.capacity_ = 0;
			other.head_ = 0;
			other.size_ = 0;
		}
		return *this;
	}

	//-- Capacity -----------------------------------------------------------------
	[[nodiscard]] size_t size() const noexcept { return size_; }
	[[nodiscard]] bool   empty() const noexcept { return size_ == 0; }
	[[nodiscard]] size_t capacity() const noexcept { return capacity_; }

	//-- Element access -----------------------------------------------------------
	T& operator[](size_t i) {
		static T default_val{};
		if (capacity_ == 0 or size_ == 0) return default_val;
		return data_[(head_ + i) % capacity_];
	}
	const T& operator[](size_t i) const {
		static const T default_val{};
		if (capacity_ == 0 or size_ == 0) return default_val;
		return data_[(head_ + i) % capacity_];
	}

	T& front() {
		static T default_val{};
		if (capacity_ == 0 or size_ == 0) return default_val;
		return data_[head_];
	}
	const T& front() const {
		static const T default_val{};
		if (capacity_ == 0 or size_ == 0) return default_val;
		return data_[head_];
	}

	T& back() {
		static T default_val{};
		if (capacity_ == 0 or size_ == 0) return default_val;
		return data_[(head_ + size_ - 1) % capacity_];
	}
	const T& back() const {
		static const T default_val{};
		if (capacity_ == 0 or size_ == 0) return default_val;
		return data_[(head_ + size_ - 1) % capacity_];
	}

	//-- Modifiers ----------------------------------------------------------------
	void push_back(const T& value) {
		if (capacity_ == 0) {
			resize(1);
		}
		if (size_ < capacity_) {
			data_[(head_ + size_) % capacity_] = value;
			++size_;
		} else {
			// Overwrite oldest
			data_[head_] = value;
			head_ = (head_ + 1) % capacity_;
		}
	}

	void pop_front() {
		if (size_ == 0) return;
		head_ = (head_ + 1) % capacity_;
		--size_;
	}

	void clear() noexcept {
		head_ = 0;
		size_ = 0;
	}

	void resize(size_t new_capacity) {
		if (new_capacity == capacity_) return;
		if (new_capacity == 0) {
			data_.reset();
			capacity_ = 0;
			head_ = 0;
			size_ = 0;
			return;
		}
		auto new_data = std::make_unique<T[]>(new_capacity);
		size_t to_keep = std::min(size_, new_capacity);
		// Copy the most recent `to_keep` elements
		size_t skip = size_ - to_keep;
		for (size_t i = 0; i < to_keep; ++i) {
			new_data[i] = data_[(head_ + skip + i) % capacity_];
		}
		data_ = std::move(new_data);
		capacity_ = new_capacity;
		head_ = 0;
		size_ = to_keep;
	}

	//-- Iterators ----------------------------------------------------------------
	iterator begin() { return {this, 0}; }
	iterator end() { return {this, size_}; }
	const_iterator begin() const { return {this, 0}; }
	const_iterator end() const { return {this, size_}; }
	const_iterator cbegin() const { return {this, 0}; }
	const_iterator cend() const { return {this, size_}; }
};

//---------------------------------------------
// Field enums for O(1) array-indexed data structures
// Replace string-keyed unordered_maps in collector structs (Plan 02 migration).
//---------------------------------------------

// CPU time fields -- matches Linux /proc/stat order; BSD uses subset (total, user, nice, system, idle)
enum class CpuField : size_t {
	total, user, nice, system, idle,
	iowait, irq, softirq, steal, guest, guest_nice,
	COUNT  // = 11
};

// Memory stat/percent fields
enum class MemField : size_t {
	used, available, cached, free,
	swap_total, swap_used, swap_free,
	COUNT  // = 7
};

// Network direction
enum class NetDir : size_t { download, upload, COUNT };

// Per-GPU percent fields
enum class GpuField : size_t {
	gpu_totals, gpu_vram_totals, gpu_pwr_totals,
	COUNT  // = 3
};

// Shared (aggregated) GPU percent fields
enum class SharedGpuField : size_t {
	gpu_average, gpu_vram_total, gpu_pwr_total,
	COUNT  // = 3
};

// Constexpr name tables for debug/serialization (replaces time_names strings in collectors in Plan 02)
inline constexpr std::array<std::string_view, static_cast<size_t>(CpuField::COUNT)> cpu_field_names = {
	"total", "user", "nice", "system", "idle",
	"iowait", "irq", "softirq", "steal", "guest", "guest_nice"
};

inline constexpr std::array<std::string_view, static_cast<size_t>(MemField::COUNT)> mem_field_names = {
	"used", "available", "cached", "free",
	"swap_total", "swap_used", "swap_free"
};

inline constexpr std::array<std::string_view, static_cast<size_t>(NetDir::COUNT)> net_dir_names = {
	"download", "upload"
};

inline constexpr std::array<std::string_view, static_cast<size_t>(GpuField::COUNT)> gpu_field_names = {
	"gpu-totals", "gpu-vram-totals", "gpu-pwr-totals"
};

inline constexpr std::array<std::string_view, static_cast<size_t>(SharedGpuField::COUNT)> shared_gpu_field_names = {
	"gpu-average", "gpu-vram-total", "gpu-pwr-total"
};

//? Runtime string-to-enum helpers for config field resolution
inline std::optional<size_t> cpu_field_from_name(const string& name) {
	for (size_t i = 0; i < cpu_field_names.size(); ++i)
		if (cpu_field_names[i] == name) return i;
	return std::nullopt;
}

inline std::optional<size_t> gpu_field_from_name(const string& name) {
	for (size_t i = 0; i < gpu_field_names.size(); ++i)
		if (gpu_field_names[i] == name) return i;
	return std::nullopt;
}

inline std::optional<size_t> shared_gpu_field_from_name(const string& name) {
	for (size_t i = 0; i < shared_gpu_field_names.size(); ++i)
		if (shared_gpu_field_names[i] == name) return i;
	return std::nullopt;
}

inline std::optional<size_t> mem_field_from_name(const string& name) {
	for (size_t i = 0; i < mem_field_names.size(); ++i)
		if (mem_field_names[i] == name) return i;
	return std::nullopt;
}

void term_resize(bool force=false);
void banner_gen();

extern void clean_quit(int sig);

#include "btop_state.hpp"

namespace Global {
	extern const vector<array<string, 2>> Banner_src;
	extern const string Version;
	extern string exit_error_msg;
	extern string banner;
	extern string overlay;
	extern string clock;
	extern uid_t real_uid, set_uid;
	extern atomic<bool> init_conf;
}

namespace Runner {
	extern atomic<bool> coreNum_reset;
	extern atomic<bool> pause_output;
	extern string debug_bg;

	/// Cooperative cancellation check — replaces direct Runner::stopping reads.
	/// Uses acquire ordering to match current seq_cst semantics.
	inline bool is_stopping() noexcept {
		return Global::runner_state_tag.load(std::memory_order_acquire)
			== Global::RunnerStateTag::Stopping;
	}

	/// Active check — replaces direct Runner::active reads.
	inline bool is_active() noexcept {
		auto tag = Global::runner_state_tag.load(std::memory_order_acquire);
		return tag == Global::RunnerStateTag::Collecting
			|| tag == Global::RunnerStateTag::Drawing;
	}

	/// Wait for runner to become idle (replaces atomic_wait(Runner::active)).
	inline void wait_idle() noexcept {
		while (is_active()) {
			Tools::atomic_wait(Global::runner_state_tag, Global::runner_state_tag.load());
		}
	}

	/// Request a full redraw on the next runner cycle.
	/// Replaces direct Runner::redraw = true mutation (RUNNER-02).
	void request_redraw() noexcept;

	void run(const string& box = "", bool no_update = false, bool force_redraw = false);
	void stop();
}

namespace Tools {
	//* Platform specific function for system_uptime (seconds since last restart)
	double system_uptime();
}

namespace Shared {
	//* Initialize platform specific needed variables and check for errors
	void init();

	extern long coreCount, page_size, clk_tck;

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	struct KvmDeleter {
		void operator()(kvm_t* handle) {
			kvm_close(handle);
		}
	};
	using KvmPtr = std::unique_ptr<kvm_t, KvmDeleter>;
#endif
}

#if defined(GPU_SUPPORT)

namespace Gpu {
	extern vector<string> box;
	extern int width, total_height, min_width, min_height;
	extern vector<int> x_vec, y_vec;
	extern vector<bool> redraw;
	extern int shown;
	extern int count;
	extern vector<int> shown_panels;
	extern vector<string> gpu_names;
	extern vector<int> gpu_b_height_offsets;
	extern long long gpu_pwr_total_max;

	extern std::array<RingBuffer<long long>, static_cast<size_t>(SharedGpuField::COUNT)> shared_gpu_percent;

	const array mem_names { "used"s, "free"s };

	//* Container for process information // TODO
	/*struct proc_info {
    unsigned int pid;
    unsigned long long mem;
	};*/

	//* Container for supported Gpu::*::collect() functions
	struct gpu_info_supported {
		bool gpu_utilization = true,
		   	 mem_utilization = true,
				 gpu_clock = true,
				 mem_clock = true,
				 pwr_usage = true,
				 pwr_state = true,
				 temp_info = true,
				 mem_total = true,
				 mem_used = true,
				 pcie_txrx = true,
				 encoder_utilization = true,
				 decoder_utilization = true;
	};

	//* Per-device container for GPU info
	struct gpu_info {
		std::array<RingBuffer<long long>, static_cast<size_t>(GpuField::COUNT)> gpu_percent{};
		unsigned int gpu_clock_speed; // MHz

		long long pwr_usage; // mW
		long long pwr_max_usage = 255000;
		long long pwr_state;

		RingBuffer<long long> temp{};
		long long temp_max = 110;

		long long mem_total = 0;
		long long mem_used = 0;
		RingBuffer<long long> mem_utilization_percent{}; // TODO: properly handle GPUs that can't report some stats
		long long mem_clock_speed = 0; // MHz

		long long pcie_tx = 0; // KB/s
		long long pcie_rx = 0;

		long long encoder_utilization = 0;
		long long decoder_utilization = 0;

		gpu_info_supported supported_functions;

		// vector<proc_info> graphics_processes = {}; // TODO
		// vector<proc_info> compute_processes = {};
	};

	namespace Nvml {
		extern bool shutdown();
	}
	namespace Rsmi {
		extern bool shutdown();
	}
	#ifdef __APPLE__
	namespace AppleSilicon {
		extern bool shutdown();
	}
	#endif

	//* Collect gpu stats and temperatures
    auto collect(bool no_update = false) -> vector<gpu_info>&;

	//* Draw contents of gpu box using <gpus> as source
  	string draw(const gpu_info& gpu, unsigned long index, bool force_redraw, bool data_same);
}

#endif // GPU_SUPPORT

namespace Cpu {
	extern string box;
	extern int x, y, width, height, min_width, min_height;
	extern bool shown, redraw, got_sensors, cpu_temp_only, has_battery, supports_watts;
	extern string cpuName, cpuHz;
	extern vector<string> available_fields;
	extern vector<string> available_sensors;
	extern tuple<int, float, long, string> current_bat;
	extern std::optional<std::string> container_engine;

	struct cpu_info {
		std::array<RingBuffer<long long>, static_cast<size_t>(CpuField::COUNT)> cpu_percent{};
		vector<RingBuffer<long long>> core_percent;
		vector<RingBuffer<long long>> temp;
		long long temp_max = 0;
		array<double, 3> load_avg;
		float usage_watts = 0;
		std::optional<std::vector<std::int32_t>> active_cpus;
	};

	//* Collect cpu stats and temperatures
	auto collect(bool no_update = false) -> cpu_info&;

	//* Draw contents of cpu box using <cpu> as source
    string draw(
		const cpu_info& cpu,
#if defined(GPU_SUPPORT)
		const vector<Gpu::gpu_info>& gpu,
#endif
		bool force_redraw = false,
		bool data_same = false
	);

	//* Parse /proc/cpu info for mapping of core ids
	auto get_core_mapping() -> std::unordered_map<int, int>;
	extern std::unordered_map<int, int> core_mapping;

	auto get_cpuHz() -> string;

	//* Get battery info from /sys
	auto get_battery() -> tuple<int, float, long, string>;

	string trim_name(string);
}

namespace Mem {
	extern string box;
	extern int x, y, width, height, min_width, min_height;
	extern bool has_swap, shown, redraw;
	const array mem_names { "used"s, "available"s, "cached"s, "free"s };
	const array swap_names { "swap_used"s, "swap_free"s };
	extern int disk_ios;

	struct disk_info {
		std::filesystem::path dev;
		string name;
		string fstype{};                // defaults to ""
		std::filesystem::path stat{};   // defaults to ""
		int64_t total{};
		int64_t used{};
		int64_t free{};
		int used_percent{};
		int free_percent{};

		array<int64_t, 3> old_io = {0, 0, 0};
		RingBuffer<long long> io_read{};
		RingBuffer<long long> io_write{};
		RingBuffer<long long> io_activity{};
	};

	struct mem_info {
		std::array<uint64_t, static_cast<size_t>(MemField::COUNT)> stats{};
		std::array<RingBuffer<long long>, static_cast<size_t>(MemField::COUNT)> percent{};
		std::unordered_map<string, disk_info> disks;
		vector<string> disks_order;
	};

	//?* Get total system memory
	uint64_t get_totalMem();

	//* Collect mem & disks stats
	auto collect(bool no_update = false) -> mem_info&;

	//* Draw contents of mem box using <mem> as source
	string draw(const mem_info& mem, bool force_redraw = false, bool data_same = false);

}

namespace Net {
	extern string box;
	extern int x, y, width, height, min_width, min_height;
	extern bool shown, redraw;
	extern string selected_iface;
	extern vector<string> interfaces;
	extern bool rescale;
	extern std::array<uint64_t, 2> graph_max;

	struct net_stat {
		uint64_t speed{};
		uint64_t top{};
		uint64_t total{};
		uint64_t last{};
		uint64_t offset{};
		uint64_t rollover{};
	};

	struct net_info {
		std::array<RingBuffer<long long>, static_cast<size_t>(NetDir::COUNT)> bandwidth{};
		std::array<net_stat, static_cast<size_t>(NetDir::COUNT)> stat{};
		string ipv4{};      // defaults to ""
		string ipv6{};      // defaults to ""
		bool connected{};
	};

	class IfAddrsPtr {
		struct ifaddrs* ifaddr;
		int status;
	public:
		IfAddrsPtr() { status = getifaddrs(&ifaddr); }
		~IfAddrsPtr() noexcept { freeifaddrs(ifaddr); }
		IfAddrsPtr(const IfAddrsPtr &) = delete;
		IfAddrsPtr& operator=(IfAddrsPtr& other) = delete;
		IfAddrsPtr(IfAddrsPtr &&) = delete;
		IfAddrsPtr& operator=(IfAddrsPtr&& other) = delete;
		[[nodiscard]] constexpr auto operator()() -> struct ifaddrs* { return ifaddr; }
		[[nodiscard]] constexpr auto get() -> struct ifaddrs* { return ifaddr; }
		[[nodiscard]] constexpr auto get_status() const noexcept -> int { return status; };
	};

	extern std::unordered_map<string, net_info> current_net;

	//* Collect net upload/download stats
	auto collect(bool no_update=false) -> net_info&;

	//* Draw contents of net box using <net> as source
	string draw(const net_info& net, bool force_redraw = false, bool data_same = false);
}

namespace Proc {
	extern atomic<int> numpids;

	extern string box;
	extern int x, y, width, height, min_width, min_height;
	extern bool shown, redraw;
	extern int select_max;
	extern atomic<int> detailed_pid;
	extern int selected_pid, start, selected, collapse, expand, filter_found, selected_depth, toggle_children;
	extern int scroll_pos;
	extern string selected_name;
	extern atomic<bool> resized;

	//? Contains the valid sorting options for processes
	const vector<string> sort_vector = {
		"pid",
		"name",
		"command",
		"threads",
		"user",
		"memory",
		"cpu direct",
		"cpu lazy",
	};

	//? Translation from process state char to explanative string
	const std::unordered_map<char, string> proc_states = {
		{'R', "Running"},
		{'S', "Sleeping"},
		{'D', "Waiting"},
		{'Z', "Zombie"},
		{'T', "Stopped"},
		{'t', "Tracing"},
		{'X', "Dead"},
		{'x', "Dead"},
		{'K', "Wakekill"},
		{'W', "Unknown"},
		{'P', "Parked"}
	};

	//* Container for process information
	struct proc_info {
		size_t pid{};
		string name{};          // defaults to ""
		string cmd{};           // defaults to ""
		string short_cmd{};     // defaults to ""
		size_t threads{};
		int name_offset{};
		string user{};          // defaults to ""
		uint64_t mem{};
		double cpu_p{};         // defaults to = 0.0
		double cpu_c{};         // defaults to = 0.0
		char state = '0';
		int64_t p_nice{};
		uint64_t ppid{};
		uint64_t cpu_s{};
		uint64_t cpu_t{};
		uint64_t death_time{};
		string prefix{};        // defaults to ""
		size_t depth{};
		size_t tree_index{};
		bool collapsed{};
		bool filtered{};
	};

	//* Container for process info box
	struct detail_container {
		size_t last_pid{};
		bool skip_smaps{};
		proc_info entry;
		string elapsed, parent, status, io_read, io_write, memory;
		long long first_mem = -1;
		RingBuffer<long long> cpu_percent{};
		RingBuffer<long long> mem_bytes{};
	};

	//? Contains all info for proc detailed box
	extern detail_container detailed;

	//* Collect and sort process information from /proc
	auto collect(bool no_update = false) -> vector<proc_info>&;

	//* Update current selection and view, returns -1 if no change otherwise the current selection
	int selection(const std::string_view cmd_key);

	//* Draw contents of proc box using <plist> as data source
	string draw(const vector<proc_info>& plist, bool force_redraw = false, bool data_same = false);

	struct tree_proc {
		std::reference_wrapper<proc_info> entry;
		vector<tree_proc> children;
	};

	//* Change priority (nice) of pid, returns true on success otherwise false
	bool set_priority(pid_t pid, int priority);

	//* Sort vector of proc_info's
	void proc_sorter(vector<proc_info>& proc_vec, const string& sorting, bool reverse, bool tree = false);

	//* Recursive sort of process tree
	void tree_sort(vector<tree_proc>& proc_vec, const string& sorting, bool reverse, bool paused,
					int& c_index, const int index_max, bool collapsed = false);

	auto matches_filter(const proc_info& proc, const std::string& filter) -> bool;

	//* Generate process tree list
	void _tree_gen(proc_info& cur_proc, vector<proc_info>& in_procs, vector<tree_proc>& out_procs,
				   int cur_depth, bool collapsed, const string& filter,
				   bool found = false, bool no_update = false, bool should_filter = false);

	//* Build prefixes for tree view
	void _collect_prefixes(tree_proc& t, bool is_last, const string &header = "");
}

/// Detect container engine.
auto detect_container() -> std::optional<std::string>;
