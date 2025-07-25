#include <cstdint>
#include <vector>
#include "crc.hpp"

namespace E2E {
	enum class P11DataIdModes {
		BOTH = 0,
		DATA_ID_NIBBLE = 1
	};

	enum class P11Status {
		NO_NEW_DATA,
		ERROR,
		WRONG_SEQ,
		WRONG_INPUT,
		REPEATED,
		OK_SOME_LOST,
		OK
	};

	class P11Config {
	public:
		P11Config();
		P11Config(const P11Config& other);

		uint16_t dataId;
		uint32_t dataLen; // in bits
		P11DataIdModes dataIdMode;
		uint32_t counterOffset; // in bits
		uint32_t crcOffset; // in bits
		uint32_t dataIdNibbleOffset; // in bits
		uint32_t maxDeltaCounter;
	};

	class P11 {
	public:
		P11(const P11Config& configRef);
		P11Status check(const std::vector<uint8_t>& frameRef);
		void protect(const std::vector<uint8_t>& frameRef, std::vector<uint8_t>& frameOutRef);
		uint64_t protect(uint64_t frame);
	private:
		uint8_t getFrameCrc(const std::vector<uint8_t>& frameRef) const;
		uint8_t getFrameCount(const std::vector<uint8_t>& frameRef) const;
		uint8_t computeCRC(const std::vector<uint8_t>& frameOutRef) const;
		void writeDataIdNibble(std::vector<uint8_t>& frameOutRef);
		void writeCounter(std::vector<uint8_t>& frameOutRef);
		bool isInputVerified(uint32_t frameLen) const;
		void countIncrement();
		P11Config config;
		uint8_t count;
		Crc crc;
	};
}