#ifndef LRU_CACHE_SV_LRU_SERVICE_H
#define LRU_CACHE_SV_LRU_SERVICE_H
#include <list>
#include <optional>
#include <unordered_map>

namespace core {
    template <typename Key, typename Value>
    class lru_cache_service {
        // Internal types
        using node_type = std::pair<Key, Value>;

    public:
        // Key& key: references the key to the dictionary cache.
        // RETURNS: the value being hold in the cache.
        std::optional<Value> getValue(const Key& key);

        // Key& key, Value& value: the pair being put in the dictionary cache
        // RETURNS: nothing.
        void putValue(Key&& key, Value&& value);

        explicit lru_cache_service(size_t capacity);

        ~lru_cache_service() = default;

        lru_cache_service(const lru_cache_service&) = delete;

        lru_cache_service(lru_cache_service&&) noexcept;

        lru_cache_service& operator=(const lru_cache_service&) = delete;

        lru_cache_service& operator=(lru_cache_service&&) noexcept;
    private:
        size_t _capacity;
        std::list<node_type> _list;
        std::unordered_map<Key, typename std::list<node_type>::iterator> _map;
    };
}

#endif //LRU_CACHE_SV_LRU_SERVICE_H