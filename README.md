# HermesFutCashStrategy

A C++20 High-Frequency Trading strategy for executing Cash vs Futures arbitrage on the Hermes architecture.

## Getting Started

1. Clone your repository.
2. Run `git submodule update --init` to pull the `extern/` dependencies (`HermesPortal`, `HermesTrader`, `HermesCommon`).
3. Read `.agents/SKILLS.md` and `.agents/RULES.md` for architectural constraints.
4. Read `STRATEGY.md` for mathematical models and signal gating logic.
5. Configure in `env.example` (and copy to `.env`). Ensure you set valid portal credentials and charge factors.

## Building

**Windows (Git Bash is Mandatory)**
```bash
& "C:\Program Files\Git\bin\bash.exe" build.sh
```

**Linux**
```bash
./build.sh
```

### Running the TUI / Dashboard
By default, the strategy runs headlessly, printing minimal status ticks to the console.
- Run with `--monitor` to launch the ImGui DX11 visual dashboard (Windows only). This dashboard dynamically computes Live Net PnL and Expected Final PnL.

### Running Tests
The strategy integrates with Catch2 for unit testing the arbitrage spread calculations and charge models.
```bash
run_tests.bat
```

## Architecture Summary

- **Event Loop**: Powered by `HermesPortal` via `IListener::onMarketTick`.
- **Execution**: Dispatched to `HermesTrader` lock-free SPSC queues. Anchor legs are filled sequentially before Hedging.
- **Config**: Live-reloaded via `ConfigLoader`.
- **Dashboard**: `ImGuiMonitor` visualizes the active `marketStore_` states.
