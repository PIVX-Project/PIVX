#pragma once
#include "uint256.h"
#include "arith_uint256.h"

/** 512-bit unsigned big integer. */
class uint512 : public base_uint<512> {
public:
    uint512() {}
    uint512(const base_uint<512>& b) : base_uint<512>(b) {}
    uint512(uint64_t b) : base_uint<512>(b) {}
    //explicit uint512(const std::vector<unsigned char>& vch) : base_uint<512>(vch) {}
    explicit uint512(const std::string& str) : base_uint<512>(str) {}

	  uint256 trim256() const
	  {
          std::vector<unsigned char> vch;
          const unsigned char* p = this->begin();
          for (unsigned int i = 0; i < 32; i++){
              vch.push_back(*p++);
          }
          uint256 retval(vch);
          return retval;
		}
};
