# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0.0] - 2026-06-02
### Added
- Production-grade logger in `LoggerWorkerLoop`:
  - Daily date-stamped log files (`YYYY-MM-DD_exec_trace.log`, `YYYY-MM-DD_<tag>_op.log`)
  - Auto-creates `logs/<LOG_DIR_NAME>/` directory at startup (once, not per batch)
  - Streams held open for process lifetime — no per-batch file re-open overhead
  - Batch drain: all queued entries processed under a single mutex acquisition
  - Batch flush: `f.flush()` once per batch, not per entry (eliminates `std::endl` overhead)
  - CSV header guard: `OP_LOG_HEADER` written iff daily file is newly created (`tellp() == 0`)
  - Tag-based op-log routing: `PushToLog(OP_LOG, row, "TAG")` → `YYYY-MM-DD_TAG_op.log`
- Two new `AppConfig` fields: `LOG_DIR_NAME` (default `"strategy"`) and `OP_LOG_HEADER` (default `""`)
- Both fields are `.env`-configurable and documented in `env.example`
### Changed
- `LogEntry` gains optional `std::string tag` field (default `""` — backward compatible)
- `PushToLog` gains optional third argument `const std::string& tag = ""` (backward compatible)
### Fixed
- `orders.csv` previously written flat in CWD with no header; now written to dated, headered file in `logs/<LOG_DIR_NAME>/`

## [0.1.0.0] - 2026-05-29
### Added
- Initial Strategy Shell template.
- Integrated `HermesPortal`, `HermesTrader`, and `HermesCommon` submodules.
- Unified build script (`build.sh`) supporting Windows (vcpkg) and Linux.
- Added `.agents/` directory structure for strategy documentation and rules.
- Base `ConfigLoader`, lock-free `SPSCQueue`, and `PinThread` utilities.
