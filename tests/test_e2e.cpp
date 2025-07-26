#include <gtest/gtest.h>
#include "e2e.hpp"

TEST(E2ETest, P11Protect) {
	E2E::P11Config config;
	config.dataId = 0x123;
	config.dataLen = 64;
	config.dataIdMode = E2E::P11DataIdModes::BOTH;
	config.counterOffset = 8;
	config.crcOffset = 0;
	config.dataIdNibbleOffset = 12;
	config.maxDeltaCounter = 3;

	E2E::P11 p11(config);
	std::vector<uint8_t> frame = {
		0, 0, 0, 0,
		0, 0, 0, 0
	};
	std::vector<uint8_t> protectedFrame(8);
	std::vector<uint8_t> expectedProtectedFrame = {
		0xcc, 0x0, 0x0, 0x0,
		0x0 , 0x0, 0x0, 0x0
	};
	p11.protect(frame, protectedFrame);
	EXPECT_EQ(expectedProtectedFrame, protectedFrame);

	p11.protect(frame, protectedFrame);
	expectedProtectedFrame = {
		0x91, 0x1, 0x0, 0x0,
		0x0 , 0x0, 0x0, 0x0
	};
	EXPECT_EQ(expectedProtectedFrame, protectedFrame);
}