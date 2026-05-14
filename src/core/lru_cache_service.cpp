#include "core/lru_cache_service.h"

#include <string>
#include <utility>

template<typename Key, typename Value>
core::lru_cache_service<Key, Value>::lru_cache_service(size_t capacity)
    : _capacity(capacity),
      _list(),
      _map()
{
    _map.reserve(_capacity);
}

template<typename Key, typename Value>
core::lru_cache_service<Key, Value>::lru_cache_service(lru_cache_service&& other) noexcept
    : _capacity(std::exchange(other._capacity, 0)),
      _list(std::move(other._list)),
      _map(std::move(other._map))
{
}

template<typename Key, typename Value>
core::lru_cache_service<Key, Value>&
core::lru_cache_service<Key, Value>::operator=(lru_cache_service&& other) noexcept {
    if (this != &other) {
        _capacity = std::exchange(other._capacity, 0);
        _list = std::move(other._list);
        _map = std::move(other._map);
    }
    return *this;
}

template<typename Key, typename Value>
std::optional<Value> core::lru_cache_service<Key, Value>::getValue(const Key& key) {
    auto it = _map.find(key);

    if (it == _map.end())
        return std::nullopt;

    // Promote to MRU position.
    auto found = it->second;
    _list.splice(_list.begin(), _list, found);

    return found->second;
}

template<typename Key, typename Value>
void core::lru_cache_service<Key, Value>::putValue(Key&& key, Value&& value) {
    auto it = _map.find(key);

    // Existing key: overwrite value and promote.
    if (it != _map.end()) {
        it->second->second = std::move(value);
        _list.splice(_list.begin(), _list, it->second);
        return;
    }

    // At capacity: evict LRU before inserting.
    if (_map.size() == _capacity) {
        auto lru = std::prev(_list.end());
        _map.erase(lru->first);
        _list.pop_back();
    }

    _list.emplace_front(std::move(key), std::move(value));
    _map.emplace(_list.begin()->first, _list.begin());
}

template class core::lru_cache_service<std::string, std::string>;
