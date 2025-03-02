#pragma once

#include <string>
#include <vector>

/**
 * Base64 encoding/decoding functions
 */
std::string base64_encode(const unsigned char* data, size_t size);
std::vector<unsigned char> base64_decode(const std::string& input);