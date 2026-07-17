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

enum class TradeAction { ENTRY, EXIT };

struct CashFutTradeContext {
  std::string symbol;
  TradeAction action;
  std::string direction; // Always "FORWARD"
  double spread;
  double netExpense;
  
  uint32_t cashToken;
  uint32_t futToken;

  TradeLeg cashLeg;
  TradeLeg futLeg;
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

struct CashFutPair {
  std::string symbol;
  std::string futExpiry;
  uint32_t    cashToken;
  uint32_t    futToken;
  std::time_t expiryEpochSec;
  int         lotSize;
  std::string cashTradingSymbol;
  std::string futTradingSymbol;
  std::shared_ptr<hermes::ContractInfo> cashContract;
  std::shared_ptr<hermes::ContractInfo> futContract;
};

struct CashFutSnapshot {
  double cashBid = 0, cashAsk = 0;
  double futBid  = 0, futAsk  = 0;

  double execSpread    = 0;
  double execSpreadPct = 0;
  double grossPnl      = 0;
  double cashCharges   = 0;
  double futCharges    = 0;
  double slippage      = 0;
  double netPnl        = 0;
  double roi           = 0;

  bool   spreadPositive = false;
  bool   roiPassed      = false;
  bool   signalValid    = false;

  uint64_t cashVolume = 0;
  uint64_t futOI = 0;

  std::string cashSymbol, futSymbol;
  mutable std::mutex mtx;
};

class ConfigLoader;

class Strategy : public Hermes::IListener {
public:
  Strategy(std::shared_ptr<hermes::ContractManager> cm,
           std::shared_ptr<ConfigLoader> config);
  ~Strategy();

  void onMarketTick(const Hermes::MarketTick &tick) override;

  void SetDebugMode(bool enabled) { debugMode_ = enabled; }
  void PrintTUI() const;

  std::map<std::string, CashFutPair>& GetPairs() { return pairs_; }
  std::map<std::string, std::shared_ptr<CashFutSnapshot>>& GetSnapshots() { return pairSnapshots_; }
  int GetOrderQueueSize() const { return 0; } // LockFree::SPSCQueue does not support size()
  int GetOrderSeq() const { return (int)orderSeq_.load(); }
  void ForceExitPosition(const std::string& symbol);

private:
  void InitializePairs();
  void ProcessPair(const std::string &symbol);
  void EvaluateEntry(const std::string &symbol, const CashFutPair &pair, const CashFutSnapshot &snap);
  void EvaluateExit(const std::string &symbol, const CashFutPair &pair, const CashFutSnapshot &snap);

  double CalcEquityCharges(double cashPrice, int lotSize);
  double CalcFuturesCharges(double futPrice, int lotSize);
  double CalcFixedCharges(); // SEBI + Exchange

  long GenerateOrderId();
  bool AcquirePairLock(int tokenIdx);
  void ReleasePairLock(int tokenIdx);

  std::shared_ptr<hermes::ContractManager> cm_;
  std::shared_ptr<ConfigLoader> config_;
  bool debugMode_ = false;

  std::time_t expiryEpoch_ = 0;

  std::unique_ptr<MarketSnapshot[]> marketStore_;
  size_t marketStoreSize_ = 0;

  std::map<std::string, CashFutPair> pairs_;
  std::map<std::string, std::shared_ptr<CashFutSnapshot>> pairSnapshots_;

  std::unique_ptr<std::atomic<uint8_t>[]> activePairs_;
  std::unique_ptr<std::atomic<int64_t>[]> pairCooldownNs_;
  std::unique_ptr<std::atomic<int64_t>[]> rejectedCooldownNs_;
  std::unique_ptr<std::atomic<int64_t>[]> pairLockTimeNs_;
  std::unique_ptr<bool[]> isFutureTick_;
  std::unique_ptr<bool[]> isCashTick_;

  std::chrono::time_point<std::chrono::steady_clock> startTime_;

  struct LogEntry {
    enum Type { TRACE_EXEC, OP_LOG };
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
  void PushMonitorLogEntry(const std::string& msg);

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
  LockFree::SPSCQueue<CashFutTradeContext> tradeQueue_;
  std::queue<CashFutTradeContext> executionQueue_;
  std::mutex poolMutex_;
  std::condition_variable poolCv_;
  std::mutex traderApiMutex_;

  void ExecutionWorkerLoop(int id);
  void ExecuteCashFutStrategy(const CashFutTradeContext &ctx);
  bool IsOrderCompleted(const std::string &orderId);
  bool IsOrderRejected(const std::string &orderId);
  double FetchFillPrice(const std::string& orderId, double fallbackPrice);
#endif
};
