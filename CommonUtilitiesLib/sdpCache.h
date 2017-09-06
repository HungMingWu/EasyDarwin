#ifndef __SDPCACHE_H__
#define __SDPCACHE_H__

#include <boost/utility/string_view.hpp>

class CSdpCache
{
private:
	CSdpCache()	= default;
public:
	~CSdpCache() = default;

	static CSdpCache* GetInstance();

	void setSdpMap(boost::string_view path, boost::string_view context);

	boost::string_view getSdpMap(boost::string_view path);

	void eraseSdpMap(boost::string_view path);
};
#endif
