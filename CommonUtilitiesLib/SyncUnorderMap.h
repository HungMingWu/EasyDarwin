#pragma once

#include <mutex>
#include <unordered_map>

struct indexKey
{
	friend int operator ==(const indexKey &key1, const indexKey &key2) {
		if ((key1.fRemoteAddr == key2.fRemoteAddr) &&
			(key1.fRemotePort == key2.fRemotePort))
			return true;
		return false;
	}

	//data:
	uint32_t fRemoteAddr;
	uint16_t fRemotePort;
};

namespace std {

	template <>
	struct hash<indexKey>
	{
		std::size_t operator()(const indexKey& k) const
		{
			using std::size_t;
			using std::hash;

			// Compute individual hash values for first,
			// second and combine them using XOR
			// and bit shifting:

			return ((hash<uint32_t>()(k.fRemoteAddr)
				^ (hash<uint16_t>()(k.fRemotePort) << 1)) >> 1);
		}
	};

}

template <typename T>
class SyncUnorderMap
{
public:

	SyncUnorderMap() = default;
	~SyncUnorderMap() = default;

	bool RegisterTask(const indexKey &key, const T& item)
	{
		std::lock_guard<std::mutex> locker(fMutex);
		auto it = fHashTable.find(key);
		if (it != end(fHashTable)) return false;
		fHashTable.insert(std::make_pair(key, item));
		return true;
	}

	void UnregisterTask(const indexKey &key)
	{
		std::lock_guard<std::mutex> locker(fMutex);
		auto it = fHashTable.find(key);
		if (it != end(fHashTable))
			fHashTable.erase(it);
	}

	T GetTask(const indexKey &key)
	{
		std::lock_guard<std::mutex> locker(fMutex);
		auto it = fHashTable.find(key);
		if (it != end(fHashTable))
			return it->second;
		return {};
	}

	bool AddrInMap(const indexKey &key)
	{
		std::lock_guard<std::mutex> locker(fMutex);
		auto it = fHashTable.find(key);
		return it != end(fHashTable);
	}

	bool empty() {
		std::lock_guard<std::mutex> locker(fMutex);
		return fHashTable.empty();
	}

private:
	std::unordered_map<indexKey, T> fHashTable;
	std::mutex          fMutex; //this data structure is shared!
};