# trading_cpp

`trading_cpp` is a small C++20 matching-engine project focused on low-latency exchange internals. The current codebase builds a single executable, `exchange_main`, which owns an in-memory matching engine, one order book per ticker, lock-free queues between components, and asynchronous logging.

This repository is not a full exchange yet. The matching engine is implemented, and `exchange_main` now also starts the order server and market-data publisher around it. Requests, responses, and market updates are represented as compact message structs and exchanged through in-process queues.

## What is implemented

- Price-time priority matching for limit orders
- `NEW` and `CANCEL` client request handling
- Immediate matches against the opposite side of the book
- Resting orders stored by price level and FIFO priority within each level
- Client response generation: `ACCEPTED`, `FILLED`, `CANCELED`, `CANCEL_REJECTED`
- Market update generation: `ADD`, `MODIFY`, `CANCEL`, `TRADE`
- Snapshot message generation: `SNAPSHOT_START`, `CLEAR`, `SNAPSHOT_END`
- Fixed-size memory pools for orders and price levels
- Single-producer/single-consumer lock-free queues
- Background logger thread and basic thread helpers
- TCP / multicast socket utilities for gateway and feed work

## Repository layout

```text
.
├── CMakeLists.txt
├── build.sh
├── common/
│   ├── lf_queue.h         # SPSC queue
│   ├── logging.h          # async logger
│   ├── mem_pool.h         # fixed-size allocator
│   ├── tcp_socket.*       # TCP socket wrapper
│   ├── tcp_server.*       # epoll-based TCP server
│   ├── mcast_socket.*     # multicast socket wrapper
│   ├── thread_utils.h     # thread creation / affinity
│   ├── time_utils.h       # nanosecond clock helpers
│   └── types.h            # shared exchange types and limits
└── exchange/
    ├── exchange_main.cpp
    ├── matcher/
    │   ├── matching_engine.*   # request loop and routing by ticker
    │   ├── me_order_book.*     # book state and matching logic
    │   └── me_order.h          # order and price-level structures
    ├── order_server/
    │   ├── client_request.h
    │   ├── client_response.h
    │   ├── fifo_sequencer.h    # batches and time-orders inbound requests
    │   └── order_server.*      # TCP-facing ingress / egress path
    └── market_data/
        ├── market_data_publisher.*  # publishes incremental multicast updates
        ├── snapshot_synthesizer.*   # maintains book image and emits snapshots
        └── market_update.h          # delta and snapshot message types
```

## Architecture

At startup, `exchange_main` allocates three lock-free queues:

- inbound client requests
- outbound client responses
- outbound market updates

It then constructs and starts:

- `Exchange::MatchingEngine`, which busy-polls the inbound request queue and routes each request to the order book for `ticker_id`
- `Exchange::OrderServer`, which reads fixed-width client requests from TCP sockets, sequence-checks them, and forwards them into the inbound queue
- `Exchange::MarketDataPublisher`, which consumes `MEMarketUpdate` messages, multicasts incremental updates, and feeds a `SnapshotSynthesizer` thread that builds periodic full-book snapshots

Each ticker owns a dedicated `MEOrderBook`. Inside the order book:

- active orders are indexed by `(client_id, client_order_id)` for fast cancel lookup
- price levels are maintained in linked lists
- orders at the same price are maintained in FIFO order
- order and price-level objects come from fixed-size memory pools

The result is a simple but realistic low-latency design: no mutexes on the hot path, bounded storage, and explicit message flow between components.

## Market data flow

The market-data path uses two closely related message structs:

- `MEMarketUpdate`: the raw matching-engine delta such as `ADD`, `MODIFY`, `CANCEL`, or `TRADE`
- `MDPMarketUpdate`: the same delta plus the public incremental sequence number used by the feed layer

The current flow is:

1. `MatchingEngine` pushes `MEMarketUpdate` values into the outbound market-data queue.
2. `MarketDataPublisher` reads that queue, multicasts the incremental update, and assigns the next feed sequence number.
3. `MarketDataPublisher` also pushes an `MDPMarketUpdate` into a second queue for the snapshot path.
4. `SnapshotSynthesizer` consumes those sequenced updates, maintains a per-ticker cached book image, and periodically emits a snapshot framed by `SNAPSHOT_START` and `SNAPSHOT_END`.

## Matching behavior

### New order flow

1. A `NEW` request is acknowledged immediately with an `ACCEPTED` client response.
2. The order is matched against the opposite side of the book while the price crosses.
3. Each execution generates:
   - one `FILLED` response for the incoming order
   - one `FILLED` response for the resting order
   - one `TRADE` market update
4. If a resting order is fully consumed, the engine emits a `CANCEL` market update for its removal.
5. If a resting order is partially filled, the engine emits a `MODIFY` market update with the remaining quantity.
6. If the incoming order still has remaining quantity, the remainder is placed on the book and published as an `ADD` market update.

### Cancel flow

1. A `CANCEL` request looks up the live order by `client_id` and `order_id`.
2. If the order is not found, the engine returns `CANCEL_REJECTED`.
3. If the order exists, it is removed from the book.
4. The engine emits a public `CANCEL` market update and a private `CANCELED` client response.

## Key limits and constants

These values are defined in [common/types.h](/home/renji/projects/trading_cpp/common/types.h):

- `ME_MAX_TICKERS = 8`
- `ME_MAX_CLIENT_UPDATES = 256 * 1024`
- `ME_MAX_MARKET_UPDATES = 256 * 1024`
- `ME_MAX_NUM_CLIENTS = 256`
- `ME_MAX_ORDER_IDS = 1024 * 1024`
- `ME_MAX_PRICE_LEVELS = 256`

Important implication: price levels are stored in a fixed array indexed by `price % ME_MAX_PRICE_LEVELS`, so this is a compact bounded implementation rather than an unbounded production-grade book structure.

## Message model

### Client request

Defined in [exchange/order_server/client_request.h](/home/renji/projects/trading_cpp/exchange/order_server/client_request.h).

Fields:

- `type_`
- `client_id_`
- `ticker_id_`
- `order_id_`
- `side_`
- `price_`
- `qty_`

Request types:

- `NEW`
- `CANCEL`

### Client response

Defined in [exchange/order_server/client_response.h](/home/renji/projects/trading_cpp/exchange/order_server/client_response.h).

Response types:

- `ACCEPTED`
- `CANCELED`
- `FILLED`
- `CANCEL_REJECTED`

### Market update

Defined in [exchange/market_data/market_update.h](/home/renji/projects/trading_cpp/exchange/market_data/market_update.h).

Incremental update types:

- `ADD`
- `MODIFY`
- `CANCEL`
- `TRADE`

Snapshot/control update types:

- `SNAPSHOT_START`
- `CLEAR`
- `SNAPSHOT_END`

Feed-layer structs:

- `MEMarketUpdate`: matching-engine book delta
- `MDPMarketUpdate`: sequenced market-data publisher message

## Build

### Requirements

- `g++` with C++20 support
- `cmake`
- either `ninja` or `make`
- pthreads support

### Build with the helper script

```bash
chmod +x build.sh
./build.sh
```

The script configures a Release build in `cmake-build-release/` and then builds `exchange_main`.

### Build manually with CMake

```bash
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j 4
```

## Run

```bash
./cmake-build-release/exchange_main
```

When started, the process:

- creates `exchange_main.log`
- creates `exchange_matching_engine.log`
- starts the matching-engine thread
- starts the market-data publisher and snapshot synthesizer threads
- starts the TCP order server
- sleeps in the main thread while the engine polls for requests

Stop with `Ctrl+C`.

## Logging

Logging is asynchronous. `Common::Logger` writes log elements into a queue and flushes them from a background thread to disk. Current log files include:

- `exchange_main.log`
- `exchange_matching_engine.log`

This is useful for debugging, but it is still synchronous at the file boundary and not tuned for production throughput analysis.

## Networking utilities

The `common/` directory already contains reusable building blocks for:

- non-blocking TCP sockets
- an epoll-based TCP server
- multicast sockets
- socket setup helpers such as `SO_REUSEADDR`, `TCP_NODELAY`, and `SO_TIMESTAMP`

Those pieces are the foundation for:

- an order entry gateway
- a market data publisher
- replay or simulation tools

## Current limitations

- No persistence or recovery
- No modify/replace request type
- No risk checks or session management
- No tests are included yet
- Shutdown handling is basic and relies on sleeps in signal/destructor paths
- Thread affinity support exists, but the current engine thread starts with core id `-1`
- Price-level storage is bounded and hash-based rather than exact and unbounded

## Entry points worth reading

- [exchange/exchange_main.cpp](/home/renji/projects/trading_cpp/exchange/exchange_main.cpp)
- [exchange/matcher/matching_engine.h](/home/renji/projects/trading_cpp/exchange/matcher/matching_engine.h)
- [exchange/matcher/me_order_book.cpp](/home/renji/projects/trading_cpp/exchange/matcher/me_order_book.cpp)
- [exchange/order_server/order_server.h](/home/renji/projects/trading_cpp/exchange/order_server/order_server.h)
- [exchange/market_data/market_data_publisher.cpp](/home/renji/projects/trading_cpp/exchange/market_data/market_data_publisher.cpp)
- [exchange/market_data/snapshot_synthesizer.cpp](/home/renji/projects/trading_cpp/exchange/market_data/snapshot_synthesizer.cpp)
- [common/types.h](/home/renji/projects/trading_cpp/common/types.h)
- [common/lf_queue.h](/home/renji/projects/trading_cpp/common/lf_queue.h)
