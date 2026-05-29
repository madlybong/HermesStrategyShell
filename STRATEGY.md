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
