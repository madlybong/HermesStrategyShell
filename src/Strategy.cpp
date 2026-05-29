#include "Strategy.h"
#include "ConfigLoader.h"
#include "Utils/PinThread.h"
#include <chrono>
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

void Strategy::PushToLog(LogEntry::Type type, const std::string& content) {
  std::lock_guard<std::mutex> lock(logMutex_);
  logQueue_.push({type, content});
  logCv_.notify_one();
}

void Strategy::LoggerWorkerLoop() {
  PinThreadToCore(std::this_thread::get_id(), 1); // Optional background pinning
  std::ofstream traceLog("trace.log", std::ios::app);
  std::ofstream opLog("orders.csv", std::ios::app);
  
  if (!opLog) std::cerr << "[Logger] Failed to open orders.csv\n";
  if (!traceLog) std::cerr << "[Logger] Failed to open trace.log\n";

  while (running_ || !logQueue_.empty()) {
    std::unique_lock<std::mutex> lock(logMutex_);
    logCv_.wait(lock, [this] { return !logQueue_.empty() || !running_; });
    
    while (!logQueue_.empty()) {
      LogEntry entry = logQueue_.front();
      logQueue_.pop();
      lock.unlock();

      if (entry.type == LogEntry::TRACE_LOG) {
        if (traceLog) traceLog << entry.content << std::endl;
      } else if (entry.type == LogEntry::OP_LOG) {
        if (opLog) opLog << entry.content << std::endl;
      }

      lock.lock();
    }
  }
}

void Strategy::WorkerLoop() {
  // Primary router thread: reads from SPSC lock-free queue and pushes to pool
#ifdef HAS_HERMES_TRADER
  TradeContext ctx;
  while (running_) {
    if (tradeQueue_.pop(ctx)) {
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
