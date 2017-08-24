#ifndef _BASE64_H_
#define _BASE64_H_

#include <vector>
#include <string>

std::string base64_encode(const char* buf, unsigned int bufLen);
std::vector<char> base64_decode(std::string const&);

#endif