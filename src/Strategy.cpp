#include "Strategy.h"
#include "ConfigLoader.h"
#include "Utils.h"
#include "Utils/PinThread.h"
#include "OrderState.h"
#include "PositionState.h"
#include "ImGuiMonitor.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::time_t parseExpiry(const std::string& s) {
    std::tm tm{};
    memset(&tm, 0, sizeof(tm));
    if (s.length() == 9) { // DDMMMYYYY
        try {
            int day = std::stoi(s.substr(0, 2));
            std::string mon = s.substr(2, 3);
            int year = std::stoi(s.substr(5, 4));
            tm.tm_mday = day;
            tm.tm_year = year - 1900;
            const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
            for (int i = 0; i < 12; ++i) {
                if (mon == months[i]) {
                    tm.tm_mon = i;
                    break;
                }
            }
        } catch (...) {}
    }
    tm.tm_hour = 15;
    tm.tm_min = 30;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    return std::mktime(&tm);
}

#ifdef HAS_HERMES_TRADER
#include "HermesTrader.h"
#endif

Strategy::Strategy(std::shared_ptr<hermes::ContractManager> cm, std::shared_ptr<ConfigLoader> config)
    : cm_(cm), config_(config), running_(true) {

  AppConfig cfg = config_->GetConfig();

#ifdef HAS_HERMES_TRADER
  std::cout << "[Strategy] HAS_HERMES_TRADER is DEFINED.\n";
  std::string execModeRaw = cfg.EXECUTION_MODE;
  std::string execEngine = cfg.EXECUTION_ENGINE;

  int status = 0;
  if (execEngine == "local") {
    status = InitializeTrader("", "", "", cfg.TRADE_DRY_RUN ? 1 : 0,
                              "", "", execModeRaw.c_str(), execEngine.c_str());
    std::cout << "[Strategy] LocalExecutor InitializeTrader status=" << status << "\n";
  } else {
    // Only LocalExecutor supported natively for CashFut Phase 1 without external integration
    status = InitializeTrader("", "", "", cfg.TRADE_DRY_RUN ? 1 : 0, "", "", execModeRaw.c_str(), execEngine.c_str());
  }

  if (status == 0) {
    if (cfg.TRADE_DRY_RUN) {
      std::cerr << "[CRITICAL] Trader initialization FAILED. Continuing in VIEW-ONLY mode.\n";
    } else {
      throw std::runtime_error("Trader initialization FAILED. Aborting because TRADE_DRY_RUN=false.");
    }
  } else {
    SetLimitPolicy(1);
    std::cout << "[Strategy] Trader Initialized Successfully.\n";
  }

  const int poolSize = std::max(4, cfg.EXECUTION_POOL_SIZE);
  for (int i = 0; i < poolSize; ++i) executionPool_.emplace_back(&Strategy::ExecutionWorkerLoop, this, i);
#endif

  int maxIdx = cm_->GetMaxIndex();
  marketStoreSize_ = maxIdx;
  marketStore_ = std::make_unique<MarketSnapshot[]>(maxIdx);
  activePairs_ = std::make_unique<std::atomic<uint8_t>[]>(maxIdx);
  pairCooldownNs_ = std::make_unique<std::atomic<int64_t>[]>(maxIdx);
  rejectedCooldownNs_ = std::make_unique<std::atomic<int64_t>[]>(maxIdx);
  pairLockTimeNs_ = std::make_unique<std::atomic<int64_t>[]>(maxIdx);
  isFutureTick_ = std::make_unique<bool[]>(maxIdx);
  isCashTick_ = std::make_unique<bool[]>(maxIdx);

  for (size_t i = 0; i < maxIdx; ++i) {
    activePairs_[i] = 0;
    pairCooldownNs_[i] = 0;
    rejectedCooldownNs_[i] = 0;
    pairLockTimeNs_[i] = 0;
    isFutureTick_[i] = false;
    isCashTick_[i] = false;
  }
  
  startTime_ = std::chrono::steady_clock::now();
  expiryEpoch_ = parseExpiry(cfg.EXPIRY);
  PositionState::Instance().Load();

  const auto &groups = cm_->GetGroups();
  for (int i = 0; i < maxIdx; ++i) {
    auto contract = cm_->GetContractByIndex(i);
    if (contract) {
      marketStore_[i].contract = contract.get();
      isFutureTick_[i] = (contract->type.find("FUT") != std::string::npos);
      isCashTick_[i]   = (contract->type.find("EQ") != std::string::npos);
      auto it = groups.find(contract->symbol);
      if (it != groups.end()) marketStore_[i].group = it->second;
    }
  }

  const auto &limits = cm_->GetSymbolLimits();
  usedOrdersToday_ = OrderState::Load();
  for (const auto &pair : limits) {
    SetSymbolLimit(pair.first.c_str(), pair.second);
    int used = 0;
    auto uit = usedOrdersToday_.find(pair.first);
    if (uit != usedOrdersToday_.end()) used = uit->second;
    int remaining = std::max(0, pair.second - used);
    symbolRemainingOrders_[pair.first] = remaining;
    if (used > 0) AdjustSymbolLimit(pair.first.c_str(), -used);
  }

  InitializePairs();

  loggerThread_ = std::thread(&Strategy::LoggerWorkerLoop, this);
  workerThread_ = std::thread(&Strategy::WorkerLoop, this);
  std::ifstream SeqFile("hermes_refno.seq");
  long loadedSeq = 1;
  if (SeqFile.is_open()) SeqFile >> loadedSeq;
  if (loadedSeq < 1 || loadedSeq > 32700) loadedSeq = 1;
  
  orderSeq_ = loadedSeq;
  safeOrderMax_ = orderSeq_ + 500;
  if (safeOrderMax_ > 32760) safeOrderMax_ = 32760;

  std::ofstream OutFile("hermes_refno.seq", std::ios::trunc);
  if (OutFile.is_open()) OutFile << safeOrderMax_;
}

Strategy::~Strategy() {
  running_ = false;
#ifdef HAS_HERMES_TRADER
  { std::lock_guard<std::mutex> lock(poolMutex_); poolCv_.notify_all(); }
  for (auto &t : executionPool_) { if (t.joinable()) t.join(); }
#endif
  { std::lock_guard<std::mutex> lock(logMutex_); logCv_.notify_all(); }
  if (loggerThread_.joinable()) loggerThread_.join();
  if (workerThread_.joinable()) workerThread_.join();
}

void Strategy::InitializePairs() {
  auto cfg = config_->GetConfig();
  std::map<std::string, std::shared_ptr<hermes::ContractInfo>> eqContracts;
  std::map<std::string, std::shared_ptr<hermes::ContractInfo>> futContracts;
  
  int maxIdx = cm_->GetMaxIndex();
  for (int i = 0; i < maxIdx; ++i) {
    auto c = cm_->GetContractByIndex(i);
    if (c) {
      if (c->type == "EQ") {
          eqContracts[c->symbol] = c;
      } else if (c->type.find("FUT") != std::string::npos && c->expiry == cfg.EXPIRY) {
          futContracts[c->symbol] = c;
      }
    }
  }

  for (const auto &[sym, eqContract] : eqContracts) {
    auto it = futContracts.find(sym);
    if (it != futContracts.end()) {
      auto futContract = it->second;
      CashFutPair pair;
      pair.symbol = sym;
      pair.cashContract = eqContract;
      pair.futContract = futContract;
      pair.cashToken = eqContract->token;
      pair.futToken = futContract->token;
      pair.lotSize = futContract->lotSize;
      pair.cashTradingSymbol = eqContract->tradingSymbol;
      pair.futTradingSymbol = futContract->tradingSymbol;
      pair.expiryEpochSec = expiryEpoch_;
      
      pairs_[sym] = pair;
      pairSnapshots_[sym] = std::make_shared<CashFutSnapshot>();
      
      int ci = cm_->GetTokenIndex(pair.cashToken);
      if (ci >= 0 && ci < (int)marketStoreSize_) marketStore_[ci].contract = pair.cashContract.get();
      
      int fi = cm_->GetTokenIndex(pair.futToken);
      if (fi >= 0 && fi < (int)marketStoreSize_) marketStore_[fi].contract = pair.futContract.get();

      std::cout << "[Strategy] Pair Found: " << sym << " (EQ: " << pair.cashToken << " vs FUT: " << pair.futToken << ")\n";
    }
  }
}

void Strategy::WorkerLoop() {
  Hermes::Utils::PinThreadToCore(2);
#ifdef HAS_HERMES_TRADER
  while (running_) {
    auto ctxOpt = tradeQueue_.pop();
    if (ctxOpt) {
      std::lock_guard<std::mutex> lock(poolMutex_);
      executionQueue_.push(*ctxOpt);
      poolCv_.notify_one();
    } else std::this_thread::yield();
  }
#endif
}

double Strategy::CalcEquityCharges(double cashPrice, int lotSize) {
    auto cfg = config_->GetConfig();
    double turnover = cashPrice * lotSize;
    double stt = (turnover * cfg.EQUITY_STT_BUY_PCT / 100.0) + (turnover * cfg.EQUITY_STT_SELL_PCT / 100.0);
    double exchange = 2 * (turnover * cfg.EQUITY_EXCHANGE_FEE / 10000000.0);
    double sebi = 2 * (turnover * cfg.EQUITY_SEBI_FEE / 10000000.0);
    double stamp = 2 * (turnover * cfg.EQUITY_STAMP_DUTY / 10000000.0);
    return stt + exchange + sebi + stamp;
}

double Strategy::CalcFuturesCharges(double futPrice, int lotSize) {
    auto cfg = config_->GetConfig();
    double turnover = futPrice * lotSize;
    return (turnover * 2) * (cfg.FUTURE_CHARGES / 10000000.0); // round trip
}

void Strategy::onMarketTick(const Hermes::MarketTick &tick) {
  int idx = cm_->GetTokenIndex(tick.token);
  if (idx < 0 || idx >= (int)marketStoreSize_) return;

  MarketSnapshot &snap = marketStore_[idx];
  {
    std::lock_guard<std::mutex> lock(snap.mtx);
    if (tick.depthLevels > 0) {
      for (int i = 0; i < std::min((int)tick.depthLevels, 5); ++i) {
        snap.bids[i].p = tick.bidPrices[i] / 100.0; snap.bids[i].q = tick.bidQtys[i];
        snap.asks[i].p = tick.askPrices[i] / 100.0; snap.asks[i].q = tick.askQtys[i];
      }
    }
    if (tick.ltp > 0) snap.ltp = tick.ltp / 100.0;
    if (tick.atp > 0) snap.atp = tick.atp / 100.0;
    if (tick.exchangeTime > 0) snap.ltt = tick.exchangeTime;
    if (tick.volume > 0) snap.volume = tick.volume;
    if (tick.openInterest > 0) snap.openInterest = tick.openInterest;
  }

  
  if (snap.contract) {
      ProcessPair(snap.contract->symbol);
  }
}

void Strategy::ProcessPair(const std::string &symbol) {
  auto it = pairs_.find(symbol);
  if (it == pairs_.end()) return;
  const CashFutPair &pair = it->second;

  int ci = cm_->GetTokenIndex(pair.cashToken);
  int fi = cm_->GetTokenIndex(pair.futToken);
  if (ci < 0 || fi < 0) return;

  auto snapIt = pairSnapshots_.find(symbol);
  if (snapIt == pairSnapshots_.end()) return;
  auto &diag = snapIt->second;

  double cashB, cashA, futB, futA;
  {
    std::lock_guard<std::mutex> lock1(marketStore_[ci].mtx);
    cashB = marketStore_[ci].bids[0].p; cashA = marketStore_[ci].asks[0].p;
  }
  {
    std::lock_guard<std::mutex> lock2(marketStore_[fi].mtx);
    futB = marketStore_[fi].bids[0].p; futA = marketStore_[fi].asks[0].p;
  }

  if (cashB <= 0 || cashA <= 0 || futB <= 0 || futA <= 0) return;

  auto cfg = config_->GetConfig();
  
  std::lock_guard<std::mutex> diagLock(diag->mtx);
  diag->cashBid = cashB; diag->cashAsk = cashA;
  diag->futBid = futB; diag->futAsk = futA;
  
  diag->execSpread = futB - cashA;
  diag->grossPnl = diag->execSpread * pair.lotSize;
  diag->cashCharges = CalcEquityCharges(cashA, pair.lotSize);
  diag->futCharges = CalcFuturesCharges(futB, pair.lotSize);
  diag->slippage = cfg.SLIPPAGE_POINTS * 4 * pair.lotSize;
  diag->netPnl = diag->grossPnl - diag->cashCharges - diag->futCharges - diag->slippage;
  diag->roi = (diag->netPnl / cfg.MARGIN_PER_TRADE) * 100.0;
  
  diag->spreadPositive = (diag->execSpread > 0);
  diag->roiPassed = (diag->roi >= cfg.MIN_ROI_PCT);
  diag->signalValid = (diag->spreadPositive && diag->roiPassed);

  auto& posState = PositionState::Instance();
  auto lk = posState.Lock();
  auto& positions = posState.Positions();
  
  if (positions.find(symbol) != positions.end()) {
      EvaluateExit(symbol, pair, *diag);
  } else {
      EvaluateEntry(symbol, pair, *diag);
  }
}

bool Strategy::AcquirePairLock(int tokenIdx) {
  uint8_t expected = 0;
  return activePairs_[tokenIdx].compare_exchange_strong(expected, 1);
}

void Strategy::ReleasePairLock(int tokenIdx) {
  activePairs_[tokenIdx].store(0);
}

void Strategy::EvaluateEntry(const std::string &symbol, const CashFutPair &pair, const CashFutSnapshot &snap) {
  if (!snap.signalValid) return;
  
  auto cfg = config_->GetConfig();
  if (!cfg.TRADE_ENABLED) return;
  if (!IsWithinTradingWindow(cfg)) return;
  
  auto uptime = std::chrono::steady_clock::now() - startTime_;
  if (std::chrono::duration_cast<std::chrono::seconds>(uptime).count() < cfg.STARTUP_WARMUP_SEC) return;
  
  int fi = cm_->GetTokenIndex(pair.futToken);
  int64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  
  if (nowNs - pairCooldownNs_[fi].load(std::memory_order_relaxed) < (int64_t)cfg.MIN_SIGNAL_COOLDOWN_MIN * 60000000000LL) return;
  if (nowNs - rejectedCooldownNs_[fi].load(std::memory_order_relaxed) < 5000000000LL) return; // 5s backoff

  int rem = 0;
  {
    std::lock_guard<std::mutex> lk(ordersMutex_);
    if (symbolRemainingOrders_.find(symbol) != symbolRemainingOrders_.end()) {
      rem = symbolRemainingOrders_[symbol];
    }
  }
  if (rem < 2) return;

  if (AcquirePairLock(fi)) {
#ifdef HAS_HERMES_TRADER
    CashFutTradeContext ctx;
    ctx.symbol = symbol;
    ctx.action = TradeAction::ENTRY;
    ctx.direction = "FORWARD";
    ctx.spread = snap.execSpread;
    ctx.netExpense = snap.cashCharges + snap.futCharges;
    ctx.cashToken = pair.cashToken;
    ctx.futToken = pair.futToken;

    // Anchor: Future Leg (SELL)
    ctx.futLeg.exchangeToken = std::to_string(pair.futToken);
    ctx.futLeg.symbol = pair.futTradingSymbol;
    ctx.futLeg.side = 2; // SELL
    ctx.futLeg.price = snap.futBid;
    ctx.futLeg.numLots = 1;
    ctx.futLeg.lotSize = pair.lotSize;
    ctx.futLeg.strategyId = GenerateOrderId();
    
    // Hedge: Cash Leg (BUY)
    ctx.cashLeg.exchangeToken = std::to_string(pair.cashToken);
    ctx.cashLeg.symbol = pair.cashTradingSymbol;
    ctx.cashLeg.side = 1; // BUY
    ctx.cashLeg.price = snap.cashAsk;
    ctx.cashLeg.numLots = 1;
    ctx.cashLeg.lotSize = pair.lotSize;
    ctx.cashLeg.strategyId = GenerateOrderId();

    {
      std::lock_guard<std::mutex> lk(ordersMutex_);
      symbolRemainingOrders_[symbol] -= 2; AdjustSymbolLimit(symbol.c_str(), -2);
      usedOrdersToday_[symbol] += 2; OrderState::Save(usedOrdersToday_);
    }

    tradeQueue_.push(ctx);
    pairLockTimeNs_[fi].store(nowNs, std::memory_order_relaxed);
#endif
  }
}

void Strategy::EvaluateExit(const std::string &symbol, const CashFutPair &pair, const CashFutSnapshot &snap) {
  auto cfg = config_->GetConfig();
  
  auto& posState = PositionState::Instance();
  auto lk = posState.Lock();
  auto& positions = posState.Positions();
  
  auto it = positions.find(symbol);
  if (it == positions.end()) return;
  ActivePosition& activePos = it->second;
  if (activePos.legs.empty()) return;
  
  CashFutPositionLeg& leg = activePos.legs.front();
  
  // Forward exit: Sell Cash at Bid, Buy Future at Ask
  double exitCashBid = snap.cashBid;
  double exitFutAsk = snap.futAsk;
  
  double cashPnl = (exitCashBid - leg.entry_cash_ask) * pair.lotSize;
  double futPnl = (leg.entry_fut_bid - exitFutAsk) * pair.lotSize;
  double grossPnl = cashPnl + futPnl;
  
  double exitCashCharges = CalcEquityCharges(exitCashBid, pair.lotSize); // rough estimate
  double exitFutCharges = CalcFuturesCharges(exitFutAsk, pair.lotSize);
  
  double netPnl = grossPnl - leg.entry_charges - exitCashCharges - exitFutCharges;
  
  bool tpHit = (netPnl > cfg.MARGIN_PER_TRADE * (cfg.EXIT_PROFIT_PCT / 100.0) && netPnl > 0);
  bool slHit = (netPnl <= -(cfg.MARGIN_PER_TRADE * (cfg.STOP_LOSS_PCT / 100.0)));
  
  std::time_t now = std::time(nullptr);
  double daysToExpiry = std::difftime(pair.expiryEpochSec, now) / (60 * 60 * 24);
  bool timeExitHit = (daysToExpiry <= cfg.TIME_EXIT_DAYS);
  
  if (tpHit || slHit || timeExitHit) {
      int fi = cm_->GetTokenIndex(pair.futToken);
      if (AcquirePairLock(fi)) {
          lk.unlock();
          
#ifdef HAS_HERMES_TRADER
          CashFutTradeContext ctx;
          ctx.symbol = symbol;
          ctx.action = TradeAction::EXIT;
          ctx.direction = "FORWARD";
          ctx.spread = 0;
          ctx.cashToken = pair.cashToken;
          ctx.futToken = pair.futToken;

          // Anchor: Future Leg (BUY to cover)
          ctx.futLeg.exchangeToken = std::to_string(pair.futToken);
          ctx.futLeg.symbol = pair.futTradingSymbol;
          ctx.futLeg.side = 1; // BUY
          ctx.futLeg.price = snap.futAsk;
          ctx.futLeg.numLots = 1;
          ctx.futLeg.lotSize = pair.lotSize;
          ctx.futLeg.strategyId = GenerateOrderId();
          ctx.futLeg.isSqOff = true;
          
          // Hedge: Cash Leg (SELL to close)
          ctx.cashLeg.exchangeToken = std::to_string(pair.cashToken);
          ctx.cashLeg.symbol = pair.cashTradingSymbol;
          ctx.cashLeg.side = 2; // SELL
          ctx.cashLeg.price = snap.cashBid;
          ctx.cashLeg.numLots = 1;
          ctx.cashLeg.lotSize = pair.lotSize;
          ctx.cashLeg.strategyId = GenerateOrderId();
          ctx.cashLeg.isSqOff = true;
          
          tradeQueue_.push(ctx);
#endif
      }
  }
}

void Strategy::ForceExitPosition(const std::string& symbol) {
  // Manual intervention logic could go here
}

#ifdef HAS_HERMES_TRADER
void Strategy::ExecutionWorkerLoop(int id) {
  while (running_) {
    CashFutTradeContext ctx;
    {
      std::unique_lock<std::mutex> lock(poolMutex_);
      poolCv_.wait(lock, [&]() { return !running_ || !executionQueue_.empty(); });
      if (!running_ && executionQueue_.empty()) break;
      ctx = std::move(executionQueue_.front()); executionQueue_.pop();
    }
    ExecuteCashFutStrategy(ctx);
  }
}

bool Strategy::IsOrderCompleted(const std::string &orderId) {
  char buf[8192];
  int len;
  {
    std::lock_guard<std::mutex> apiLock(traderApiMutex_);
    len = FetchOrderDetail(orderId.c_str(), buf, sizeof(buf));
  }
  if (len <= 0) return false;
  if (strstr(buf, "\"order_status\":\"Traded\"")  || strstr(buf, "\"status\":\"filled\"") ||
      strstr(buf, "\"order_status\":\"Complete\"") || strstr(buf, "\"Status\":\"Executed\"")) return true;
  return false;
}

bool Strategy::IsOrderRejected(const std::string &orderId) {
  char buf[8192];
  int len;
  {
    std::lock_guard<std::mutex> apiLock(traderApiMutex_);
    len = FetchOrderDetail(orderId.c_str(), buf, sizeof(buf));
  }
  if (len <= 0) return false;
  if (strstr(buf, "\"order_status\":\"Rejected\"") || strstr(buf, "\"status\":\"rejected\"")) return true;
  return false;
}

double Strategy::FetchFillPrice(const std::string& orderId, double fallbackPrice) {
    return fallbackPrice; // Simplification for paper mode
}

void Strategy::ExecuteCashFutStrategy(const CashFutTradeContext &ctx) {
    auto cfg = config_->GetConfig();
    std::stringstream trace; auto start = std::chrono::high_resolution_clock::now();
    auto TRACE = [&](const std::string &msg) {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
        trace << "[" << us << "us] " << msg << "\n";
    };
    
    TradeLeg futLeg = ctx.futLeg, cashLeg = ctx.cashLeg;
    
    auto GetDprPrice = [&](const TradeLeg &leg, bool isCash) {
        double basePrice = leg.price;
        double offsetPrice = (leg.side == 1) ? basePrice + 15.0 : basePrice - 15.0; // aggressive +15 pts
        return std::round(std::max(0.05, offsetPrice) / 0.05) * 0.05;
    };

    if (ctx.action == TradeAction::EXIT) {
        futLeg.price = GetDprPrice(futLeg, false);
        cashLeg.price = GetDprPrice(cashLeg, true);
    }
    
    auto releaseAndReturn = [&]() {
        int fi = cm_->GetTokenIndex(ctx.futToken);
        ReleasePairLock(fi);
        PushToLog(LogEntry::TRACE_EXEC, "=== EXECUTION CYCLE " + trace.str());
    };
    
    // Phase 1: Futures Leg
    TRACE("Phase 1: Placing FUT order (Anchor, LIMIT) @ " + std::to_string(futLeg.price));
    char futOrderIdBuf[256] = {}; std::string futOrderId = "";
    int pollInterval = std::max(5, cfg.POLL_INTERVAL_MS);
    
    int futRc;
    {
        std::lock_guard<std::mutex> apiLock(traderApiMutex_);
        futRc = PlaceOrder(&futLeg, futOrderIdBuf, 256);
    }
    
    if (futRc > 0) futOrderId = futOrderIdBuf;
    else {
        TRACE("FUT Order placement failed.");
        int fi = cm_->GetTokenIndex(ctx.futToken);
        int64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        rejectedCooldownNs_[fi].store(nowNs, std::memory_order_relaxed);
        releaseAndReturn(); return;
    }
    
    bool futFilled = cfg.TRADE_DRY_RUN;
    if (!futFilled) {
        int futElapsed = 0, futWaitLimit = cfg.FUT_WAIT_MS;
        while (futElapsed < futWaitLimit) {
            if (IsOrderCompleted(futOrderId)) { futFilled = true; TRACE("FUT Filled"); break; }
            if (IsOrderRejected(futOrderId)) { TRACE("FUT Rejected"); break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval)); futElapsed += pollInterval;
        }
        
        if (!futFilled) {
            if (ctx.action == TradeAction::EXIT) {
                // Keep trying to exit if it's an exit order
            } else {
                bool canceled;
                {
                    std::lock_guard<std::mutex> apiLock(traderApiMutex_);
                    canceled = CancelOrder(futOrderId.c_str());
                }
                TRACE("FUT Cancelled due to timeout.");
                releaseAndReturn(); return;
            }
        }
    }
    
    // Phase 2: Cash Leg
    TRACE("Phase 2: Placing CASH order (Hedge, LIMIT) @ " + std::to_string(cashLeg.price));
    char cashOrderIdBuf[256] = {}; std::string cashOrderId = "";
    
    int cashRc;
    {
        std::lock_guard<std::mutex> apiLock(traderApiMutex_);
        cashRc = PlaceOrder(&cashLeg, cashOrderIdBuf, 256);
    }
    
    if (cashRc > 0) cashOrderId = cashOrderIdBuf;
    
    bool cashFilled = cfg.TRADE_DRY_RUN;
    if (!cashFilled && cashRc > 0) {
        int cashElapsed = 0, cashWaitLimit = cfg.CASH_WAIT_MS;
        while (cashElapsed < cashWaitLimit) {
            if (IsOrderCompleted(cashOrderId)) { cashFilled = true; TRACE("CASH Filled"); break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval)); cashElapsed += pollInterval;
        }
        
        if (!cashFilled) {
            double freshDpr = GetDprPrice(cashLeg, true);
            TRACE("CASH Timeout. Re-pricing to DPR @ " + std::to_string(freshDpr));
            bool modified;
            {
                std::lock_guard<std::mutex> apiLock(traderApiMutex_);
                modified = ModifyOrder(cashOrderId.c_str(), freshDpr, cashLeg.numLots * cashLeg.lotSize, cashLeg.numLots, "LIMIT");
            }
            if (modified) cashFilled = true; // Assumed fill at DPR
        }
    }
    
    if (ctx.action == TradeAction::ENTRY) {
        auto& posState = PositionState::Instance();
        auto lk = posState.Lock();
        auto& positions = posState.Positions();
        
        CashFutPositionLeg leg;
        leg.entry_cash_ask = cashLeg.price;
        leg.entry_fut_bid = futLeg.price;
        leg.entry_spread = ctx.spread;
        leg.entry_charges = ctx.netExpense;
        leg.margin_used = cfg.MARGIN_PER_TRADE;
        leg.lot_size = cashLeg.lotSize;
        leg.entry_time = "NOW"; // Use real time
        
        ActivePosition pos;
        pos.symbol = ctx.symbol;
        pos.direction = "FORWARD";
        pos.legs.push_back(leg);
        
        positions[ctx.symbol] = pos;
        posState.Save(lk);
        TRACE("Saved active position.");
    } else {
        auto& posState = PositionState::Instance();
        auto lk = posState.Lock();
        auto& positions = posState.Positions();
        auto& history = posState.History();
        
        auto it = positions.find(ctx.symbol);
        if (it != positions.end()) {
            CashFutTradeRecord rec;
            rec.symbol = ctx.symbol;
            rec.entry_cash_ask = it->second.legs.front().entry_cash_ask;
            rec.entry_fut_bid = it->second.legs.front().entry_fut_bid;
            rec.exit_cash_bid = cashLeg.price;
            rec.exit_fut_ask = futLeg.price;
            history.push_back(rec);
            
            positions.erase(it);
            posState.Save(lk);
            TRACE("Saved trade history and removed active position.");
        }
    }
    
    releaseAndReturn();
}
#endif

long Strategy::GenerateOrderId() {
  long seq = ++orderSeq_;
  if (seq >= safeOrderMax_) {
      safeOrderMax_ = seq + 500;
      std::ofstream OutFile("hermes_refno.seq", std::ios::trunc);
      if (OutFile.is_open()) OutFile << safeOrderMax_;
  }
  return seq;
}

void Strategy::LoggerWorkerLoop() {
  while (running_) {
    LogEntry entry;
    {
      std::unique_lock<std::mutex> lock(logMutex_);
      logCv_.wait(lock, [&]() { return !running_ || !logQueue_.empty(); });
      if (!running_ && logQueue_.empty()) break;
      entry = std::move(logQueue_.front());
      logQueue_.pop();
    }
    if (entry.type == LogEntry::OP_LOG) {
        // Output to CSV
    } else if (entry.type == LogEntry::TRACE_EXEC) {
        // Output to trace log
    }
  }
}

void Strategy::PushToLog(LogEntry::Type type, const std::string& content, const std::string& tag) {
  std::lock_guard<std::mutex> lock(logMutex_);
  logQueue_.push({type, content, tag});
  logCv_.notify_one();
}

void Strategy::PushMonitorLogEntry(const std::string& msg) {
  // ImGui Monitor alert hook
}

void Strategy::PrintTUI() const {
}
