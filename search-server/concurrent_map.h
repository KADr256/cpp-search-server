#pragma once

#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>
#include <utility>

template <typename Key, typename Value>
class ConcurrentMap {
public:
	static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");//s

	struct Access {
		explicit Access(std::mutex& m_inc, Value& ref_to_value_request) :m(m_inc), ref_to_value(ref_to_value_request) {
		}
		~Access() {
			m.unlock();
		}
		std::mutex& m;
		Value& ref_to_value;
	};

	explicit ConcurrentMap(size_t bucket_count) :splitted_map_(bucket_count) {
	};

	Access operator[](const Key& key) {
		const uint64_t bucket_access = static_cast<uint64_t>(key) % splitted_map_.size();
		splitted_map_[bucket_access].mutex.lock();
		return Access(splitted_map_[bucket_access].mutex, splitted_map_[bucket_access].map[key]);
	};

	std::map<Key, Value> BuildOrdinaryMap() {
		std::map<Key, Value> result;
		for (auto& data:splitted_map_) {
			std::lock_guard guard(data.mutex);
			result.merge(data.map);
		}
		return result;
	};

	void erase(const Key& key) {
		const uint64_t bucket_access = static_cast<uint64_t>(key) % splitted_map_.size();
		{
			std::lock_guard guard(splitted_map_[bucket_access].mutex);
			splitted_map_[bucket_access].map.erase(key);
		}
	}

private:
	struct Bucket {
		std::mutex mutex;
		std::map<Key, Value> map;
	};

	std::vector<Bucket> splitted_map_;
};
