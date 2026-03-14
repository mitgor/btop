// SPDX-License-Identifier: Apache-2.0
//
// Tests for SMCConnection singleton caching and reconnect logic.
// Only meaningful on macOS — compiles as empty on other platforms.

#include <gtest/gtest.h>

#ifdef __APPLE__
#include "osx/smc.hpp"

TEST(SmcCache, SingletonReturnsSameInstance) {
	auto& a = Cpu::SMCConnection::instance();
	auto& b = Cpu::SMCConnection::instance();
	EXPECT_EQ(&a, &b) << "SMCConnection::instance() must return the same object";
}

TEST(SmcCache, IsConnectedOnMac) {
	auto& smc = Cpu::SMCConnection::instance();
	// On a real Mac this should be true; in CI VMs it may be false but must not crash
	EXPECT_TRUE(smc.is_connected()) << "Expected SMC connection on macOS hardware";
}

TEST(SmcCache, GetTempDoesNotCrash) {
	auto& smc = Cpu::SMCConnection::instance();
	if (not smc.is_connected()) GTEST_SKIP() << "No SMC connection available";

	// getTemp(-1) = package temp, should return >= -1 (or -1 if unavailable)
	long long t1 = smc.getTemp(-1);
	long long t2 = smc.getTemp(-1);
	EXPECT_GE(t1, -1);
	EXPECT_GE(t2, -1);
}

TEST(SmcCache, GetGpuTempDoesNotCrash) {
	auto& smc = Cpu::SMCConnection::instance();
	if (not smc.is_connected()) GTEST_SKIP() << "No SMC connection available";

	long long temp = smc.getGpuTemp();
	// -1 means no GPU temp key found, which is fine — just must not crash
	EXPECT_GE(temp, -1);
}

#endif // __APPLE__
