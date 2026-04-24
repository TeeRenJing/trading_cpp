#pragma once

#include "common/thread_utils.h"
#include "common/lf_queue.h"
#include "common/macros.h"

#include "order_server/client_request.h"
#include "order_server/client_response.h"
#include "market_data/market_update.h"

#include "me_order_book.h"

namespace Exchange
{
    class MatchingEngine final
    {
    public:
        MatchingEngine(ClientRequestLFQueue *client_requests,
                       ClientResponseLFQueue *client_responses,
                       MEMarketUpdateLFQueue *market_updates);

        ~MatchingEngine();

        auto start() -> void;

        auto stop() -> void;

        auto processClientRequest(const MEClientRequest *client_request) noexcept
        {
            // The ticker id tells us which instrument's book should handle the
            // request. In a real exchange this is the routing step into the
            // correct per-symbol matching state.
            auto order_book = ticker_order_book_[client_request->ticker_id_];
            switch (client_request->type_)
            {
            case ClientRequestType::NEW:
            {
                // "NEW" means add a fresh order, but the order book may match it
                // immediately against resting liquidity before any remainder is stored.
                order_book->add(client_request->client_id_, client_request->order_id_, client_request->ticker_id_,
                                client_request->side_, client_request->price_, client_request->qty_);
            }
            break;

            case ClientRequestType::CANCEL:
            {
                // "CANCEL" only removes a previously accepted resting order.
                order_book->cancel(client_request->client_id_, client_request->order_id_, client_request->ticker_id_);
            }
            break;

            default:
            {
                FATAL("Received invalid client-request-type:" + clientRequestTypeToString(client_request->type_));
            }
            break;
            }
        }

        auto sendClientResponse(const MEClientResponse *client_response) noexcept
        {
            // Client responses are point-to-point acknowledgements or fills sent
            // back toward the order gateway / client session.
            logger_.log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), client_response->toString());
            ASSERT(outgoing_ogw_responses_->tryPush(*client_response),
                   "MatchingEngine: outgoing client response queue is full");
        }

        auto sendMarketUpdate(const MEMarketUpdate *market_update) noexcept
        {
            // Market updates are public book events: add, modify, cancel, trade.
            // They represent the visible state changes that a market data feed
            // would publish to subscribers.
            logger_.log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), market_update->toString());
            ASSERT(outgoing_md_updates_->tryPush(*market_update),
                   "MatchingEngine: outgoing market update queue is full");
        }

        auto run() noexcept
        {
            logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
            while (run_)
            {
                // The engine busy-polls a single-producer/single-consumer queue.
                // This avoids locks, which is common in low-latency systems.
                const auto me_client_request = incoming_requests_->front();
                if (LIKELY(me_client_request))
                {
                    logger_.log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                                me_client_request->toString());
                    processClientRequest(me_client_request);
                    // Only after processing do we advance the read index, which
                    // marks this queue slot as consumed.
                    incoming_requests_->pop();
                }
            }
        }

        // Deleted default, copy & move constructors and assignment-operators.
        MatchingEngine() = delete;

        MatchingEngine(const MatchingEngine &) = delete;

        MatchingEngine(const MatchingEngine &&) = delete;

        MatchingEngine &operator=(const MatchingEngine &) = delete;

        MatchingEngine &operator=(const MatchingEngine &&) = delete;

    private:
        // One order book per ticker/symbol.
        OrderBookHashMap ticker_order_book_;

        // Inbound requests from clients and outbound event streams produced by
        // the matching engine.
        ClientRequestLFQueue *incoming_requests_ = nullptr;
        ClientResponseLFQueue *outgoing_ogw_responses_ = nullptr;
        MEMarketUpdateLFQueue *outgoing_md_updates_ = nullptr;

        volatile bool run_ = false;

        std::string time_str_;
        Logger logger_;
    };
}
