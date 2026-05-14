#ifndef LRU_CACHE_SV_SERVER_H
#define LRU_CACHE_SV_SERVER_H
#include <memory>
#include <string>

namespace net {
    class server {
    public:
        server(std::uint16_t port, std::string listeningAddr) : _port(port),
                                                                _listeningAddr(std::move(listeningAddr)) {};
        virtual ~server() = default;
        virtual void listen() = 0;
    private:
        std::uint16_t _port;
        std::string _listeningAddr;
    };
}

#endif //LRU_CACHE_SV_SERVER_H