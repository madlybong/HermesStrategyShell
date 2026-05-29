# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0.0] - 2026-05-29
### Added
- Initial Strategy Shell template.
- Integrated `HermesPortal`, `HermesTrader`, and `HermesCommon` submodules.
- Unified build script (`build.sh`) supporting Windows (vcpkg) and Linux.
- Added `.agents/` directory structure for strategy documentation and rules.
- Base `ConfigLoader`, lock-free `SPSCQueue`, and `PinThread` utilities.
