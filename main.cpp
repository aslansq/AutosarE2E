#include <iostream>
#include "e2e.hpp"

int main()
{
	std::vector<uint8_t> crcTable;
	Crc crc;
	crc.gen_j1850_table(crcTable);

	std::cout << "J1850 CRC table:" << std::endl;
	for (size_t row = 0; row < 16; ++row) {
		for (size_t col = 0; col < 16; ++col) {
			size_t idx = row * 16 + col;
			std::cout << std::hex << std::uppercase
				<< (crcTable[idx] < 16 ? "0" : "") << static_cast<int>(crcTable[idx]) << " ";
		}
		std::cout << std::endl;
	}

	std::cout << std::endl;

	E2E::P11Config config;
	config.dataId = 0x123;
	config.dataLen = 64;
	config.dataIdMode = E2E::P11DataIdModes::BOTH;
	config.counterOffset = 8;
	config.crcOffset = 0;
	config.dataIdNibbleOffset = 12;
	config.maxDeltaCounter = 3;

	E2E::P11 p11(config);
	std::vector<uint8_t> frame = { 0, 0, 0, 0, 0, 0, 0, 0};
	std::vector<uint8_t> protectedFrame(8);

	std::cout << "Protected frame:" << std::endl;
	for(int i = 0; i < 15; ++i) {
		p11.protect(frame, protectedFrame);
		for (const auto& byte : protectedFrame) {
			std::cout << std::hex << std::uppercase
				<< (byte < 16 ? "0" : "") << static_cast<int>(byte) << " ";
		}
		std::cout << std::endl;
	}
	return 0;
}