
#include <cstdint>
#include <vector>

class Crc {
public:
	Crc();
	uint8_t calc_j1850(const std::vector<uint8_t>& dataRef, uint8_t startValue, bool isFirstCall) const;
	void get_j1850_table(std::vector<uint8_t>& tableRef) const;
	void gen_j1850_table(std::vector<uint8_t>& tableRef);
private:
	static const std::vector<uint8_t> j1850Table;
};