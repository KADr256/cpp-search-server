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

	explicit ConcurrentMap(size_t bucket_count) :bucket_count_(bucket_count), mutex_vector_(bucket_count), splitted_map_(bucket_count) {
	};

	Access operator[](const Key& key) {
		const uint64_t bucket_access = static_cast<uint64_t>(key) % bucket_count_;
		mutex_vector_[bucket_access].lock();
		return Access(mutex_vector_[bucket_access], splitted_map_[bucket_access][key]);
	};

	std::map<Key, Value> BuildOrdinaryMap() {
		std::map<Key, Value> result;
		for (size_t i = 0; i < bucket_count_; ++i) {
			std::lock_guard guard(mutex_vector_[i]);
			result.merge(splitted_map_[i]);
		}
		return result;
	};

	void erase(const Key& key) {
		const uint64_t bucket_access = static_cast<uint64_t>(key) % bucket_count_;
		{
			std::lock_guard guard(mutex_vector_[bucket_access]);
			splitted_map_[bucket_access].erase(key);
		}
	}

private:
	size_t bucket_count_;
	std::vector<std::mutex> mutex_vector_;
	std::vector<std::map<Key, Value>> splitted_map_;
};
