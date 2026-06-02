#pragma once
#include <HermesConfig.h>
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

  // === Timing ===
  int POLL_INTERVAL_MS = 500;
  int LTT_RANGE = 0;
  int MIN_SIGNAL_COOLDOWN_MIN = 1;
  int FAR_WAIT_MS  = 60000;   // TODO: Remove if not needed (FutFut)
  int NEAR_WAIT_MS = 5000;    // TODO: Remove if not needed (FutFut)
  int FUT_WAIT_MS = 500;      // TODO: Remove if not needed (CR)
  int OPT_WAIT_MS = 50;       // TODO: Remove if not needed (CR)

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
};

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
