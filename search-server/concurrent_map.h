#include <algorithm>
#include <cstdlib>
#include <execution>
#include <future>
#include <map>
#include <mutex>
#include <numeric>
#include <random>
#include <string>
#include <vector>

template <typename Key, typename Value>
class ConcurrentMap {
 public:
  static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

  struct Access {
    std::lock_guard<std::mutex> guard;
    Value& ref_to_value;
  };

  explicit ConcurrentMap(size_t bucket_count)
      : buckets_(bucket_count), bucket_count_(bucket_count), mut_(bucket_count) {}

  Access operator[](const Key& key) {
    const size_t hash = key % bucket_count_;
    return {std::lock_guard<std::mutex>(mut_[hash]), buckets_[hash][key]};
  }

  Access at(const Key& key) {
    const size_t hash = key % bucket_count_;
    return {std::lock_guard<std::mutex>(mut_[hash]), buckets_[hash].at(key)};
  }

  auto erase(const Key& key) {
    const size_t hash = key % bucket_count_;
    const std::lock_guard<std::mutex> lock{mut_[hash]};
    return buckets_[hash].erase(key);
  }

  size_t size() const {

    return std::transform_reduce(std::execution::par, buckets_.cbegin(), buckets_.cend(),
                                 size_t{}, std::plus<>{},
                                 [](const auto& el) { return el.size(); });
    ;
  }

  std::map<Key, Value> BuildOrdinaryMap() {
    std::map<Key, Value> result;
    for (size_t i = 0; i < bucket_count_; ++i) {
      std::lock_guard<std::mutex> lock(mut_[i]);
      for (const auto& [key, value] : buckets_[i]) {
        result[key] = value;
      }
    }
    return result;
  }
  std::vector<std::pair<Key, Value>> BuildFlatContainer() {
    std::vector<std::pair<Key, Value>> result;
    result.reserve(bucket_count_);
    for (size_t i = 0; i < bucket_count_; ++i) {
      std::lock_guard<std::mutex> lock(mut_[i]);
      for (const auto& [key, value] : buckets_[i]) {
        result.emplace_back(key, value);
      }
    }
    return result;
  }

 private:
  std::vector<std::map<Key, Value>> buckets_;
  const size_t bucket_count_;
  std::vector<std::mutex> mut_;
};