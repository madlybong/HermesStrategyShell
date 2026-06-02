#pragma once
#include <ContractManager.h>
#include "HermesPortal.h"
#include "Utils/SPSCQueue.h"
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

#ifdef HAS_HERMES_TRADER
#include <HermesTrader.h>

// TODO: Define your strategy-specific trade context struct here
struct TradeContext {
  std::string symbol;
  uint32_t anchorToken;
  uint32_t hedgeToken;
  // TODO: Add prices, quantities, direction flags, net expense, etc.
};
#endif

// A standard superset market snapshot
struct MarketSnapshot {
  double ltp = 0;
  uint64_t ltt = 0;
  double atp = 0;
  uint64_t volume = 0;
  uint64_t openInterest = 0;
  uint64_t prevOi = 0;
  int64_t oiChange = 0;
  struct Level { double p = 0; int q = 0; };
  Level bids[5];
  Level asks[5];
  const hermes::ContractInfo *contract = nullptr;
  std::shared_ptr<hermes::InstrumentGroup> group;
  mutable std::mutex mtx;
};

// TODO: Define your strategy-specific snapshot/diagnostics structure here
struct DiagnosticSnapshot {
  std::string symbol;
  double currentSpread = 0;
  double currentSpreadPct = 0;
  double netEdge = 0;
  mutable std::mutex mtx;
};

class ConfigLoader;

class Strategy : public Hermes::IListener {
public:
  Strategy(std::shared_ptr<hermes::ContractManager> cm,
           std::shared_ptr<ConfigLoader> config);
  ~Strategy();

  // IListener implementation — the core hook for market data
  void onMarketTick(const Hermes::MarketTick &tick) override;

  // Debug TUI hook
  void SetDebugMode(bool enabled) { debugMode_ = enabled; }
  void PrintTUI() const;

private:
  void InitializeInstruments();
  void ProcessSignal(const std::string &symbol);

  // TODO: Define expense calculation method
  double CalculateExpenses(/* parameters */);

  long GenerateOrderId();

  std::shared_ptr<hermes::ContractManager> cm_;
  std::shared_ptr<ConfigLoader> config_;
  bool debugMode_ = false;

  // Market Data Store — Token indexed array for O(1) lookups
  std::unique_ptr<MarketSnapshot[]> marketStore_;
  size_t marketStoreSize_ = 0;

  // Strategy Diagnostics
  std::map<std::string, std::shared_ptr<DiagnosticSnapshot>> diagnostics_;

  // Debounce State (keyed on token index)
  std::unique_ptr<std::atomic<uint8_t>[]> activeLocks_;
  std::unique_ptr<std::atomic<int64_t>[]> pairCooldownNs_;

  // Async Logger (Non-blocking)
  struct LogEntry {
    enum Type { TRACE_LOG, OP_LOG };
    Type type;
    std::string content;
    std::string tag;
  };
  std::queue<LogEntry> logQueue_;
  std::mutex logMutex_;
  std::condition_variable logCv_;
  std::thread loggerThread_;
  void LoggerWorkerLoop();
  void PushToLog(LogEntry::Type type, const std::string& content, const std::string& tag = "");

  // Async Execution
  std::thread workerThread_;
  std::vector<std::thread> executionPool_;
  std::atomic<bool> running_;
  std::atomic<long> orderSeq_ = 0;
  long safeOrderMax_ = 0;

  std::map<std::string, int> symbolRemainingOrders_;
  std::map<std::string, int> usedOrdersToday_;
  std::mutex ordersMutex_;

  void WorkerLoop();

#ifdef HAS_HERMES_TRADER
  // Lock-free queue for signal handoff from the network thread to the execution pool
  LockFree::SPSCQueue<TradeContext> tradeQueue_;
  std::queue<TradeContext> executionQueue_;
  std::mutex poolMutex_;
  std::condition_variable poolCv_;

  void ExecutionWorkerLoop(int id);
  void ExecuteStrategy(const TradeContext &ctx);
  bool IsOrderCompleted(const std::string &orderId);
  bool IsOrderRejected(const std::string &orderId);
#endif
};
