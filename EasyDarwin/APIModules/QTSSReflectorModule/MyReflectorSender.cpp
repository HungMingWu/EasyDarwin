#include "MyReflectorSender.h"
MyReflectorSender::MyReflectorSender(MyReflectorStream* inStream, uint32_t inWriteFlag)
	: fStream(inStream),
	fWriteFlag(inWriteFlag)
{
}
