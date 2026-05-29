# Hermes Strategy Shell

A GitHub Template Repository for bootstrapping C++20 High-Frequency Trading strategies on the Hermes architecture.

## Getting Started

1. Click **Use this template** on GitHub to create a new strategy repository.
2. Clone your new repository.
3. Run `git submodule update --init` to pull the `extern/` dependencies (`HermesPortal`, `HermesTrader`, `HermesCommon`).
4. Read `.agents/SKILLS.md` and `.agents/RULES.md` for architectural constraints.
5. Define your strategy logic in `STRATEGY.md`.
6. Implement in `src/Strategy.cpp`.
7. Configure in `env.example` (and copy to `.env`).

## Building

**Windows (Git Bash is Mandatory)**
```bash
& "C:\Program Files\Git\bin\bash.exe" build.sh
```

**Linux**
```bash
./build.sh
```

## Architecture Summary

- **Event Loop**: Powered by `HermesPortal` via `IListener::onMarketTick`.
- **Execution**: Dispatched to `HermesTrader` lock-free queues.
- **Config**: Live-reloaded via `ConfigLoader`.
- **Docs**: Centralized in `.agents/` for both human and AI reference.
