#include "market_data_publisher.h"

namespace Exchange
{
    MarketDataPublisher::MarketDataPublisher(MEMarketUpdateLFQueue *market_updates, const std::string &iface,
                                             const std::string &snapshot_ip, int snapshot_port,
                                             const std::string &incremental_ip, int incremental_port)
        : outgoing_md_updates_(market_updates), snapshot_md_updates_(ME_MAX_MARKET_UPDATES),
          run_(false), logger_("exchange_market_data_publisher.log"), incremental_socket_(logger_)
    {
        ASSERT(incremental_socket_.init(incremental_ip, iface, incremental_port, /*is_listening*/ false) >= 0,
               "Unable to create incremental mcast socket. error:" + std::string(std::strerror(errno)));
        snapshot_synthesizer_ = new SnapshotSynthesizer(&snapshot_md_updates_, iface, snapshot_ip, snapshot_port);
    }

    auto MarketDataPublisher::run() noexcept -> void
    {
        logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
        while (run_)
        {
            // Each matching-engine update is sent twice:
            // 1) immediately on the incremental multicast stream
            // 2) into a local queue so the snapshot thread can rebuild full state
            for (auto market_update = outgoing_md_updates_->front();
                 outgoing_md_updates_->size() && market_update; market_update = outgoing_md_updates_->front())
            {
                logger_.log("%:% %() % Sending seq:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), next_inc_seq_num_,
                            market_update->toString().c_str());

                incremental_socket_.send(&next_inc_seq_num_, sizeof(next_inc_seq_num_));
                incremental_socket_.send(market_update, sizeof(MEMarketUpdate));

                // After the wire copy is staged we can release the source queue slot.
                outgoing_md_updates_->pop();

                // The snapshot thread consumes a richer message that includes the
                // public incremental sequence number alongside the book delta.
                ASSERT(snapshot_md_updates_.tryPush(MDPMarketUpdate{next_inc_seq_num_, *market_update}),
                       "Snapshot market data queue is full.");

                ++next_inc_seq_num_;
            }

            incremental_socket_.sendAndRecv();
        }
    }
}
