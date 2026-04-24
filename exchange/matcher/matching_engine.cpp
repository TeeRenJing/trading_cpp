#include "matching_engine.h"

namespace Exchange
{
    MatchingEngine::MatchingEngine(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses,
                                   MEMarketUpdateLFQueue *market_updates)
        : incoming_requests_(client_requests), outgoing_ogw_responses_(client_responses), outgoing_md_updates_(market_updates),
          logger_("exchange_matching_engine.log")
    {
        // Each ticker gets its own order book. That keeps the matching rules local:
        // orders in ticker 0 never interact with orders in ticker 1.
        for (size_t i = 0; i < ticker_order_book_.size(); ++i)
        {
            ticker_order_book_[i] = new MEOrderBook(i, &logger_, this);
        }
    }

    MatchingEngine::~MatchingEngine()
    {
        // Tell the worker loop to stop before tearing down shared state.
        run_ = false;

        using namespace std::literals::chrono_literals;
        // This small pause is a crude handoff to give the background thread time
        // to observe run_ == false before we null out pointers below.
        std::this_thread::sleep_for(1s);

        incoming_requests_ = nullptr;
        outgoing_ogw_responses_ = nullptr;
        outgoing_md_updates_ = nullptr;

        for (auto &order_book : ticker_order_book_)
        {
            delete order_book;
            order_book = nullptr;
        }
    }

    auto MatchingEngine::start() -> void
    {
        run_ = true;
        // The matching engine runs on its own thread so it can poll the inbound
        // request queue continuously without blocking the rest of the process.
        ASSERT(Common::createAndStartThread(-1, "Exchange/MatchingEngine", [this]()
                                            { run(); }) != nullptr,
               "Failed to start MatchingEngine thread.");
    }

    auto MatchingEngine::stop() -> void
    {
        run_ = false;
    }
}
