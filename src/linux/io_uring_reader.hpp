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
#include <cstddef>
#include <cstdint>
#include <sys/types.h>

#ifdef USE_IO_URING
#include <liburing.h>
#endif

namespace Linux {

class ProcReader {
public:
	struct ReadRequest {
		const char* path;     // e.g., "/proc/1234/stat"
		char* buffer;         // caller-owned buffer
		size_t buf_size;      // buffer capacity
		ssize_t result;       // bytes read on success, -1 on error
	};

	ProcReader();
	~ProcReader();

	/// Batch-read multiple /proc files. Returns number of successful reads.
	/// Uses io_uring when available, otherwise sequential POSIX fallback.
	int submit_batch(ReadRequest* requests, int count);

	/// Whether io_uring is active (for logging/benchmarking)
	bool using_io_uring() const;

	// Non-copyable
	ProcReader(const ProcReader&) = delete;
	ProcReader& operator=(const ProcReader&) = delete;

private:
	int sequential_fallback(ReadRequest* requests, int count);

#ifdef USE_IO_URING
	struct io_uring ring;
	bool uring_available = false;
	static constexpr unsigned RING_SIZE = 4096;  // power-of-2, supports up to ~1365 files (3 SQEs each)
	static constexpr int MAX_BATCH = 1024;       // max files per submit (3072 SQEs)

	bool init_uring();
	int uring_batch(ReadRequest* requests, int count);
#endif
};

} // namespace Linux
