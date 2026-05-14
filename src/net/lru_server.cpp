#include "net/lru_server.h"
#include "net/server.h"

#define BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/this_coro.hpp>

#include <cctype>
#include <iostream>
#include <istream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using boost::asio::use_awaitable;
using boost::asio::awaitable;
using boost::asio::detached;

namespace {

// Build a TCP endpoint from an address string and port. Empty address binds to v4 any.
tcp::endpoint make_endpoint(const std::string& addr, std::uint16_t port) {
    if (addr.empty()) {
        return tcp::endpoint(tcp::v4(), port);
    }
    return tcp::endpoint(boost::asio::ip::make_address(addr), port);
}

awaitable<std::string> read_line(tcp::socket& sock, boost::asio::streambuf& buf) {
    co_await boost::asio::async_read_until(sock, buf, "\r\n", use_awaitable);
    std::istream is(&buf);
    std::string line;
    std::getline(is, line);
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    co_return line;
}

awaitable<std::string> read_exact(tcp::socket& sock, boost::asio::streambuf& buf, std::size_t n) {
    if (buf.size() < n) {
        co_await boost::asio::async_read(
            sock, buf,
            boost::asio::transfer_exactly(n - buf.size()),
            use_awaitable);
    }
    std::istream is(&buf);
    std::string out(n, '\0');
    is.read(out.data(), static_cast<std::streamsize>(n));
    co_return out;
}

awaitable<std::optional<std::vector<std::string>>>
read_command(tcp::socket& sock, boost::asio::streambuf& buf) {
    boost::system::error_code ec;
    co_await boost::asio::async_read_until(
        sock, buf, "\r\n",
        boost::asio::redirect_error(use_awaitable, ec));

    if (ec == boost::asio::error::eof ||
        ec == boost::asio::error::connection_reset ||
        ec == boost::asio::error::operation_aborted) {
        co_return std::nullopt;
    }
    if (ec) {
        throw boost::system::system_error(ec);
    }

    std::istream is(&buf);
    std::string header;
    std::getline(is, header);
    if (!header.empty() && header.back() == '\r') {
        header.pop_back();
    }
    if (header.empty()) {
        co_return std::nullopt;
    }

    if (header[0] == '*') {
        const int count = std::stoi(header.substr(1));
        if (count < 0) {
            co_return std::nullopt;
        }
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            std::string bulk_header = co_await read_line(sock, buf);
            if (bulk_header.empty() || bulk_header[0] != '$') {
                throw std::runtime_error("Protocol error: expected bulk string");
            }
            const int len = std::stoi(bulk_header.substr(1));
            if (len < 0) {
                args.emplace_back();
                continue;
            }
            std::string payload = co_await read_exact(sock, buf, static_cast<std::size_t>(len));
            // Consume trailing CRLF after bulk payload.
            co_await read_exact(sock, buf, 2);
            args.push_back(std::move(payload));
        }
        co_return args;
    }

    std::vector<std::string> args;
    std::istringstream iss(header);
    std::string tok;
    while (iss >> tok) {
        args.push_back(std::move(tok));
    }
    if (args.empty()) {
        co_return std::nullopt;
    }
    co_return args;
}

awaitable<void> write_all(tcp::socket& sock, std::string data) {
    co_await boost::asio::async_write(sock, boost::asio::buffer(data), use_awaitable);
}

awaitable<void> write_simple(tcp::socket& sock, std::string_view s) {
    std::string out;
    out.reserve(s.size() + 3);
    out.push_back('+');
    out.append(s);
    out.append("\r\n");
    co_await write_all(sock, std::move(out));
}

awaitable<void> write_error(tcp::socket& sock, std::string_view msg) {
    std::string out;
    out.reserve(msg.size() + 7);
    out.append("-ERR ");
    out.append(msg);
    out.append("\r\n");
    co_await write_all(sock, std::move(out));
}

awaitable<void> write_bulk(tcp::socket& sock, std::string_view s) {
    std::string out;
    out.reserve(s.size() + 16);
    out.push_back('$');
    out.append(std::to_string(s.size()));
    out.append("\r\n");
    out.append(s);
    out.append("\r\n");
    co_await write_all(sock, std::move(out));
}

awaitable<void> write_nil(tcp::socket& sock) {
    co_await write_all(sock, "$-1\r\n");
}

awaitable<void> write_empty_array(tcp::socket& sock) {
    co_await write_all(sock, "*0\r\n");
}

std::string to_upper(std::string s) {
    for (auto& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

// All commands known to the server.
enum class Cmd { Ping, Command, Get, Set, Unknown };

Cmd parse_verb(std::string_view verb) {
    static const std::unordered_map<std::string_view, Cmd> kVerbs = {
        {"PING",    Cmd::Ping},
        {"COMMAND", Cmd::Command},
        {"GET",     Cmd::Get},
        {"SET",     Cmd::Set},
    };
    const auto it = kVerbs.find(verb);
    return (it == kVerbs.end()) ? Cmd::Unknown : it->second;
}

} 

template<typename Key, typename Value>
net::lru_server<Key, Value>::lru_server(std::uint16_t port, std::string listeningAddr)
    : net::server(port, listeningAddr),
      _io_context(1),
      _acceptor(_io_context, make_endpoint(listeningAddr, port)),
      _cache(std::make_unique<lru_cache_service<Key, Value>>(50)) {}

template<typename Key, typename Value>
void net::lru_server<Key, Value>::listen() {
    co_spawn(_io_context, accept_connections(), detached);
    _io_context.run();
}

template<typename Key, typename Value>
awaitable<void> net::lru_server<Key, Value>::accept_connections() {
    auto executor = co_await boost::asio::this_coro::executor;
    for (;;) {
        boost::system::error_code ec;
        tcp::socket socket = co_await _acceptor.async_accept(
            boost::asio::redirect_error(use_awaitable, ec));
        if (ec) {
            std::cerr << "accept error: " << ec.message() << '\n';
            continue;
        }
        co_spawn(executor, handle_connection(std::move(socket)), detached);
    }
}

template<typename Key, typename Value>
awaitable<void> net::lru_server<Key, Value>::handle_connection(tcp::socket socket) {
    boost::asio::streambuf buf;
    try {
        for (;;) {
            auto cmd_opt = co_await read_command(socket, buf);
            if (!cmd_opt) {
                break;
            }
            auto& args = *cmd_opt;
            if (args.empty()) {
                continue;
            }

            switch (parse_verb(to_upper(args[0]))) {
                case Cmd::Ping:    co_await cmd_ping(socket, args);       break;
                case Cmd::Command: co_await cmd_command(socket);          break;
                case Cmd::Get:     co_await cmd_get(socket, args);        break;
                case Cmd::Set:     co_await cmd_set(socket, args);        break;
                case Cmd::Unknown: co_await cmd_unknown(socket, args[0]); break;
            }
        }
    } catch (const boost::system::system_error&) {
        // Peer reset: drop the connection silently
    } catch (const std::exception& e) {
        boost::system::error_code ec;
        co_await boost::asio::async_write(
            socket,
            boost::asio::buffer(std::string("-ERR ") + e.what() + "\r\n"),
            boost::asio::redirect_error(use_awaitable, ec));
    }

    boost::system::error_code ignore;
    socket.shutdown(tcp::socket::shutdown_both, ignore);
    socket.close(ignore);
}

template<typename Key, typename Value>
awaitable<void> net::lru_server<Key, Value>::cmd_ping(
    tcp::socket& sock, const std::vector<std::string>& args) {
    if (args.size() >= 2) {
        co_await write_bulk(sock, args[1]);
    } else {
        co_await write_simple(sock, "PONG");
    }
}

template<typename Key, typename Value>
awaitable<void> net::lru_server<Key, Value>::cmd_command(tcp::socket& sock) {
    co_await write_empty_array(sock);
}

template<typename Key, typename Value>
awaitable<void> net::lru_server<Key, Value>::cmd_get(
    tcp::socket& sock, const std::vector<std::string>& args) {
    if (args.size() != 2) {
        co_await write_error(sock, "wrong number of arguments for 'get'");
        co_return;
    }
    auto val = _cache->getValue(args[1]);
    if (val.has_value()) {
        co_await write_bulk(sock, *val);
    } else {
        co_await write_nil(sock);
    }
}

template<typename Key, typename Value>
awaitable<void> net::lru_server<Key, Value>::cmd_set(
    tcp::socket& sock, std::vector<std::string>& args) {
    if (args.size() != 3) {
        co_await write_error(sock, "wrong number of arguments for 'set'");
        co_return;
    }
    Key k = std::move(args[1]);
    Value v = std::move(args[2]);
    _cache->putValue(std::move(k), std::move(v));
    co_await write_simple(sock, "OK");
}

template<typename Key, typename Value>
awaitable<void> net::lru_server<Key, Value>::cmd_unknown(
    tcp::socket& sock, std::string_view verb) {
    std::string msg = "unknown command '";
    msg.append(verb);
    msg.push_back('\'');
    co_await write_error(sock, msg);
}

template class net::lru_server<std::string, std::string>;
