# [Strategy Name] — Reference Manual

> Version: vX.Y.Z · Engine: [RepoName]

## 1. Core Concept
[Describe the arbitrage/market inefficiency being exploited]

## 2. Trade Types
[Enumerate each trade variant with entry condition, leg direction, profit/loss scenario]

## 3. Mathematical Model
[Formulas for spread/diff, expenses, threshold]

## 4. Signal Gating
[Freshness check, debounce, O(1) lookup approach]

## 5. Execution Flow
[Phase-based execution diagram — Anchor → Hedge → DPR fallback]

## 6. Exit Logic
[TP/SL conditions, formula]

## 7. Risk Controls
[Symbol caps, cooldowns, stale tick guard, dry run mode, order ID namespace]

## 8. Expense Model
[Charges formula: brokerage, STT, interest, safety buffer]

## 9. Performance Architecture
[O(1) hot path design, lock-free patterns used]

## 10. Configuration Reference
[Table of all .env keys specific to this strategy]

## 11. Component Map
[List of executables, data files, their roles]

---

## Execution Observability (MANDATORY for HAS_HERMES_TRADER builds)

When implementing `ExecuteStrategy`:
1. **Always** call `TRACE("Phase N: ...")` before and after each `PlaceOrder` call
2. **Always** log PlaceOrder return explicitly: `TRACE("Leg: " + (id.empty() ? "FAILED" : id))`
3. **Never** use `std::endl` inside TRACE — use `"\n"` (TRACE accumulates in stringstream)
4. **Always** push the final trace: `PushToLog(LogEntry::TRACE_LOG, "=== EXECUTION CYCLE " + trace.str())`
5. **Guard** `IsOrderCompleted`/`IsOrderRejected` against empty orderId — return true immediately
