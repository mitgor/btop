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

#include <variant>

#include <gtest/gtest.h>
#include "btop_menu_pda.hpp"

// --- Minimal compile check for Task 1 TDD RED ---

TEST(MenuFrameVar, HasExactlyEightAlternatives) {
	EXPECT_EQ(std::variant_size_v<menu::MenuFrameVar>, 8u);
}
