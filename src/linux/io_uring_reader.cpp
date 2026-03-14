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

#include "io_uring_reader.hpp"
#include "../btop_tools.hpp"

#include <fcntl.h>
#include <unistd.h>

#ifdef USE_IO_URING
#include <cstring>
#endif

namespace Linux {

//? ------ Constructor / Destructor ------

ProcReader::ProcReader() {
#ifdef USE_IO_URING
	uring_available = init_uring();
#endif
}

ProcReader::~ProcReader() {
#ifdef USE_IO_URING
	if (uring_available) {
		io_uring_queue_exit(&ring);
	}
#endif
}

//? ------ Public API ------

int ProcReader::submit_batch(ReadRequest* requests, int count) {
	if (count <= 0) return 0;

#ifdef USE_IO_URING
	if (uring_available) {
		return uring_batch(requests, count);
	}
#endif

	return sequential_fallback(requests, count);
}

bool ProcReader::using_io_uring() const {
#ifdef USE_IO_URING
	return uring_available;
#else
	return false;
#endif
}

//? ------ POSIX Sequential Fallback ------

int ProcReader::sequential_fallback(ReadRequest* requests, int count) {
	int success = 0;
	for (int i = 0; i < count; ++i) {
		requests[i].result = Tools::read_proc_file(requests[i].path, requests[i].buffer, requests[i].buf_size);
		if (requests[i].result > 0) ++success;
	}
	return success;
}

//? ------ io_uring Backend ------

#ifdef USE_IO_URING

bool ProcReader::init_uring() {
	int ret = io_uring_queue_init(RING_SIZE, &ring, 0);
	if (ret < 0) return false;

	//? Probe for required opcodes
	struct io_uring_probe* probe = io_uring_get_probe_ring(&ring);
	if (!probe) {
		io_uring_queue_exit(&ring);
		return false;
	}

	bool has_openat = io_uring_opcode_supported(probe, IORING_OP_OPENAT);
	bool has_read = io_uring_opcode_supported(probe, IORING_OP_READ);
	bool has_close = io_uring_opcode_supported(probe, IORING_OP_CLOSE);
	io_uring_free_probe(probe);

	if (!has_openat || !has_read || !has_close) {
		io_uring_queue_exit(&ring);
		return false;
	}

	return true;
}

//? Encode request index and operation type into user_data
//? Op: 0=OPENAT, 1=READ, 2=CLOSE
static inline uint64_t encode_user_data(int index, int op) {
	return (static_cast<uint64_t>(index) << 2) | static_cast<uint64_t>(op);
}

static inline int decode_index(uint64_t user_data) {
	return static_cast<int>(user_data >> 2);
}

static inline int decode_op(uint64_t user_data) {
	return static_cast<int>(user_data & 0x3);
}

int ProcReader::uring_batch(ReadRequest* requests, int count) {
	//? Process in chunks of MAX_BATCH to avoid ring exhaustion
	if (count > MAX_BATCH) {
		int total_success = 0;
		for (int offset = 0; offset < count; offset += MAX_BATCH) {
			int chunk = (count - offset > MAX_BATCH) ? MAX_BATCH : (count - offset);
			total_success += uring_batch(requests + offset, chunk);
		}
		return total_success;
	}

	//? Prepare linked SQE chains: OPENAT -> READ -> CLOSE for each file
	for (int i = 0; i < count; ++i) {
		requests[i].result = -1; // default to error

		//? OPENAT SQE
		struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
		if (!sqe) {
			//? Ring full — fall back to sequential for remaining
			return sequential_fallback(requests, count);
		}
		io_uring_prep_openat(sqe, AT_FDCWD, requests[i].path, O_RDONLY | O_CLOEXEC, 0);
		io_uring_sqe_set_data64(sqe, encode_user_data(i, 0));
		sqe->flags |= IOSQE_IO_LINK;

		//? READ SQE (fd filled by linked OPENAT)
		sqe = io_uring_get_sqe(&ring);
		if (!sqe) {
			return sequential_fallback(requests, count);
		}
		io_uring_prep_read(sqe, 0, requests[i].buffer, static_cast<unsigned>(requests[i].buf_size - 1), 0);
		io_uring_sqe_set_data64(sqe, encode_user_data(i, 1));
		sqe->flags |= IOSQE_IO_HARDLINK; // prevent short-read chain cancellation

		//? CLOSE SQE (fd filled by linked OPENAT)
		sqe = io_uring_get_sqe(&ring);
		if (!sqe) {
			return sequential_fallback(requests, count);
		}
		io_uring_prep_close(sqe, 0);
		io_uring_sqe_set_data64(sqe, encode_user_data(i, 2));
		// No link flag on last SQE in chain
	}

	//? Submit all chains at once
	int submitted = io_uring_submit(&ring);
	if (submitted < 0) {
		return sequential_fallback(requests, count);
	}

	//? Harvest completions: 3 CQEs per file (OPENAT + READ + CLOSE)
	const int total_cqes = count * 3;
	int success = 0;

	for (int i = 0; i < total_cqes; ++i) {
		struct io_uring_cqe* cqe;
		int ret = io_uring_wait_cqe(&ring, &cqe);
		if (ret < 0) break;

		uint64_t ud = io_uring_cqe_get_data64(cqe);
		int idx = decode_index(ud);
		int op = decode_op(ud);

		if (op == 1 && idx >= 0 && idx < count) {
			//? READ completion
			if (cqe->res > 0) {
				requests[idx].result = cqe->res;
				requests[idx].buffer[cqe->res] = '\0';
			}
			// else result stays -1
		}

		io_uring_cqe_seen(&ring, cqe);
	}

	//? Count successes
	for (int i = 0; i < count; ++i) {
		if (requests[i].result > 0) ++success;
	}

	return success;
}

#endif // USE_IO_URING

} // namespace Linux
