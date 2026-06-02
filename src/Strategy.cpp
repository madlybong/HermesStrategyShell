#include "Strategy.h"
#include "ConfigLoader.h"
#include "Utils/PinThread.h"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

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

  // ── Sequence file: persist order ID watermark across restarts ─────────────
  const std::string seqFile = config_->GetConfig().ORDER_SEQ_FILE;
  if (!seqFile.empty()) {
    std::ifstream sf(seqFile);
    long persisted = 0;
    if (sf >> persisted) {
      orderSeq_ = persisted;
      std::cout << "[Strategy] Resumed order sequence from " << seqFile
                << " at " << persisted << "\n";
    }
  }

  InitializeInstruments();

  loggerThread_ = std::thread(&Strategy::LoggerWorkerLoop, this);
  workerThread_ = std::thread(&Strategy::WorkerLoop, this);

#ifdef HAS_HERMES_TRADER
  // ── Startup confirmation ──────────────────────────────────────────────────
  const auto& eng = config_->GetConfig().EXECUTION_ENGINE;
  const bool dryRun = config_->GetConfig().TRADE_DRY_RUN;
  std::cout << "[Strategy] HAS_HERMES_TRADER is DEFINED."
            << " Engine=" << eng
            << " DryRun=" << (dryRun ? "true" : "false") << "\n";

  int poolSize = config_->GetConfig().EXECUTION_POOL_SIZE;
  for (int i = 0; i < poolSize; ++i) {
    executionPool_.emplace_back(&Strategy::ExecutionWorkerLoop, this, i);
  }
  std::cout << "[Strategy] ExecutionPool started: " << poolSize << " threads.\n";
#endif
}

Strategy::~Strategy() {
  // ── Persist final sequence watermark before signalling threads ───────────
  const std::string seqFile = config_->GetConfig().ORDER_SEQ_FILE;
  if (!seqFile.empty()) {
    std::ofstream sf(seqFile, std::ios::trunc);
    sf << orderSeq_.load() << "\n";
  }

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
  auto cfg = config_->GetConfig();
  std::stringstream trace;
  auto start = std::chrono::high_resolution_clock::now();
  auto TRACE = [&](const std::string &msg) {
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start).count();
    trace << "[" << us << "us] " << msg << "\n";
  };

  // ── Phase 1: Submit all legs concurrently ─────────────────────────────────
  TRACE("Phase 1: Submitting legs (engine=" + cfg.EXECUTION_ENGINE + ")");

  // TODO: Build and submit your strategy's legs here.
  // Example (replace with your actual TradeLeg construction):
  //   char id1Buf[256] = {};
  //   auto f1 = std::async(std::launch::async, [&]() {
  //       return (PlaceOrder(&leg1, id1Buf, 256) > 0) ? std::string(id1Buf) : "";
  //   });
  //   std::string id1 = f1.get();
  //   TRACE("Leg 1: " + (id1.empty() ? "FAILED (PlaceOrder returned 0)" : id1));

  // ── Phase 2: Poll for fills ───────────────────────────────────────────────
  TRACE("Phase 2: Polling fills");
  // TODO: Poll IsOrderCompleted / IsOrderRejected per leg.
  // Follow CR pattern: loop up to OPT_WAIT_MS with POLL_INTERVAL_MS steps.

  // ── Phase 3: DPR fallback for unfilled legs ───────────────────────────────
  TRACE("Phase 3: DPR fallback check");
  // TODO: ModifyOrder to DPR price for any leg still open after OPT_WAIT_MS.

  // ── Commit trace to log ───────────────────────────────────────────────────
  PushToLog(LogEntry::TRACE_LOG, "=== EXECUTION CYCLE " + trace.str());
}

bool Strategy::IsOrderCompleted(const std::string &orderId) {
  if (orderId.empty()) return true; // Guard: empty ID = PlaceOrder failed, treat as done
  char buf[8192] = {};
  int len = FetchOrderDetail(orderId.c_str(), buf, sizeof(buf));
  if (len <= 0) return false;
  // TODO: Parse broker-specific "filled/traded/complete" status from buf.
  // Example: return strstr(buf, "\"status\":\"filled\"") != nullptr;
  return false;
}

bool Strategy::IsOrderRejected(const std::string &orderId) {
  if (orderId.empty()) return true; // Guard: empty ID = treat as rejected
  char buf[8192] = {};
  int len = FetchOrderDetail(orderId.c_str(), buf, sizeof(buf));
  if (len <= 0) return false;
  // TODO: Parse broker-specific "rejected" status from buf.
  // Example: return strstr(buf, "\"status\":\"rejected\"") != nullptr;
  return false;
}
#endif
