#include "sdpCache.h"
#include <unordered_map>
#include <string>

using namespace std;

static unordered_map<string, string> sdpmap;

CSdpCache* CSdpCache::GetInstance()
{
	static CSdpCache cache;
	return &cache;
}

void CSdpCache::setSdpMap(boost::string_view path, boost::string_view context)
{
	if (path.empty() || context.empty())
		return;

	sdpmap[string(path)] = string(context);
}

boost::string_view CSdpCache::getSdpMap(boost::string_view path)
{
	auto it = sdpmap.find(string(path));
	if (it == sdpmap.end())
		return {};

	return it->second;
}

void CSdpCache::eraseSdpMap(boost::string_view path)
{
	auto it = sdpmap.find(string(path));
	if (it == sdpmap.end())
		return;
	sdpmap.erase(it);
}