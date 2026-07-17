#pragma once
#include <map>
#include <string>

// Persists per-symbol order usage to a daily file so restarts
// within the same trading day inherit the correct remaining counts.
struct OrderState {
    static std::string FilePath();                            // logs/order_state_YYYYMMDD.json
    static std::map<std::string, int> Load();                 // returns {symbol → used_count}
    static void Save(const std::map<std::string, int>& used); // atomic write
};
