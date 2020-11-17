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

#pragma once

//! Deps
#include <boost/lockfree/queue.hpp>

//! QT
#include <QObject>

//! Project Headers
#include "atomicdex/constants/qt.actions.hpp"
#include "atomicdex/constants/qt.trading.enums.hpp"
#include "atomicdex/events/events.hpp"
#include "atomicdex/events/qt.events.hpp"
#include "atomicdex/models/qt.candlestick.charts.model.hpp"
#include "atomicdex/models/qt.portfolio.model.hpp"
#include "atomicdex/widgets/dex/qt.market.pairs.hpp"
#include "atomicdex/widgets/dex/qt.orderbook.hpp"

namespace atomic_dex
{
    class trading_page final : public QObject, public ag::ecs::pre_update_system<trading_page>
    {
      private:
        //! Q_Object definition
        Q_OBJECT

        //! Q Properties definitions
        Q_PROPERTY(qt_orderbook_wrapper* orderbook READ get_orderbook_wrapper NOTIFY orderbookChanged)
        Q_PROPERTY(market_pairs* market_pairs_mdl READ get_market_pairs_mdl NOTIFY marketPairsChanged)
        Q_PROPERTY(QVariant buy_sell_last_rpc_data READ get_buy_sell_last_rpc_data WRITE set_buy_sell_last_rpc_data NOTIFY buySellLastRpcDataChanged)
        Q_PROPERTY(bool buy_sell_rpc_busy READ is_buy_sell_rpc_busy WRITE set_buy_sell_rpc_busy NOTIFY buySellRpcStatusChanged)
        Q_PROPERTY(bool fetching_multi_ticker_fees_busy READ is_fetching_multi_ticker_fees_busy WRITE set_fetching_multi_ticker_fees_busy NOTIFY
                       multiTickerFeesStatusChanged)

        //! Trading logic
        Q_PROPERTY(MarketMode market_mode READ get_market_mode WRITE set_market_mode NOTIFY marketModeChanged)
        Q_PROPERTY(QString price READ get_price WRITE set_price NOTIFY priceChanged)
        Q_PROPERTY(QString volume READ get_volume WRITE set_volume NOTIFY volumeChanged)
        Q_PROPERTY(QString max_volume READ get_max_volume WRITE set_max_volume NOTIFY maxVolumeChanged)


        //! Private enum
        enum models
        {
            orderbook       = 0,
            market_selector = 1,
            models_size     = 2
        };

        enum models_actions
        {
            orderbook_need_a_reset = 0,
            models_actions_size    = 1
        };

        enum class trading_actions
        {
            post_process_orderbook_finished = 0,
        };

        //! Private typedefs
        using t_models               = std::array<QObject*, models_size>;
        using t_models_actions       = std::array<std::atomic_bool, models_actions_size>;
        using t_actions_queue        = boost::lockfree::queue<trading_actions>;
        using t_qt_synchronized_json = boost::synchronized_value<QJsonObject>;

        //! Private members fields
        ag::ecs::system_manager& m_system_manager;
        std::atomic_bool&        m_about_to_exit_the_app;
        t_models                 m_models;
        t_models_actions         m_models_actions;
        t_actions_queue          m_actions_queue{g_max_actions_size};
        std::atomic_bool         m_rpc_buy_sell_busy{false};
        std::atomic_bool         m_fetching_multi_ticker_fees_busy{false};
        t_qt_synchronized_json   m_rpc_buy_sell_result;

        //! Trading Logic
        MarketMode m_market_mode{MarketModeGadget::Sell};
        QString    m_price{""};
        QString    m_volume{""};
        QString    m_max_volume{"0"};

        //! Private function
        void common_cancel_all_orders(bool by_coin = false, const QString& ticker = "");
        void clear_forms() noexcept;
        void determine_max_volume() noexcept;
        void cap_volume() noexcept;

      public:
        //! Constructor
        explicit trading_page(
            entt::registry& registry, ag::ecs::system_manager& system_manager, std::atomic_bool& exit_status, portfolio_model* portfolio,
            QObject* parent = nullptr);
        ~trading_page() noexcept final = default;

        //! Public override
        void update() noexcept final;

        //! Public API
        void process_action();
        void connect_signals();
        void disconnect_signals();
        void clear_models();
        void disable_coins(const QStringList& coins) noexcept;

        //! Public QML API
        Q_INVOKABLE void on_gui_enter_dex();
        Q_INVOKABLE void on_gui_leave_dex();
        Q_INVOKABLE void cancel_order(const QStringList& orders_id);
        Q_INVOKABLE void cancel_all_orders();
        Q_INVOKABLE void cancel_all_orders_by_ticker(const QString& ticker);


        Q_INVOKABLE QVariant get_raw_mm2_coin_cfg(const QString& ticker) const noexcept;

        //! Trading business
        Q_INVOKABLE void swap_market_pair();                                             ///< market_selector (button to switch market selector and orderbook)
        Q_INVOKABLE void set_current_orderbook(const QString& base, const QString& rel); ///< market_selector (called and selecting another coin)

        Q_INVOKABLE void place_buy_order(
            const QString& base, const QString& rel, const QString& price, const QString& volume, bool is_created_order, const QString& price_denom,
            const QString& price_numer, const QString& base_nota = "", const QString& base_confs = "");
        Q_INVOKABLE void place_sell_order(
            const QString& base, const QString& rel, const QString& price, const QString& volume, bool is_created_order, const QString& price_denom,
            const QString& price_numer, const QString& rel_nota = "", const QString& rel_confs = "");

        Q_INVOKABLE void fetch_additional_fees(const QString& ticker) noexcept; ///< multi ticker (when enabling a coin of the list)
        Q_INVOKABLE void place_multiple_sell_order() noexcept;                  ///< multi ticker (when confirming a multi order)

        //! Properties
        [[nodiscard]] qt_orderbook_wrapper* get_orderbook_wrapper() const noexcept;
        [[nodiscard]] market_pairs*         get_market_pairs_mdl() const noexcept;
        [[nodiscard]] bool                  is_buy_sell_rpc_busy() const noexcept;
        void                                set_buy_sell_rpc_busy(bool status) noexcept;

        //! Trading Logic
        [[nodiscard]] MarketMode get_market_mode() const noexcept;
        void                     set_market_mode(MarketMode market_mode) noexcept;
        [[nodiscard]] QString    get_price() const noexcept;
        void                     set_price(QString price) noexcept;
        [[nodiscard]] QString    get_volume() const noexcept;
        void                     set_volume(QString volume) noexcept;
        [[nodiscard]] QString    get_max_volume() const noexcept;
        void                     set_max_volume(QString max_volume) noexcept;

        //! For multi ticker part
        [[nodiscard]] bool is_fetching_multi_ticker_fees_busy() const noexcept;
        void               set_fetching_multi_ticker_fees_busy(bool status) noexcept;

        [[nodiscard]] QVariant get_buy_sell_last_rpc_data() const noexcept;
        void                   set_buy_sell_last_rpc_data(QVariant rpc_data) noexcept;

        //! Events Callbacks
        void on_process_orderbook_finished_event(const process_orderbook_finished& evt) noexcept;
        void on_refresh_ohlc_event(const refresh_ohlc_needed& evt) noexcept;
        void on_multi_ticker_enabled(const multi_ticker_enabled& evt) noexcept;

      signals:
        void orderbookChanged();
        void candlestickChartsChanged();
        void marketPairsChanged();
        void buySellLastRpcDataChanged();
        void buySellRpcStatusChanged();
        void multiTickerFeesStatusChanged();

        //! Trading logic
        void priceChanged();
        void volumeChanged();
        void marketModeChanged();
        void maxVolumeChanged();
    };
} // namespace atomic_dex

REFL_AUTO(type(atomic_dex::trading_page))
