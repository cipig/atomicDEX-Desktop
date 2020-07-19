/******************************************************************************
 * Copyright © 2013-2019 The Komodo Platform Developers.                      *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * Komodo Platform software, including this file may be copied, modified,     *
 * propagated or distributed except according to the terms contained in the   *
 * LICENSE file                                                               *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

//! Project headers
#include "atomic.dex.provider.cex.prices.hpp"
#include "atomic.dex.provider.cex.prices.api.hpp"
#include "atomic.threadpool.hpp"

namespace atomic_dex
{
    cex_prices_provider::cex_prices_provider(entt::registry& registry, mm2& mm2_instance) : system(registry), m_mm2_instance(mm2_instance)
    {
        spdlog::debug("{} l{} f[{}]", __FUNCTION__, __LINE__, fs::path(__FILE__).filename().string());
        disable();
        dispatcher_.sink<mm2_started>().connect<&cex_prices_provider::on_mm2_started>(*this);
        dispatcher_.sink<orderbook_refresh>().connect<&cex_prices_provider::on_current_orderbook_ticker_pair_changed>(*this);
    }

    void
    cex_prices_provider::update() noexcept
    {
    }

    cex_prices_provider::~cex_prices_provider() noexcept
    {
        //!
        spdlog::debug("{} l{} f[{}]", __FUNCTION__, __LINE__, fs::path(__FILE__).filename().string());

        consume_pending_tasks();

        m_provider_thread_timer.interrupt();

        if (m_provider_ohlc_fetcher_thread.joinable())
        {
            m_provider_ohlc_fetcher_thread.join();
        }

        dispatcher_.sink<mm2_started>().disconnect<&cex_prices_provider::on_mm2_started>(*this);
        dispatcher_.sink<orderbook_refresh>().disconnect<&cex_prices_provider::on_current_orderbook_ticker_pair_changed>(*this);
    }

    void
    cex_prices_provider::on_current_orderbook_ticker_pair_changed(const orderbook_refresh& evt) noexcept
    {
        spdlog::debug("{} l{} f[{}]", __FUNCTION__, __LINE__, fs::path(__FILE__).filename().string());

        {
            {
                std::unique_lock<std::mutex> locker(m_ohlc_data_mutex, std::try_to_lock);
                m_current_ohlc_data = nlohmann::json::array();
            }
            this->dispatcher_.trigger<refresh_ohlc_needed>();
        }

        {
            m_current_orderbook_ticker_pair = {boost::algorithm::to_lower_copy(evt.base), boost::algorithm::to_lower_copy(evt.rel)};
            auto [base, rel]                = m_current_orderbook_ticker_pair;
            spdlog::debug("new orderbook pair for cex provider [{} / {}]", base, rel);
            m_pending_tasks.push(spawn([base = base, rel = rel, this]() { process_ohlc(base, rel); }));
        }
    }

    void
    cex_prices_provider::on_mm2_started([[maybe_unused]] const mm2_started& evt) noexcept
    {
        spdlog::debug("{} l{} f[{}]", __FUNCTION__, __LINE__, fs::path(__FILE__).filename().string());

        m_provider_ohlc_fetcher_thread = std::thread([this]() {
            //
            spdlog::info("cex prices provider thread started");

            using namespace std::chrono_literals;
            do {
                spdlog::info("fetching ohlc value");
                auto [base, rel] = m_current_orderbook_ticker_pair;
                if (not base.empty() && not rel.empty())
                {
                    process_ohlc(base, rel);
                }
                else
                {
                    spdlog::info("Nothing todo, sleeping");
                }
            } while (not m_provider_thread_timer.wait_for(1h));
        });
    }

    bool
    cex_prices_provider::process_ohlc(const std::string& base, const std::string& rel) noexcept
    {
        spdlog::debug("{} l{} f[{}]", __FUNCTION__, __LINE__, fs::path(__FILE__).filename().string());
        if (is_pair_supported(base, rel))
        {
            spdlog::info("{} / {} is supported, processing", base, rel);
            atomic_dex::ohlc_request req{base, rel};
            auto                     answer = atomic_dex::rpc_ohlc_get_data(std::move(req));
            if (answer.result.has_value())
            {
                {
                    std::unique_lock<std::mutex> locker(m_ohlc_data_mutex);
                    m_current_ohlc_data = answer.result.value().raw_result;
                }
                this->dispatcher_.trigger<refresh_ohlc_needed>();
                return true;
            }
            spdlog::error("http error: {}", answer.error.value_or("dummy"));
            return false;
        }

        spdlog::warn("{} / {}  not supported yet from the provider, skipping", base, rel);
        return false;
    }

    bool
    cex_prices_provider::is_pair_supported(const std::string& base, const std::string& rel) const noexcept
    {
        const std::string final_ticker_pair = boost::algorithm::to_lower_copy(base) + "-" + boost::algorithm::to_lower_copy(rel);
        return std::any_of(begin(m_supported_pair), end(m_supported_pair), [final_ticker_pair](auto&& cur_str) { return cur_str == final_ticker_pair; });
    }

    bool
    cex_prices_provider::is_ohlc_data_available() const noexcept
    {
        spdlog::debug("{} l{} f[{}]", __FUNCTION__, __LINE__, fs::path(__FILE__).filename().string());
        bool res = false;
        {
            std::unique_lock<std::mutex> locker(m_ohlc_data_mutex);
            res = !m_current_ohlc_data.empty();
            spdlog::debug("data available: {}", res);
        }
        return res;
    }

    nlohmann::json
    cex_prices_provider::get_ohlc_data(const std::string& range) noexcept
    {
        nlohmann::json res = nlohmann::json::array();
        {
            std::unique_lock<std::mutex> locker(m_ohlc_data_mutex);
            if (m_current_ohlc_data.contains(range))
            {
                res = m_current_ohlc_data.at(range);
            }
        }
        return res;
    }

    void
    cex_prices_provider::consume_pending_tasks()
    {
        while (not m_pending_tasks.empty())
        {
            auto& fut_tasks = m_pending_tasks.front();
            if (fut_tasks.valid())
            {
                fut_tasks.wait();
            }
            m_pending_tasks.pop();
        }
    }

} // namespace atomic_dex