#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

enum class SpreadDirection { BUY, SELL };

inline std::string to_string(SpreadDirection dir) {
    return (dir == SpreadDirection::BUY) ? "BUY" : "SELL";
}

inline SpreadDirection direction_from_string(const std::string& s) {
    return (s == "BUY") ? SpreadDirection::BUY : SpreadDirection::SELL;
}

struct PositionLeg {
    int    leg_id = 0;              // 0 = original entry, 1..N = averaging entries
    double entry_spread = 0.0;      // execution spread at leg entry (far_px - near_px)
    double entry_near_price = 0.0;  // actual fill price (near leg)
    double entry_far_price = 0.0;   // actual fill price (far leg)
    double signal_entry_near = 0.0; // BBO price that triggered signal (audit)
    double signal_entry_far = 0.0;  // BBO price that triggered signal (audit)
    int    lot_size = 0;
    double margin_used = 0.0;       // MARGIN_PER_TRADE at entry time
    double entry_expenses = 0.0;      // full-lot Rs brokerage for entry side: (near+far)*lot*(FUTURE_CHARGES/1e7)
    std::string entry_time;     // ISO8601 timestamp or similar formatted time

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PositionLeg, leg_id, entry_spread, entry_near_price, entry_far_price, signal_entry_near, signal_entry_far, lot_size, margin_used, entry_expenses, entry_time)
};

struct ActivePosition {
    std::string     symbol;
    SpreadDirection direction = SpreadDirection::BUY;
    double          initial_spread = 0.0;  // spread at L0 entry — averaging trigger anchor
    int             avg_count = 0;   // how many averaging entries have been added
    std::vector<PositionLeg> legs;
};

// nlohmann custom serialization for ActivePosition to handle SpreadDirection enum
inline void to_json(nlohmann::json& j, const ActivePosition& p) {
    j = nlohmann::json{
        {"symbol", p.symbol},
        {"direction", to_string(p.direction)},
        {"initial_spread", p.initial_spread},
        {"avg_count", p.avg_count},
        {"legs", p.legs}
    };
}

inline void from_json(const nlohmann::json& j, ActivePosition& p) {
    j.at("symbol").get_to(p.symbol);
    std::string dirStr;
    j.at("direction").get_to(dirStr);
    p.direction = direction_from_string(dirStr);
    j.at("initial_spread").get_to(p.initial_spread);
    j.at("avg_count").get_to(p.avg_count);
    j.at("legs").get_to(p.legs);
}

struct TradeRecord {
    std::string symbol;
    std::string direction;          // "BUY" or "SELL"
    int         leg_id = 0;
    std::string entry_time;
    std::string exit_time;
    double      entry_spread = 0.0;
    double      exit_spread = 0.0;
    double      entry_near_price = 0.0;
    double      entry_far_price = 0.0;
    double      exit_near_price = 0.0;
    double      exit_far_price = 0.0;
    int         lot_size = 0;
    double      margin_used = 0.0;
    double      entry_expenses = 0.0;
    double      exit_expenses = 0.0;
    double      total_expenses = 0.0;
    double      gross_pnl = 0.0;
    double      net_pnl = 0.0;
    std::string exit_reason;        // "PROFIT_TARGET", "STOP_LOSS", "TIME_EXIT"
    std::string execution_note;
    double      signal_entry_near = 0.0;
    double      signal_entry_far = 0.0;
    double      signal_exit_near = 0.0;
    double      signal_exit_far = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TradeRecord, symbol, direction, leg_id, entry_time, exit_time, entry_spread, exit_spread,
                                   entry_near_price, entry_far_price, exit_near_price, exit_far_price, lot_size, margin_used,
                                   entry_expenses, exit_expenses, total_expenses, gross_pnl, net_pnl, exit_reason,
                                   execution_note, signal_entry_near, signal_entry_far, signal_exit_near, signal_exit_far)
};

class PositionState {
public:
    static PositionState& Instance();
    void Load();   // Called once at startup
    void Save(std::unique_lock<std::mutex>& lk);   // Called after every mutation (releases lock before disk write)

    // Accessors — caller must manage thread-safety with lock
    std::unique_lock<std::mutex> Lock();
    std::map<std::string, ActivePosition>& Positions() { return positions_; }
    std::vector<TradeRecord>&              History()   { return history_; }
    void ClearHistory();

private:
    PositionState() = default;
    std::map<std::string, ActivePosition> positions_;
    std::vector<TradeRecord>              history_;
    std::mutex                            mutex_;
};
