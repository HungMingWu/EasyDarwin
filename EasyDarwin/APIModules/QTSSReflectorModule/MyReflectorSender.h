#pragma once
#include <cstdint>
class MyReflectorStream;
class MyReflectorSender
{
	MyReflectorStream*    fStream;
	uint32_t              fWriteFlag;
public:
	MyReflectorSender(MyReflectorStream* inStream, uint32_t inWriteFlag);
	~MyReflectorSender() = default;
};