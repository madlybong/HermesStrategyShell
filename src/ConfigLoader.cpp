#include "ConfigLoader.h"
#include <iostream>

ConfigLoader::ConfigLoader(const std::string &filePath) : filePath_(filePath) {
  auto &hcfg = hermes::HermesConfig::getInstance();
  hcfg.addAlias("COST_FUT_CHARGES",   "FUTURE_CHARGES");
  hcfg.addAlias("COST_INTEREST_RATE", "INTEREST_CHARGES");
  hcfg.addAlias("COST_INTEREST_BASE", "INTEREST_MARGIN_BASE");
  hcfg.addAlias("COST_FIXED_STT",     "FIXED_STT");
  hcfg.addAlias("ENGINE_TYPE",        "EXECUTION_ENGINE");
  hcfg.addAlias("ENGINE_MODE",        "EXECUTION_MODE");
  hcfg.addAlias("TRADE_DRY_RUN",      "DRY_RUN");
  hcfg.addAlias("COST_INDEX_MULTIPLIER", "INDEX_FUT_MULTIPLIER");
  hcfg.addAlias("COST_STOCK_MULTIPLIER", "STOCK_FUT_MULTIPLIER");
  hcfg.addAlias("PORTAL_DATA_LEVEL", "MARKET_DATA_LEVEL");

  Load();
  std::error_code ec;
  lastWriteTime_ = std::filesystem::last_write_time(filePath_, ec);
}

void ConfigLoader::Update() {
  std::error_code ec;
  auto currentWriteTime = std::filesystem::last_write_time(filePath_, ec);
  if (!ec && currentWriteTime != lastWriteTime_) {
    lastWriteTime_ = currentWriteTime;
    Load();
  }
}

AppConfig ConfigLoader::GetConfig() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return config_;
}

void ConfigLoader::Load() {
  auto &hcfg = hermes::HermesConfig::getInstance();
  hcfg.clear();
  if (!hcfg.load(filePath_)) {
    std::cerr << "[ConfigLoader] Failed to load .env\n";
  }
  hcfg.injectEnvironmentVariables();

  std::lock_guard<std::mutex> lock(mutex_);

  config_.ASSET_CONTRACT_PATH = hcfg.getString("ASSET_CONTRACT_PATH", config_.ASSET_CONTRACT_PATH);
  config_.ASSET_FNO_FILE = hcfg.getString("ASSET_FNO_FILE", config_.ASSET_FNO_FILE);

  config_.PORTAL_INTERFACE_IP = hcfg.getString("PORTAL_INTERFACE_IP", config_.PORTAL_INTERFACE_IP);
  config_.PORTAL_ALLOW_SCRIPTS = hcfg.getString("PORTAL_ALLOW_SCRIPTS", config_.PORTAL_ALLOW_SCRIPTS);
  config_.PORTAL_FO_EXPIRIES = hcfg.getString("PORTAL_FO_EXPIRIES", config_.PORTAL_FO_EXPIRIES);
  config_.PORTAL_RCVBUF_SIZE = hcfg.getInt("PORTAL_RCVBUF_SIZE", config_.PORTAL_RCVBUF_SIZE);
  config_.MARKET_DATA_LEVEL = hcfg.getInt("MARKET_DATA_LEVEL", config_.MARKET_DATA_LEVEL);

  config_.TRADE_ENABLED = hcfg.getBool("TRADE_ENABLED", config_.TRADE_ENABLED);
  config_.TRADE_DRY_RUN = hcfg.getBool("TRADE_DRY_RUN", config_.TRADE_DRY_RUN);
  config_.EXECUTION_MODE = hcfg.getString("EXECUTION_MODE", config_.EXECUTION_MODE);
  config_.EXECUTION_ENGINE = hcfg.getString("EXECUTION_ENGINE", config_.EXECUTION_ENGINE);
  config_.EXECUTION_POOL_SIZE = hcfg.getInt("EXECUTION_POOL_SIZE", config_.EXECUTION_POOL_SIZE);
  config_.ORDER_SEQ_FILE = hcfg.getString("ORDER_SEQ_FILE", config_.ORDER_SEQ_FILE);

  config_.POLL_INTERVAL_MS = hcfg.getInt("POLL_INTERVAL_MS", config_.POLL_INTERVAL_MS);
  config_.LTT_RANGE = hcfg.getInt("LTT_RANGE", config_.LTT_RANGE);
  config_.MIN_SIGNAL_COOLDOWN_MIN = hcfg.getInt("MIN_SIGNAL_COOLDOWN_MIN", config_.MIN_SIGNAL_COOLDOWN_MIN);
  config_.FAR_WAIT_MS = hcfg.getInt("FAR_WAIT_MS", config_.FAR_WAIT_MS);
  config_.NEAR_WAIT_MS = hcfg.getInt("NEAR_WAIT_MS", config_.NEAR_WAIT_MS);
  config_.FUT_WAIT_MS = hcfg.getInt("FUT_WAIT_MS", config_.FUT_WAIT_MS);
  config_.OPT_WAIT_MS = hcfg.getInt("OPT_WAIT_MS", config_.OPT_WAIT_MS);
  config_.ENTRY_ORDER_TYPE = hcfg.getString("ENTRY_ORDER_TYPE", config_.ENTRY_ORDER_TYPE);

  config_.TRADE_START = hcfg.getString("TRADE_START", config_.TRADE_START);
  config_.TRADE_STOP = hcfg.getString("TRADE_STOP", config_.TRADE_STOP);
  config_.STARTUP_WARMUP_SEC = hcfg.getInt("STARTUP_WARMUP_SEC", config_.STARTUP_WARMUP_SEC);

  config_.FIXED_STT = hcfg.getDouble("FIXED_STT", config_.FIXED_STT);
  config_.FUTURE_CHARGES = hcfg.getDouble("FUTURE_CHARGES", config_.FUTURE_CHARGES);
  config_.OPTION_CHARGES = hcfg.getDouble("OPTION_CHARGES", config_.OPTION_CHARGES);
  config_.INTEREST_CHARGES = hcfg.getDouble("INTEREST_CHARGES", config_.INTEREST_CHARGES);
  config_.INTEREST_MARGIN_BASE = hcfg.getDouble("INTEREST_MARGIN_BASE", config_.INTEREST_MARGIN_BASE);
  config_.INDEX_FUT_MULTIPLIER = hcfg.getDouble("INDEX_FUT_MULTIPLIER", config_.INDEX_FUT_MULTIPLIER);
  config_.STOCK_FUT_MULTIPLIER = hcfg.getDouble("STOCK_FUT_MULTIPLIER", config_.STOCK_FUT_MULTIPLIER);

  // GreekSoft
  config_.GREEKSOFT_HOST = hcfg.getString("GREEK_API_HOST", config_.GREEKSOFT_HOST);
  config_.GREEKSOFT_REST_PORT = hcfg.getString("GREEK_API_PORT", config_.GREEKSOFT_REST_PORT);
  config_.GREEKSOFT_AUTH_TOKEN = hcfg.getString("GREEK_API_TOKEN", config_.GREEKSOFT_AUTH_TOKEN);
  config_.GREEKSOFT_GCID = hcfg.getString("GREEKS_GCID", config_.GREEKSOFT_GCID);
  config_.GREEKSOFT_GSCID = hcfg.getString("GREEKS_GSCID", config_.GREEKSOFT_GSCID);
  config_.GREEKSOFT_DEVICE_TYPE = hcfg.getString("GREEKSOFT_DEVICE_TYPE", config_.GREEKSOFT_DEVICE_TYPE);
  config_.GREEKSOFT_SESSION_VALID_FOR = hcfg.getString("GREEKSOFT_SESSION_VALID_FOR", config_.GREEKSOFT_SESSION_VALID_FOR);
  config_.GREEKSOFT_IRIS_WS_URL = hcfg.getString("GREEKSOFT_IRIS_WS_URL", config_.GREEKSOFT_IRIS_WS_URL);
  config_.GREEKSOFT_IRIS_PORT = hcfg.getString("GREEKSOFT_IRIS_PORT", config_.GREEKSOFT_IRIS_PORT);
  config_.GREEKSOFT_HEARTBEAT_INTERVAL_MS = hcfg.getString("GREEKSOFT_HEARTBEAT_INTERVAL_MS", config_.GREEKSOFT_HEARTBEAT_INTERVAL_MS);

  // TradeX
  config_.TRADEX_HOST = hcfg.getString("TRADEX_HOST", config_.TRADEX_HOST);
  config_.TRADEX_PORT = hcfg.getString("TRADEX_PORT", config_.TRADEX_PORT);
  config_.TRADEX_CLIENT_CODE = hcfg.getString("TRADEX_CLIENT_CODE", config_.TRADEX_CLIENT_CODE);
  config_.TRADEX_AUTH_TOKEN = hcfg.getString("TRADEX_AUTH_TOKEN", config_.TRADEX_AUTH_TOKEN);

  // SaralMSMQ
  config_.SARAL_MSMQ_CLIENT = hcfg.getString("SARAL_MSMQ_CLIENT", config_.SARAL_MSMQ_CLIENT);
  config_.SARAL_MSMQ_HOST = hcfg.getString("SARAL_MSMQ_HOST", config_.SARAL_MSMQ_HOST);
  config_.SARAL_MSMQ_EXCHANGE = hcfg.getString("SARAL_MSMQ_EXCHANGE", config_.SARAL_MSMQ_EXCHANGE);
  config_.SARAL_MSMQ_CLIENT_TYPE = hcfg.getString("SARAL_MSMQ_CLIENT_TYPE", config_.SARAL_MSMQ_CLIENT_TYPE);
  config_.SARAL_MSMQ_BOOK_TYPE = hcfg.getString("SARAL_MSMQ_BOOK_TYPE", config_.SARAL_MSMQ_BOOK_TYPE);
  config_.SARAL_MSMQ_PRODUCT = hcfg.getString("SARAL_MSMQ_PRODUCT", config_.SARAL_MSMQ_PRODUCT);

  // Logging
  config_.LOG_DIR_NAME = hcfg.getString("LOG_DIR_NAME", config_.LOG_DIR_NAME);
  config_.OP_LOG_HEADER = hcfg.getString("OP_LOG_HEADER", config_.OP_LOG_HEADER);
  config_.LOGGER_CORE = hcfg.getInt("LOGGER_CORE", config_.LOGGER_CORE);
}
