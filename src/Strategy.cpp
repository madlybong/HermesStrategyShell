#include "Strategy.h"
#include "ConfigLoader.h"
#include "Utils/PinThread.h"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>

using namespace std::chrono;

Strategy::Strategy(std::shared_ptr<hermes::ContractManager> cm,
                   std::shared_ptr<ConfigLoader> config)
    : cm_(cm), config_(config), running_(true) {

  // TODO: Allocate the market store based on the max token ID in contract.csv
  marketStoreSize_ = 200000; 
  marketStore_ = std::make_unique<MarketSnapshot[]>(marketStoreSize_);
  activeLocks_ = std::make_unique<std::atomic<uint8_t>[]>(marketStoreSize_);
  pairCooldownNs_ = std::make_unique<std::atomic<int64_t>[]>(marketStoreSize_);

  for (size_t i = 0; i < marketStoreSize_; ++i) {
    activeLocks_[i].store(0);
    pairCooldownNs_[i].store(0);
  }

  // TODO: Load order high-water mark
  // orderSeq_ = ReadSequenceFile("hermes_refno.seq");

  InitializeInstruments();

  loggerThread_ = std::thread(&Strategy::LoggerWorkerLoop, this);
  workerThread_ = std::thread(&Strategy::WorkerLoop, this);

#ifdef HAS_HERMES_TRADER
  int poolSize = config_->GetConfig().EXECUTION_POOL_SIZE;
  for (int i = 0; i < poolSize; ++i) {
    executionPool_.emplace_back(&Strategy::ExecutionWorkerLoop, this, i);
  }
#endif
}

Strategy::~Strategy() {
  running_ = false;
  logCv_.notify_all();
#ifdef HAS_HERMES_TRADER
  poolCv_.notify_all();
#endif
  if (loggerThread_.joinable()) loggerThread_.join();
  if (workerThread_.joinable()) workerThread_.join();
  for (auto &t : executionPool_) {
    if (t.joinable()) t.join();
  }
}

void Strategy::InitializeInstruments() {
  // TODO: Subscribe to relevant tokens / build lookup tables
  // Example: g_underlyingBySymbol["NIFTY"] = 26000;
}

void Strategy::onMarketTick(const Hermes::MarketTick &tick) {
  // TODO: Implement O(1) data ingestion into marketStore_
  // Update LTP, bids, asks, timestamp
  // Call ProcessSignal(symbol) if it's a target contract
}

void Strategy::ProcessSignal(const std::string &symbol) {
  // TODO: Implement entry/exit logic
  // Read prices from marketStore_
  // Calculate Diff/Gap and Expense
  // If Threshold met: Push context to tradeQueue_
}

double Strategy::CalculateExpenses() {
  // TODO: Implement expense model (Brokerage, STT, Interest)
  return 0.0;
}

long Strategy::GenerateOrderId() {
  return ++orderSeq_;
}

void Strategy::PrintTUI() const {
  if (!debugMode_) return;
  std::cout << "\033[2J\033[H";
  std::cout << "========================================\n";
  std::cout << "Hermes Strategy Shell — Live Monitor\n";
  std::cout << "========================================\n";
  // TODO: Print live diagnostics from diagnostics_ map
}

void Strategy::PushToLog(LogEntry::Type type, const std::string& content, const std::string& tag) {
  std::lock_guard<std::mutex> lock(logMutex_);
  logQueue_.push({type, content, tag});
  logCv_.notify_one();
}

void Strategy::LoggerWorkerLoop() {
  Hermes::Utils::PinThreadToCore(1); // Optional background pinning

  // ── 1. Derive log directory from config (once, at startup) ────────────────
  const std::string logDir = "logs/" + config_->GetConfig().LOG_DIR_NAME;
  if (!std::filesystem::exists(logDir))
    std::filesystem::create_directories(logDir);

  // ── 2. Track open streams keyed by file path (held open for lifetime) ─────
  std::map<std::string, std::ofstream> streams;
  auto getStream = [&](const std::string& path) -> std::ofstream& {
    auto it = streams.find(path);
    if (it == streams.end()) {
      auto& f = streams[path];
      f.open(path, std::ios::app);
      if (!f) std::cerr << "[Logger] Failed to open: " << path << "\n";
      return f;
    }
    return it->second;
  };

  // ── 3. Batch drain loop ────────────────────────────────────────────────────
  while (true) {
    std::vector<LogEntry> batch;
    {
      std::unique_lock<std::mutex> lock(logMutex_);
      logCv_.wait(lock, [this] { return !logQueue_.empty() || !running_; });
      while (!logQueue_.empty()) {
        batch.push_back(std::move(logQueue_.front()));
        logQueue_.pop();
      }
    }
    if (batch.empty() && !running_) break;

    // ── 4. Compute date once per batch ──────────────────────────────────────
    std::time_t now_t = std::time(nullptr);
    char dateBuf[12];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", std::localtime(&now_t));
    const std::string dateStr(dateBuf);
    
    // Cache the config for this batch iteration to avoid multiple mutex acquisitions
    const auto appConfig = config_->GetConfig();

    for (const auto& entry : batch) {
      if (entry.type == LogEntry::TRACE_LOG) {
        // ── 5a. Trace log: date-stamped, persistent stream ─────────────────
        const std::string path = logDir + "/" + dateStr + "_exec_trace.log";
        auto& f = getStream(path);
        if (f) f << entry.content << "\n";

      } else if (entry.type == LogEntry::OP_LOG) {
        // ── 5b. Op log: tag-suffixed, date-stamped, CSV header guard ────────
        const std::string suffix = entry.tag.empty()
            ? "_op.log" : ("_" + entry.tag + "_op.log");
        const std::string path = logDir + "/" + dateStr + suffix;

        auto& f = getStream(path);
        if (f) {
          if (f.tellp() == 0 && !appConfig.OP_LOG_HEADER.empty())
            f << appConfig.OP_LOG_HEADER << "\n";
          f << entry.content << "\n";
        }
      }
    }

    // ── 6. Flush all open streams once per batch ───────────────────────────
    for (auto& [path, f] : streams) f.flush();
  }
}

void Strategy::WorkerLoop() {
  // Primary router thread: reads from SPSC lock-free queue and pushes to pool
#ifdef HAS_HERMES_TRADER
  TradeContext ctx;
  while (running_) {
    if (auto optCtx = tradeQueue_.pop()) {
      ctx = *optCtx;
      std::lock_guard<std::mutex> lock(poolMutex_);
      executionQueue_.push(ctx);
      poolCv_.notify_one();
    } else {
      std::this_thread::yield();
    }
  }
#endif
}

#ifdef HAS_HERMES_TRADER
void Strategy::ExecutionWorkerLoop(int id) {
  while (running_) {
    TradeContext ctx;
    {
      std::unique_lock<std::mutex> lock(poolMutex_);
      poolCv_.wait(lock, [this] { return !executionQueue_.empty() || !running_; });
      if (!running_ && executionQueue_.empty()) break;
      ctx = executionQueue_.front();
      executionQueue_.pop();
    }
    ExecuteStrategy(ctx);
  }
}

void Strategy::ExecuteStrategy(const TradeContext &ctx) {
  // TODO: Phase-based execution logic
  // Place Anchor Leg → Poll → Place Hedge Leg → DPR Fallback
}

bool Strategy::IsOrderCompleted(const std::string &orderId) {
  return false; // TODO: Implement HermesTrader Status Poll
}

bool Strategy::IsOrderRejected(const std::string &orderId) {
  return false; // TODO: Implement HermesTrader Status Poll
}
#endif
