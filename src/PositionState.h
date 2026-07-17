#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

struct CashFutPositionLeg {
    int    leg_id = 0;              // always 0 (no averaging)
    double entry_spread = 0.0;      // fut_bid - cash_ask
    double entry_cash_ask = 0.0;    // actual fill price (EQ)
    double entry_fut_bid = 0.0;     // actual fill price (FO)
    double signal_cash_ask = 0.0;   // BBO price that triggered signal (audit)
    double signal_fut_bid = 0.0;    // BBO price that triggered signal (audit)
    int    lot_size = 0;
    double margin_used = 0.0;       // MARGIN_PER_TRADE at entry time
    double entry_charges = 0.0;     // cash_charges + fut_charges
    std::string entry_time;         // ISO8601 timestamp or similar formatted time

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CashFutPositionLeg, leg_id, entry_spread, entry_cash_ask, entry_fut_bid, signal_cash_ask, signal_fut_bid, lot_size, margin_used, entry_charges, entry_time)
};

struct ActivePosition {
    std::string symbol;
    std::string direction = "FORWARD"; // Always FORWARD for CashFut
    std::vector<CashFutPositionLeg> legs;
};

// nlohmann custom serialization for ActivePosition
inline void to_json(nlohmann::json& j, const ActivePosition& p) {
    j = nlohmann::json{
        {"symbol", p.symbol},
        {"direction", p.direction},
        {"legs", p.legs}
    };
}

inline void from_json(const nlohmann::json& j, ActivePosition& p) {
    j.at("symbol").get_to(p.symbol);
    j.at("direction").get_to(p.direction);
    j.at("legs").get_to(p.legs);
}

struct CashFutTradeRecord {
    std::string symbol;
    std::string direction = "FORWARD";
    int         leg_id = 0;
    std::string entry_time;
    std::string exit_time;
    double      entry_spread = 0.0;
    double      exit_spread = 0.0;
    double      entry_cash_ask = 0.0;
    double      entry_fut_bid = 0.0;
    double      exit_cash_bid = 0.0;
    double      exit_fut_ask = 0.0;
    int         lot_size = 0;
    double      margin_used = 0.0;
    double      entry_charges = 0.0;
    double      exit_charges = 0.0;
    double      total_charges = 0.0;
    double      cash_pnl = 0.0;
    double      futures_pnl = 0.0;
    double      gross_pnl = 0.0;
    double      net_pnl = 0.0;
    std::string exit_reason;        // "PROFIT_TARGET", "STOP_LOSS", "TIME_EXIT"
    std::string execution_note;
    double      signal_entry_cash = 0.0;
    double      signal_entry_fut = 0.0;
    double      signal_exit_cash = 0.0;
    double      signal_exit_fut = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CashFutTradeRecord, symbol, direction, leg_id, entry_time, exit_time, entry_spread, exit_spread,
                                   entry_cash_ask, entry_fut_bid, exit_cash_bid, exit_fut_ask, lot_size, margin_used,
                                   entry_charges, exit_charges, total_charges, cash_pnl, futures_pnl, gross_pnl, net_pnl, exit_reason,
                                   execution_note, signal_entry_cash, signal_entry_fut, signal_exit_cash, signal_exit_fut)
};

class PositionState {
public:
    static PositionState& Instance();
    void Load();
    void Save(std::unique_lock<std::mutex>& lk);

    std::unique_lock<std::mutex> Lock();
    std::map<std::string, ActivePosition>& Positions() { return positions_; }
    std::vector<CashFutTradeRecord>&       History()   { return history_; }
    void ClearHistory();

private:
    PositionState() = default;
    std::map<std::string, ActivePosition> positions_;
    std::vector<CashFutTradeRecord>       history_;
    std::mutex                            mutex_;
};
