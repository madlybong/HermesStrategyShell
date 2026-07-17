## 1. Submodule Isolation [CRITICAL]
All files under `extern/` are strictly READ-ONLY.
Never propose or execute changes to any file under extern/.

## 2. Architecture Integrity
- O(1) Hot Path: token-indexed array lookups, no linear scans on tick callbacks
- Lock-Free Principles: SPSC queues + atomics on all hot paths
- Async Logging: zero-block — no I/O on execution threads
- Phase-Based Execution: Anchor leg fills BEFORE Hedge leg placed
- Zero-Allocation Hot Path: no heap allocations inside onMarketTick()

## 3. Config & Environment
- All config flows through ConfigLoader (.env → struct → strategy)
- Never hardcode parameter defaults in Strategy.cpp — add to AppConfig + env.example
- `.env` is gitignored; `env.example` is the documented reference
- Call `config->Update()` in the main loop for live hot-reload

## 4. Thread Safety Standards
- std::atomic for per-token flags and debounce state
- std::shared_mutex for read-heavy state (contract maps, strike tables)
- std::unique_ptr / std::shared_ptr for all heap resources (RAII)

## 5. Risk Controls (Mandatory)
- Daily symbol order caps enforced via ordersMutex_ (never bypass)
- Signal debounce: per-pair cooldown prevents rapid re-entry
- Stale tick guard: LTT_RANGE check before processing any signal
- High-water mark: order ID persisted to hermes_refno.seq — never reset without deliberate bump

## 6. Build & Test Environment [STRICT]
Windows: always build via Git Bash
  & "C:\Program Files\Git\bin\bash.exe" build.sh
Linux: ./build.sh
Never invoke cmake directly without the wrapper script.
Flags: --clean, --debug, --no-msmq, --no-local, --monitor

**Testing**: You MUST run `run_tests.bat` and ensure 100% test pass rates before declaring a build successful.

## 7. State Management
- Utilize `PositionState` for any multi-leg entry and exit execution logic rather than maintaining bare ad-hoc arrays.
- Track independent leg margins, costs, and PnL through the `TradeRecord` architecture.

## 8. Version Management
The VERSION file is the single source of truth.
To bump: edit VERSION → commit → tag vX.Y.Z.W
build.sh auto-stamps vcpkg.json. Never edit vcpkg.json version-string manually.

## 9. Strategy-Specific Rules (HermesFutOptStrategy)
- **Bid/Ask Spread Execution:** ALWAYS read depths (bid/ask) instead of LTP to simulate executable opportunities.
- **Slippage Modeling:** Apply a strict 4-point penalty per leg per trade to offset realistic market impact.
- **Sequential Leg Fills (Anchor First):** For Forward arbitrage, the Futures leg (NSE:FO) is always executed first. The Cash leg (NSE:EQ) follows only if the Futures fill succeeds.
- **ROI Gating:** Margin requirements are 100% upfront for cash, 20% for futures. Only issue `ENTER` if Expected ROI >= MIN_ROI_PERCENT.
