#pragma once
#include <HermesConfig.h>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>

struct AppConfig {
  // === Assets ===
  std::string ASSET_CONTRACT_PATH = "contract.csv";
  std::string ASSET_FNO_FILE = "FnoStocks.csv";

  // === Portal ===
  std::string PORTAL_INTERFACE_IP = "0.0.0.0";
  std::string PORTAL_ALLOW_SCRIPTS = "";
  std::string PORTAL_FO_EXPIRIES = "";
  int PORTAL_RCVBUF_SIZE = 0;
  int MARKET_DATA_LEVEL = 0;

  // === Execution ===
  bool TRADE_ENABLED = false;
  bool TRADE_DRY_RUN = true;
  std::string EXECUTION_MODE = "PARALLEL";
  std::string EXECUTION_ENGINE = "greeksoft";
  int EXECUTION_POOL_SIZE = 8;
  std::string ORDER_SEQ_FILE = "hermes_refno.seq"; // "" = no persistence

  // === Timing ===
  int POLL_INTERVAL_MS = 500;
  int LTT_RANGE = 0;
  int MIN_SIGNAL_COOLDOWN_MIN = 1;
  int FAR_WAIT_MS  = 60000;
  int NEAR_WAIT_MS = 5000;
  int FUT_WAIT_MS = 500;
  int OPT_WAIT_MS = 50;
  std::string ENTRY_ORDER_TYPE = "LIMIT";

  // === Trading Window ===
  std::string TRADE_START = "";
  std::string TRADE_STOP = "";
  int STARTUP_WARMUP_SEC = 30;

  // === Charges ===
  double FIXED_STT = 0.9;
  double FUTURE_CHARGES = 1500.0;
  double OPTION_CHARGES = 13000.0;
  double INTEREST_CHARGES = 0.12;
  double INTEREST_MARGIN_BASE = 80000.0;
  double INDEX_FUT_MULTIPLIER = 2.0;
  double STOCK_FUT_MULTIPLIER = 2.0;

  // === Engines: GreekSoft ===
  std::string GREEKSOFT_HOST = "127.0.0.1";
  std::string GREEKSOFT_REST_PORT = "3303";
  std::string GREEKSOFT_AUTH_TOKEN = "";
  std::string GREEKSOFT_GCID = "";
  std::string GREEKSOFT_GSCID = "";
  std::string GREEKSOFT_DEVICE_TYPE = "0";
  std::string GREEKSOFT_SESSION_VALID_FOR = "30d";
  std::string GREEKSOFT_IRIS_WS_URL = "";
  std::string GREEKSOFT_IRIS_PORT = "";
  std::string GREEKSOFT_HEARTBEAT_INTERVAL_MS = "0";

  // === Engines: SaralMSMQ ===
  std::string SARAL_MSMQ_CLIENT = "";
  std::string SARAL_MSMQ_HOST = "127.0.0.1";
  std::string SARAL_MSMQ_EXCHANGE = "NseFO";
  std::string SARAL_MSMQ_CLIENT_TYPE = "Pro";
  std::string SARAL_MSMQ_BOOK_TYPE = "RL";
  std::string SARAL_MSMQ_PRODUCT = "Normal";

  // === Engines: TradeX ===
  std::string TRADEX_HOST = "127.0.0.1";
  std::string TRADEX_PORT = "8080";
  std::string TRADEX_CLIENT_CODE = "";
  std::string TRADEX_AUTH_TOKEN = "";

  // === Logging ===
  std::string LOG_DIR_NAME = "strategy";
  std::string OP_LOG_HEADER = "";
  int LOGGER_CORE = -1; // -1 = unpinned
};

// Returns true when the current local clock is within [TRADE_START, TRADE_STOP).
// If either field is empty the window is considered unrestricted.
inline bool IsWithinTradingWindow(const AppConfig& cfg) {
  if (cfg.TRADE_START.empty() || cfg.TRADE_STOP.empty()) return true;
  auto parseHHMM = [](const std::string& s) -> int {
    if (s.size() < 5) return -1;
    try {
      int h = std::stoi(s.substr(0, 2));
      int m = std::stoi(s.substr(3, 2));
      return h * 60 + m;
    } catch (...) { return -1; }
  };
  int startMin = parseHHMM(cfg.TRADE_START);
  int stopMin  = parseHHMM(cfg.TRADE_STOP);
  if (startMin < 0 || stopMin < 0) return true; // malformed — fail open
  std::time_t now_t = std::time(nullptr);
  std::tm* lt = std::localtime(&now_t);
  int nowMin = lt->tm_hour * 60 + lt->tm_min;
  if (startMin <= stopMin)
    return nowMin >= startMin && nowMin < stopMin;
  // Overnight wrap-around (e.g. 22:00 - 06:00)
  return nowMin >= startMin || nowMin < stopMin;
}

class ConfigLoader {
public:
  ConfigLoader(const std::string &filePath);
  void Update();
  AppConfig GetConfig() const;

private:
  void Load();
  std::string filePath_;
  std::filesystem::file_time_type lastWriteTime_;
  mutable std::mutex mutex_;
  AppConfig config_;
};
