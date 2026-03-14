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
#include <string>
#include <thread>
#include <unistd.h>
#include <sys/uio.h>

#include "btop_tools.hpp"

using std::string;

TEST(WriteStdout, WritesExactContent) {
	int pipefd[2];
	ASSERT_EQ(pipe(pipefd), 0);
	int saved_stdout = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);

	string test_data = "Hello, write_stdout!";
	Tools::write_stdout(test_data);

	dup2(saved_stdout, STDOUT_FILENO);
	close(saved_stdout);

	char buf[256];
	ssize_t n = read(pipefd[0], buf, sizeof(buf));
	close(pipefd[0]);

	ASSERT_EQ(n, static_cast<ssize_t>(test_data.size()));
	EXPECT_EQ(string(buf, n), test_data);
}

TEST(WriteStdout, EmptyStringNoError) {
	int pipefd[2];
	ASSERT_EQ(pipe(pipefd), 0);
	int saved_stdout = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);

	Tools::write_stdout(string{});

	dup2(saved_stdout, STDOUT_FILENO);
	close(saved_stdout);

	// Close write end is already done; read should return 0 (EOF)
	char buf[16];
	ssize_t n = read(pipefd[0], buf, sizeof(buf));
	close(pipefd[0]);

	EXPECT_EQ(n, 0);
}

TEST(WriteStdout, LargeStringWritesCompletely) {
	int pipefd[2];
	ASSERT_EQ(pipe(pipefd), 0);
	int saved_stdout = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);

	// 100KB test data -- exceeds pipe buffer so we must read concurrently
	string test_data(100 * 1024, 'X');

	// Reader thread drains the pipe while writer may block
	string result;
	result.reserve(test_data.size());
	std::thread reader([&result, read_fd = pipefd[0]] {
		char buf[4096];
		ssize_t n;
		while ((n = read(read_fd, buf, sizeof(buf))) > 0) {
			result.append(buf, n);
		}
	});

	Tools::write_stdout(test_data);

	// Restore stdout so the pipe write-end (STDOUT_FILENO) is closed,
	// allowing the reader to see EOF
	dup2(saved_stdout, STDOUT_FILENO);
	close(saved_stdout);

	reader.join();
	close(pipefd[0]);

	EXPECT_EQ(result.size(), test_data.size());
	EXPECT_EQ(result, test_data);
}

TEST(WriteStdout, CharPointerOverload) {
	int pipefd[2];
	ASSERT_EQ(pipe(pipefd), 0);
	int saved_stdout = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);

	const char* data = "raw pointer test";
	Tools::write_stdout(data, 16);

	dup2(saved_stdout, STDOUT_FILENO);
	close(saved_stdout);

	char buf[256];
	ssize_t n = read(pipefd[0], buf, sizeof(buf));
	close(pipefd[0]);

	ASSERT_EQ(n, 16);
	EXPECT_EQ(string(buf, n), "raw pointer test");
}

TEST(WritevStdout, SingleSegment) {
	int pipefd[2];
	ASSERT_EQ(pipe(pipefd), 0);
	int saved_stdout = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);

	string data = "single segment test";
	struct iovec iov[1];
	iov[0].iov_base = data.data();
	iov[0].iov_len = data.size();
	Tools::write_stdout_iov(iov, 1);

	dup2(saved_stdout, STDOUT_FILENO);
	close(saved_stdout);

	char buf[256];
	ssize_t n = read(pipefd[0], buf, sizeof(buf));
	close(pipefd[0]);

	ASSERT_EQ(n, static_cast<ssize_t>(data.size()));
	EXPECT_EQ(string(buf, n), data);
}

TEST(WritevStdout, MultipleSegments) {
	int pipefd[2];
	ASSERT_EQ(pipe(pipefd), 0);
	int saved_stdout = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);

	string seg1 = "AAA";
	string seg2 = "BBB";
	string seg3 = "CCC";
	struct iovec iov[3];
	iov[0].iov_base = seg1.data();
	iov[0].iov_len = seg1.size();
	iov[1].iov_base = seg2.data();
	iov[1].iov_len = seg2.size();
	iov[2].iov_base = seg3.data();
	iov[2].iov_len = seg3.size();
	Tools::write_stdout_iov(iov, 3);

	dup2(saved_stdout, STDOUT_FILENO);
	close(saved_stdout);

	char buf[256];
	ssize_t n = read(pipefd[0], buf, sizeof(buf));
	close(pipefd[0]);

	ASSERT_EQ(n, 9);
	EXPECT_EQ(string(buf, n), "AAABBBCCC");
}

TEST(WritevStdout, EmptySegment) {
	int pipefd[2];
	ASSERT_EQ(pipe(pipefd), 0);
	int saved_stdout = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);

	string seg1 = "HEAD";
	string seg2;  // empty
	string seg3 = "TAIL";
	struct iovec iov[3];
	iov[0].iov_base = seg1.data();
	iov[0].iov_len = seg1.size();
	iov[1].iov_base = seg2.data();
	iov[1].iov_len = seg2.size();
	iov[2].iov_base = seg3.data();
	iov[2].iov_len = seg3.size();
	Tools::write_stdout_iov(iov, 3);

	dup2(saved_stdout, STDOUT_FILENO);
	close(saved_stdout);

	char buf[256];
	ssize_t n = read(pipefd[0], buf, sizeof(buf));
	close(pipefd[0]);

	ASSERT_EQ(n, 8);
	EXPECT_EQ(string(buf, n), "HEADTAIL");
}

TEST(WritevStdout, LargeScatterGather) {
	int pipefd[2];
	ASSERT_EQ(pipe(pipefd), 0);
	int saved_stdout = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);

	// 3 segments totaling ~96KB -- exceeds pipe buffer
	string seg1(32 * 1024, 'A');
	string seg2(32 * 1024, 'B');
	string seg3(32 * 1024, 'C');
	struct iovec iov[3];
	iov[0].iov_base = seg1.data();
	iov[0].iov_len = seg1.size();
	iov[1].iov_base = seg2.data();
	iov[1].iov_len = seg2.size();
	iov[2].iov_base = seg3.data();
	iov[2].iov_len = seg3.size();

	// Reader thread drains the pipe while writer may block
	string result;
	result.reserve(seg1.size() + seg2.size() + seg3.size());
	std::thread reader([&result, read_fd = pipefd[0]] {
		char buf[4096];
		ssize_t n;
		while ((n = read(read_fd, buf, sizeof(buf))) > 0) {
			result.append(buf, n);
		}
	});

	Tools::write_stdout_iov(iov, 3);

	dup2(saved_stdout, STDOUT_FILENO);
	close(saved_stdout);

	reader.join();
	close(pipefd[0]);

	string expected = seg1 + seg2 + seg3;
	EXPECT_EQ(result.size(), expected.size());
	EXPECT_EQ(result, expected);
}
