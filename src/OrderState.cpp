#include "OrderState.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

std::string OrderState::FilePath() {
    auto t = std::time(nullptr);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << "logs/order_state_" << std::put_time(&tm, "%Y%m%d") << ".json";
    return oss.str();
}

std::map<std::string, int> OrderState::Load() {
    std::map<std::string, int> used;
    std::string path = FilePath();
    if (std::filesystem::exists(path)) {
        std::ifstream ifs(path);
        if (ifs.is_open()) {
            json j;
            try {
                ifs >> j;
                for (auto& el : j.items()) {
                    used[el.key()] = el.value().get<int>();
                }
            } catch (...) {
                // Ignore parse errors, just return empty/partial
            }
        }
    }
    return used;
}

void OrderState::Save(const std::map<std::string, int>& used) {
    std::string path = FilePath();
    // Ensure logs directory exists
    std::filesystem::create_directories("logs");
    std::ofstream ofs(path);
    if (ofs.is_open()) {
        json j = used;
        ofs << j.dump(4);
    }
}
