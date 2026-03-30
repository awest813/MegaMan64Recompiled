#ifndef MM64_YAZ0_H
#define MM64_YAZ0_H

#include <span>

void yaz0_decompress(std::span<const uint8_t> input, std::span<uint8_t> output);

#endif
