#pragma once

#include <sstream>

#include "common/types.h"
#include "common/lf_queue.h"

using namespace Common;

namespace Exchange
{
#pragma pack(push, 1)
    // This request is intended to cross a queue or wire without padding bytes.
    enum class ClientRequestType : uint8_t
    {
        INVALID = 0,
        NEW = 1,
        CANCEL = 2
    };

    inline std::string clientRequestTypeToString(ClientRequestType type)
    {
        switch (type)
        {
        case ClientRequestType::NEW:
            return "NEW";
        case ClientRequestType::CANCEL:
            return "CANCEL";
        case ClientRequestType::INVALID:
            return "INVALID";
        }
        return "UNKNOWN";
    }

    struct MEClientRequest
    {
        ClientRequestType type_ = ClientRequestType::INVALID;

        // Defaults keep partially initialized requests obviously invalid.
        ClientId client_id_ = ClientId_INVALID;
        TickerId ticker_id_ = TickerId_INVALID;
        OrderId order_id_ = OrderId_INVALID;
        Side side_ = Side::INVALID;
        Price price_ = Price_INVALID;
        Qty qty_ = Qty_INVALID;

        auto toString() const
        {
            std::stringstream ss;
            ss << "MEClientRequest"
               << " ["
               << "type:" << clientRequestTypeToString(type_)
               << " client:" << clientIdToString(client_id_)
               << " ticker:" << tickerIdToString(ticker_id_)
               << " oid:" << orderIdToString(order_id_)
               << " side:" << sideToString(side_)
               << " qty:" << qtyToString(qty_)
               << " price:" << priceToString(price_)
               << "]";
            return ss.str();
        }
    };

#pragma pack(pop)

    // SPSC queue used between the client-facing ingress path and the matching engine.
    typedef LFQueue<MEClientRequest> ClientRequestLFQueue;
}
