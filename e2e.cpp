#include <cstdint>
#include <vector>
#include <stdexcept>
#include "e2e.hpp"

namespace E2E {
	P11Config::P11Config() :
		dataId(0),
		dataLen(0),
		dataIdMode(P11DataIdModes::BOTH),
		counterOffset(0),
		crcOffset(0),
		dataIdNibbleOffset(0),
		maxDeltaCounter(0)
	{
	}

	P11Config::P11Config(const P11Config& other) :
		dataId(other.dataId),
		dataLen(other.dataLen),
		dataIdMode(other.dataIdMode),
		counterOffset(other.counterOffset),
		crcOffset(other.crcOffset),
		dataIdNibbleOffset(other.dataIdNibbleOffset),
		maxDeltaCounter(other.maxDeltaCounter)
	{
	}

	P11::P11(const P11Config& configRef) :
		config(configRef),
		count(0)
	{
	}

	uint8_t P11::getFrameCrc(const std::vector<uint8_t>& frameRef) const
	{
		uint32_t crcIdx = config.crcOffset / 8;

		if (crcIdx >= frameRef.size()) {
			throw std::runtime_error("CRC offset exceeds frame size.");
		}

		if(crcIdx % 8 != 0) {
			throw std::runtime_error("CRC offset must be a multiple of 8.");
		}

		return frameRef[crcIdx];
	}

	uint8_t P11::getFrameCount(const std::vector<uint8_t>& frameRef) const
	{
		uint32_t byteIdx = config.counterOffset / 8;
		uint32_t bitIdx = config.counterOffset % 8;

		if (byteIdx >= frameRef.size()) {
			throw std::runtime_error("Counter offset exceeds frame size.");
		}

		return (frameRef[byteIdx] >> bitIdx) & 0x0F;
	}

	P11Status P11::check(const std::vector<uint8_t>& frameRef)
	{
		uint8_t rxCrc;
		uint8_t calculatedCrc;
		uint8_t rxFrameCount;
		uint8_t expectedFrameCount;
		uint8_t deltaCount;

		std::vector<uint8_t> protectedFrame;
		protectedFrame.clear();

		if(frameRef.size() != (config.dataLen / 8)) {
			return P11Status::WRONG_INPUT;
		}

		protect(frameRef, protectedFrame);
		count--;
		rxCrc = getFrameCrc(frameRef);
		calculatedCrc = getFrameCrc(protectedFrame);

		if(rxCrc != calculatedCrc) {
			return P11Status::ERROR;
		}

		rxFrameCount = getFrameCount(frameRef);
		expectedFrameCount = getFrameCount(protectedFrame);

		if(rxFrameCount == expectedFrameCount) {
			return P11Status::REPEATED;
		}

		if(rxFrameCount < expectedFrameCount) {
			deltaCount = expectedFrameCount + 0x0F - rxFrameCount;
		} else {
			deltaCount = rxFrameCount - expectedFrameCount;
		}

		if(deltaCount != 0 && deltaCount < config.maxDeltaCounter) {
			return P11Status::OK_SOME_LOST;
		}

		if(deltaCount > config.maxDeltaCounter) {
			return P11Status::ERROR;
		}

		count++;
		return P11Status::OK;
	}

	void P11::protect(const std::vector<uint8_t>& frameRef, std::vector<uint8_t>& frameOutRef)
	{
		uint8_t calculatedCRC;
		uint32_t crcIdx;

		if(!isInputVerified(frameRef.size())) {
			throw std::runtime_error("Input frame length does not match expected length.");
		}

		frameOutRef = frameRef; // Copy input frame to output frame

		if(config.dataIdMode == P11DataIdModes::DATA_ID_NIBBLE) {
			writeDataIdNibble(frameOutRef);
		}

		writeCounter(frameOutRef);

		calculatedCRC = computeCRC(frameOutRef);

		crcIdx = config.crcOffset / 8;

		frameOutRef[crcIdx] = calculatedCRC;

		countIncrement();
	}

	uint64_t P11::protect(uint64_t frame)
	{
		std::vector<uint8_t> frameRef(8);
		std::vector<uint8_t> frameOutRef(8);

		for (int i = 0; i < 8; ++i) {
			frameRef[i] = (frame >> (i * 8)) & 0xFF;
		}

		protect(frameRef, frameOutRef);

		uint64_t protectedFrame = 0;
		for (int i = 0; i < 8; ++i) {
			protectedFrame |= (static_cast<uint64_t>(frameOutRef[i]) << (i * 8));
		}

		return protectedFrame;
	}

	uint8_t P11::computeCRC(const std::vector<uint8_t>& frameOutRef) const
	{
		uint8_t calculatedCRC = 0;
		uint32_t crcIdx = config.crcOffset / 8;
		std::vector<uint8_t> temp;

		if (config.crcOffset % 8 != 0) {
			throw std::runtime_error("CRC offset must be a multiple of 8.");
		}

		if (crcIdx >= frameOutRef.size()) {
			throw std::runtime_error("CRC offset exceeds frame size.");
		}

		if (config.dataIdMode == P11DataIdModes::DATA_ID_NIBBLE) {
			temp.clear();
			temp.push_back((uint8_t)(config.dataId & 0x00ff));
			calculatedCRC = crc.calc_j1850(temp, 0xff, false);

			temp.clear();
			temp.push_back(0);
			calculatedCRC = crc.calc_j1850(temp, calculatedCRC, false);

			temp.clear();
			temp = frameOutRef;
			temp.erase(temp.begin() + crcIdx);
			calculatedCRC = crc.calc_j1850(temp, calculatedCRC, false);
		} else if(config.dataIdMode == P11DataIdModes::BOTH) {
			temp.clear();
			temp.push_back((uint8_t)(config.dataId & 0x00ff));
			calculatedCRC = crc.calc_j1850(temp, 0xff, false);

			temp.clear();
			temp.push_back((uint8_t)config.dataId >> 8);
			calculatedCRC = crc.calc_j1850(temp, calculatedCRC, false);

			temp.clear();
			temp = frameOutRef;
			temp.erase(temp.begin() + crcIdx);
			calculatedCRC = crc.calc_j1850(temp, calculatedCRC, false);

			calculatedCRC ^= 0xff;
		}

		return calculatedCRC;
	}

	void P11::writeDataIdNibble(std::vector<uint8_t>& frameOutRef)
	{
		// the high nibble of high byte of DataID is not used (it is 0x0), as the DataID is
		// limited to 12 bits,
		uint16_t dataId = config.dataId & 0x0FFF;

		// the low nibble of high byte of DataID is transmitted explicitly and covered by CRC
		//calculation when computing the CRC over Data.
		uint8_t lowNibbleOfHighByte = (dataId >> 8) & 0x0F;

		uint32_t byteIdx = config.dataIdNibbleOffset / 8;
		uint32_t bitIdx = config.dataIdNibbleOffset % 8;

		if (byteIdx >= frameOutRef.size()) {
			throw std::runtime_error("Data ID nibble offset exceeds frame size.");
		}

		// Clear the bits where the nibble will be written
		frameOutRef[byteIdx] &= ~(0x0F << bitIdx);
		// Write the low nibble of the high byte of DataID
		frameOutRef[byteIdx] |= (lowNibbleOfHighByte << bitIdx);
	}

	void P11::writeCounter(std::vector<uint8_t>& frameOutRef)
	{
		uint32_t byteIdx = config.counterOffset / 8;
		uint32_t bitIdx = config.counterOffset % 8;

		if (byteIdx >= frameOutRef.size()) {
			throw std::runtime_error("Counter offset exceeds frame size.");
		}

		if (bitIdx > 4) {
			throw std::runtime_error("Counter offset bit index must be 0-4.");
		}

		// Clear the bits where the counter will be written
		frameOutRef[byteIdx] &= ~(0x0F << bitIdx);
		// Write the counter value
		frameOutRef[byteIdx] |= (count << bitIdx);
	}

	bool P11::isInputVerified(uint32_t frameLen) const
	{
		return ((config.dataLen / 8) == frameLen);
	}

	void P11::countIncrement()
	{
		count++;
		if(count == 0x0F) {
			count = 0;
		}
	}
}
