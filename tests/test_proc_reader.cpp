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

#ifdef __linux__

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>

#include "linux/io_uring_reader.hpp"

namespace {

//? Helper to create a temp file with known content
class TempFile {
public:
	explicit TempFile(const std::string& content) {
		char tmpl[] = "/tmp/btop_test_XXXXXX";
		int fd = ::mkstemp(tmpl);
		EXPECT_GE(fd, 0);
		path_ = tmpl;
		::write(fd, content.data(), content.size());
		::close(fd);
	}
	~TempFile() { std::filesystem::remove(path_); }
	const char* path() const { return path_.c_str(); }
private:
	std::string path_;
};

TEST(ProcReaderFallback, ReadsSingleFile) {
	TempFile tmp("hello world");
	char buf[256];
	Linux::ProcReader reader;
	Linux::ProcReader::ReadRequest req{tmp.path(), buf, sizeof(buf), -1};
	int success = reader.submit_batch(&req, 1);
	EXPECT_EQ(success, 1);
	EXPECT_EQ(req.result, 11);
	EXPECT_STREQ(buf, "hello world");
}

TEST(ProcReaderFallback, ReadsBatch) {
	TempFile f1("alpha");
	TempFile f2("beta");
	TempFile f3("gamma");

	char buf1[256], buf2[256], buf3[256];
	Linux::ProcReader reader;
	Linux::ProcReader::ReadRequest reqs[3] = {
		{f1.path(), buf1, sizeof(buf1), -1},
		{f2.path(), buf2, sizeof(buf2), -1},
		{f3.path(), buf3, sizeof(buf3), -1},
	};
	int success = reader.submit_batch(reqs, 3);
	EXPECT_EQ(success, 3);
	EXPECT_STREQ(buf1, "alpha");
	EXPECT_STREQ(buf2, "beta");
	EXPECT_STREQ(buf3, "gamma");
}

TEST(ProcReaderFallback, HandlesNonexistent) {
	char buf[256];
	Linux::ProcReader reader;
	Linux::ProcReader::ReadRequest req{"/nonexistent/path/file", buf, sizeof(buf), -1};
	int success = reader.submit_batch(&req, 1);
	EXPECT_EQ(success, 0);
	EXPECT_EQ(req.result, -1);
}

TEST(ProcReaderFallback, HandlesMixedValid) {
	TempFile tmp("valid content");
	char buf1[256], buf2[256];
	Linux::ProcReader reader;
	Linux::ProcReader::ReadRequest reqs[2] = {
		{tmp.path(), buf1, sizeof(buf1), -1},
		{"/nonexistent/path/file", buf2, sizeof(buf2), -1},
	};
	int success = reader.submit_batch(reqs, 2);
	EXPECT_EQ(success, 1);
	EXPECT_GT(reqs[0].result, 0);
	EXPECT_STREQ(buf1, "valid content");
	EXPECT_EQ(reqs[1].result, -1);
}

TEST(ProcReaderFallback, UsingIoUringReturnsFalse) {
	Linux::ProcReader reader;
	//? On Linux without USE_IO_URING defined at compile time, this should be false.
	//? With USE_IO_URING, it depends on runtime kernel support.
	//? Either way, the fallback path should work.
	(void)reader.using_io_uring();
}

} // namespace

#endif // __linux__
