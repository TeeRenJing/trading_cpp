#pragma once

#include <sstream>

#include "common/types.h"

using namespace Common;

namespace Exchange
{
#pragma pack(push, 1)
    // Market data flows in two shapes:
    // 1) plain matching-engine deltas (`MEMarketUpdate`)
    // 2) feed-publisher messages with an attached sequence number (`MDPMarketUpdate`)
    enum class MarketUpdateType : uint8_t
    {
        INVALID = 0,
        ADD = 1,
        MODIFY = 2,
        CANCEL = 3,
        TRADE = 4,
        SNAPSHOT_START = 5,
        CLEAR = 6,
        SNAPSHOT_END = 7
    };

    inline std::string marketUpdateTypeToString(MarketUpdateType type)
    {
        switch (type)
        {
        case MarketUpdateType::ADD:
            return "ADD";
        case MarketUpdateType::MODIFY:
            return "MODIFY";
        case MarketUpdateType::CANCEL:
            return "CANCEL";
        case MarketUpdateType::TRADE:
            return "TRADE";
        case MarketUpdateType::SNAPSHOT_START:
            return "SNAPSHOT_START";
        case MarketUpdateType::CLEAR:
            return "CLEAR";
        case MarketUpdateType::SNAPSHOT_END:
            return "SNAPSHOT_END";
        case MarketUpdateType::INVALID:
            return "INVALID";
        }
        return "UNKNOWN";
    }

    struct MEMarketUpdate
    {
        // This is the engine's "book changed" event. It is small enough to move
        // across lock-free queues and network sockets as a plain value type.
        MarketUpdateType type_ = MarketUpdateType::INVALID;

        OrderId order_id_ = OrderId_INVALID;
        TickerId ticker_id_ = TickerId_INVALID;
        Side side_ = Side::INVALID;
        Price price_ = Price_INVALID;
        Qty qty_ = Qty_INVALID;
        Priority priority_ = Priority_INVALID;

        auto toString() const
        {
            std::stringstream ss;
            ss << "MEMarketUpdate"
               << " ["
               << " type:" << marketUpdateTypeToString(type_)
               << " ticker:" << tickerIdToString(ticker_id_)
               << " oid:" << orderIdToString(order_id_)
               << " side:" << sideToString(side_)
               << " qty:" << qtyToString(qty_)
               << " price:" << priceToString(price_)
               << " priority:" << priorityToString(priority_)
               << "]";
            return ss.str();
        }
    };

    struct MDPMarketUpdate
    {
        // Snapshot consumers need the incremental sequence number so they can
        // prove the cached book state was built without gaps.
        size_t seq_num_ = 0;
        MEMarketUpdate me_market_update_;

        auto toString() const
        {
            std::stringstream ss;
            ss << "MDPMarketUpdate"
               << " ["
               << " seq:" << seq_num_
               << " " << me_market_update_.toString()
               << "]";
            return ss.str();
        }
    };

#pragma pack(pop)

    // Matching engine -> market data publisher.
    typedef Common::LFQueue<Exchange::MEMarketUpdate> MEMarketUpdateLFQueue;
    // Market data publisher -> snapshot synthesizer.
    typedef Common::LFQueue<Exchange::MDPMarketUpdate> MDPMarketUpdateLFQueue;
}
