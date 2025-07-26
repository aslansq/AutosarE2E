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

	P11::P11(P11Functionality functionality, const P11Config& configRef) :
		functionality(functionality),
		config(configRef),
		count(0)
	{
		if(functionality == P11Functionality::CHECK) {
			count = countMaxVal;
		}
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

	uint8_t P11::getReadDataIdNibble(const std::vector<uint8_t>& frameRef) const
	{
		uint8_t nibble = 0;

		if(config.dataIdMode == P11DataIdModes::DATA_ID_NIBBLE) {
			uint32_t byteIdx = config.dataIdNibbleOffset / 8;
			uint32_t bitIdx = config.dataIdNibbleOffset % 8;

			if (byteIdx >= frameRef.size()) {
				throw std::runtime_error("Data ID nibble offset exceeds frame size.");
			}

			nibble = (frameRef[byteIdx] >> bitIdx) & 0x0F;
		}

		return nibble;
	}

	P11Status P11::check(const std::vector<uint8_t>& frameRef)
	{
		if(functionality != P11Functionality::CHECK) {
			throw std::runtime_error("Functionality must be CHECK for this method.");
		}

		uint8_t readNibble;
		uint8_t nibble;
		uint8_t readCount;
		uint8_t readCrc;
		uint8_t computedCRC;
		uint16_t deltaCount;

		if (!isInputVerified(frameRef.size())) {
			return P11Status::WRONG_INPUT;
		}

		if(config.dataIdMode == P11DataIdModes::DATA_ID_NIBBLE) {
			readNibble = getReadDataIdNibble(frameRef);
			nibble = (config.dataId & 0x00f0) >> 4;
		}

		readCount = getFrameCount(frameRef);
		readCrc = getFrameCrc(frameRef);
		computedCRC = computeCRC(frameRef);

		if(readCount >= count) {
			deltaCount = readCount - count;
		} else {
			deltaCount = (readCount + countMaxVal - count + 1); // Wrap around case
		}

		if(computedCRC != readCrc) {
			return P11Status::ERROR;
		}

		if((config.dataIdMode == P11DataIdModes::DATA_ID_NIBBLE) && (readNibble != nibble)) {
			return P11Status::ERROR;
		}

		//if(!(deltaCount <= config.maxDeltaCounter && deltaCount > 0)) {
		//	count = readCount;
		//	return P11Status::WRONG_SEQ;
		//}

		if(deltaCount == 0) {
			count = readCount;
			return P11Status::REPEATED;
		}

		if(deltaCount != 1) {
			count = readCount;
			return P11Status::OK_SOME_LOST;
		}

		count = readCount;
		return P11Status::OK; // Placeholder for actual implementation
	}

	void P11::protect(const std::vector<uint8_t>& frameRef, std::vector<uint8_t>& frameOutRef)
	{
		uint8_t calculatedCRC;
		uint32_t crcIdx;

		if(functionality != P11Functionality::PROTECT) {
			throw std::runtime_error("Functionality must be PROTECT for this method.");
		}

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
		if(config.dataLen != 64) {
			throw std::runtime_error("Data length must be 64 bits for this method.");
		}

		if(functionality != P11Functionality::PROTECT) {
			throw std::runtime_error("Functionality must be PROTECT for this method.");
		}

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
		std::vector<uint8_t> toBeCalcVect;
		std::vector<uint8_t> temp;
		toBeCalcVect.clear();
		temp.clear();

		if (config.crcOffset % 8 != 0) {
			throw std::runtime_error("CRC offset must be a multiple of 8.");
		}

		if (crcIdx >= frameOutRef.size()) {
			throw std::runtime_error("CRC offset exceeds frame size.");
		}

		if (config.dataIdMode == P11DataIdModes::DATA_ID_NIBBLE) {
			toBeCalcVect.push_back((uint8_t)(config.dataId & 0x00ff));
			toBeCalcVect.push_back(0);

		} else if(config.dataIdMode == P11DataIdModes::BOTH) {
			toBeCalcVect.push_back((uint8_t)(config.dataId & 0x00ff));
			toBeCalcVect.push_back((uint8_t)(config.dataId >> 8));
		}

		temp = frameOutRef;
		temp.erase(temp.begin() + crcIdx);

		for(const uint8_t& byte : temp) {
			toBeCalcVect.push_back(byte);
		}
		
		calculatedCRC = crc.calc_j1850(toBeCalcVect, 0xff, false);

		calculatedCRC ^= 0xff;

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
		if(count == (countMaxVal + 1)) {
			count = 0;
		}
	}
}
