#include "PositionState.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

PositionState& PositionState::Instance() {
    static PositionState instance;
    return instance;
}

std::unique_lock<std::mutex> PositionState::Lock() {
    return std::unique_lock<std::mutex>(mutex_);
}

void PositionState::Load() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Make sure logs directory exists
    if (!fs::exists("logs")) {
        fs::create_directories("logs");
    }

    // Load active positions
    std::string posPath = "logs/active_positions.json";
    if (fs::exists(posPath)) {
        try {
            std::ifstream f(posPath);
            if (f.is_open()) {
                nlohmann::json j;
                f >> j;
                positions_ = j.get<std::map<std::string, ActivePosition>>();
                std::cout << "[PositionState] Loaded " << positions_.size() << " active positions.\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[PositionState] Failed to load active positions: " << e.what() << "\n";
        }
    }

    // Load trade history
    std::string histPath = "logs/trade_history.json";
    if (fs::exists(histPath)) {
        try {
            std::ifstream f(histPath);
            if (f.is_open()) {
                nlohmann::json j;
                f >> j;
                history_ = j.get<std::vector<TradeRecord>>();
                std::cout << "[PositionState] Loaded " << history_.size() << " trade history records.\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[PositionState] Failed to load trade history: " << e.what() << "\n";
        }
    }
}

void PositionState::Save(std::unique_lock<std::mutex>& lk) {
    // 1. Copy the data under lock
    nlohmann::json posJson = positions_;
    nlohmann::json histJson = history_;

    // 2. Unlock the mutex so file I/O does not hold it
    if (lk.owns_lock()) {
        lk.unlock();
    }

    // 3. Serialize file writes using a static I/O mutex to prevent 
    // multiple threads from writing to the same .tmp files simultaneously
    static std::mutex io_mutex;
    std::lock_guard<std::mutex> io_lock(io_mutex);

    std::error_code ec;
    if (!fs::exists("logs")) {
        fs::create_directories("logs", ec);
    }

    // Atomic write for active positions
    std::string posPath = "logs/active_positions.json";
    std::string posTmp  = posPath + ".tmp";
    try {
        std::ofstream f(posTmp);
        if (f.is_open()) {
            f << posJson.dump(2);
            f.close();
            fs::remove(posPath, ec);           // ignore error (file may not exist yet)
            fs::rename(posTmp, posPath, ec);   // now rename safely
            if (ec) {
                std::cerr << "[PositionState] Failed to save active positions: " << ec.message() << "\n";
                fs::remove(posTmp, ec);        // clean up orphaned tmp
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[PositionState] Failed to save active positions: " << e.what() << "\n";
    }

    // Atomic write for trade history
    std::string histPath = "logs/trade_history.json";
    std::string histTmp  = histPath + ".tmp";
    try {
        std::ofstream f(histTmp);
        if (f.is_open()) {
            f << histJson.dump(2);
            f.close();
            fs::remove(histPath, ec);
            fs::rename(histTmp, histPath, ec);
            if (ec) {
                std::cerr << "[PositionState] Failed to save trade history: " << ec.message() << "\n";
                fs::remove(histTmp, ec);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[PositionState] Failed to save trade history: " << e.what() << "\n";
    }
}

void PositionState::ClearHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
    std::string histPath = "logs/trade_history.json";
    try {
        if (fs::exists(histPath)) {
            fs::remove(histPath);
        }
    } catch (...) {}
}
