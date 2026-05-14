# lru-cache

Small LRU cache server in C++23. Speaks a subset of [RESP2](https://redis.io/docs/latest/develop/reference/protocol-spec/) so you can poke at it with `redis-cli`.

Mostly an excuse to play with C++20 coroutines on top of Boost.Asio. The cache itself is the usual `std::list` + `std::unordered_map` combo (O(1) get/put/evict). The server runs one detached coroutine per connection on a single-threaded `io_context`.

## Commands

The bare minimum:

- `PING [msg]` — `+PONG` or echoes `msg`
- `GET <key>` — bulk or nil
- `SET <key> <value>` — `+OK`
- `COMMAND` — `*0` (stub so `redis-cli` doesn't bail at startup)

Anything else gets `-ERR unknown command 'X'`.

## Build

C++23 compiler + Boost. Boost comes via [vcpkg](https://github.com/microsoft/vcpkg) (see `vcpkg.json`).

```bash
cmake -B build
cmake --build build
```

## Run

```bash
./lru_cache_sv [port] [address]   # defaults: 6379, 0.0.0.0
```

Then in another terminal:

```bash
$ redis-cli -p 6379
127.0.0.1:6379> SET foo bar
OK
127.0.0.1:6379> GET foo
"bar"
```

## Caveats

Single-threaded, no auth, no persistence, capacity is hardcoded to 50. It's a learning project — don't put it in front of users.
