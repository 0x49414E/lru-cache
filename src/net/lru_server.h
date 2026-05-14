#ifndef LRU_CACHE_SV_LRU_SERVER_H
#define LRU_CACHE_SV_LRU_SERVER_H
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "net/server.h"
#define BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio.hpp>
#include "core/lru_cache_service.h"
#include <boost/asio/awaitable.hpp>

using boost::asio::awaitable;
using boost::asio::io_context;
using boost::asio::ip::tcp;

template<typename Key, typename Value>
using lru_cache_service = core::lru_cache_service<Key, Value>;

namespace net {
    template<typename Key, typename Value>
    class lru_server : public server {
    public:
        lru_server(std::uint16_t port, std::string listeningAddr);

        ~lru_server() override = default;

        void listen() override;
    private:
        awaitable<void> accept_connections();
        awaitable<void> handle_connection(tcp::socket socket);

        awaitable<void> cmd_ping(tcp::socket& sock, const std::vector<std::string>& args);
        awaitable<void> cmd_command(tcp::socket& sock);
        awaitable<void> cmd_get(tcp::socket& sock, const std::vector<std::string>& args);
        awaitable<void> cmd_set(tcp::socket& sock, std::vector<std::string>& args);
        awaitable<void> cmd_unknown(tcp::socket& sock, std::string_view verb);

        io_context _io_context;
        tcp::acceptor _acceptor;
        std::unique_ptr<lru_cache_service<Key, Value>> _cache;
    };
}

#endif // LRU_CACHE_SV_LRU_SERVER_H
