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
#include "btop_tools.hpp"

using std::string;

TEST(UncolorTest, EmptyString) {
	EXPECT_EQ(Fx::uncolor(""), "");
}

TEST(UncolorTest, NoEscapes) {
	EXPECT_EQ(Fx::uncolor("hello world"), "hello world");
}

TEST(UncolorTest, OnlyEscapes) {
	EXPECT_EQ(Fx::uncolor("\033[0m\033[1m\033[38;5;196m"), "");
}

TEST(UncolorTest, ResetSequence) {
	EXPECT_EQ(Fx::uncolor("\033[0mreset text"), "reset text");
}

TEST(UncolorTest, BoldSequence) {
	EXPECT_EQ(Fx::uncolor("\033[1mbold\033[0m"), "bold");
}

TEST(UncolorTest, Color256Foreground) {
	EXPECT_EQ(Fx::uncolor("\033[38;5;196mred text\033[0m"), "red text");
}

TEST(UncolorTest, Color256Background) {
	EXPECT_EQ(Fx::uncolor("\033[48;5;21mblue bg\033[0m"), "blue bg");
}

TEST(UncolorTest, TruecolorForeground) {
	EXPECT_EQ(Fx::uncolor("\033[38;2;255;128;0morange\033[0m"), "orange");
}

TEST(UncolorTest, TruecolorBackground) {
	EXPECT_EQ(Fx::uncolor("\033[48;2;0;255;0mgreen bg\033[0m"), "green bg");
}

TEST(UncolorTest, ConsecutiveEscapes) {
	EXPECT_EQ(Fx::uncolor("\033[1m\033[38;2;255;0;0mbold red\033[0m"), "bold red");
}

TEST(UncolorTest, MixedTextAndEscapes) {
	string input = "CPU: \033[38;2;255;0;0m95%\033[0m | MEM: \033[38;5;21m4.2G\033[0m";
	EXPECT_EQ(Fx::uncolor(input), "CPU: 95% | MEM: 4.2G");
}

TEST(UncolorTest, NonSGRSequencePreserved) {
	// Cursor movement (\033[10C) should NOT be stripped (ends in C, not m)
	// The ESC[ prefix will be consumed by the parser since it looks
	// for digits/semicolons then 'm'. When 'm' is not found, the ESC char
	// is copied. Verify the text portion survives.
	string input = "before\033[10Cafter";
	string result = Fx::uncolor(input);
	// The non-SGR sequence is not stripped -- text around it preserved
	EXPECT_TRUE(result.find("before") != string::npos);
	EXPECT_TRUE(result.find("after") != string::npos);
}

TEST(UncolorTest, PartialEscapeAtEnd) {
	// String ending with incomplete escape
	EXPECT_EQ(Fx::uncolor("text\033"), "text\033");
	EXPECT_EQ(Fx::uncolor("text\033["), "text\033[");
	EXPECT_EQ(Fx::uncolor("text\033[38"), "text\033[38");
}

TEST(UncolorTest, LargeString) {
	// Simulate a typical btop output line with many escapes
	string line;
	for (int i = 0; i < 100; i++) {
		line += "\033[38;2;" + std::to_string(i) + ";128;0m" + "X";
	}
	line += "\033[0m";
	string result = Fx::uncolor(line);
	EXPECT_EQ(result, string(100, 'X'));
}
