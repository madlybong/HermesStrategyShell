#include "version.h"
#include "ConfigLoader.h"
#include <ContractManager.h>
#include "Strategy.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

int main(int argc, char *argv[]) {
  bool debugMode = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--debug") { debugMode = true; break; }
  }

  std::cout << "[HSS] HermesStrategyShell v" << HSS_VERSION 
            << (debugMode ? " (DEBUG MODE)" : "") << "...\n";

  auto config = std::make_shared<ConfigLoader>(".env");
  AppConfig appCfg = config->GetConfig();

  auto cm = std::make_shared<hermes::ContractManager>();
  if (!cm->LoadContracts("")) {
    std::cerr << "Failed to load contracts. Check ASSET_CONTRACT_PATH and ASSET_FNO_FILE in .env\n";
  }

  std::shared_ptr<Strategy> strategy;
  try {
    strategy = std::make_shared<Strategy>(cm, config);
  } catch (const std::exception &e) {
    std::cerr << "[FATAL] Strategy Initialization Error: " << e.what() << "\n";
    return 1;
  }

  strategy->SetDebugMode(debugMode);

  Hermes::Portal *portal = Hermes::Portal::Create();
  portal->SetListener(strategy.get());

  Hermes::Config cfg;
  cfg.interfaceIp  = appCfg.PORTAL_INTERFACE_IP;
  cfg.rcvBufSize   = appCfg.PORTAL_RCVBUF_SIZE;
  // TODO: Add Hermes::FeedSource to cfg.feeds based on PORTAL_FEEDS env logic or manual setup

  auto SplitCsv = [](const std::string &s) {
    std::vector<std::string> result;
    std::stringstream ss(s); std::string item;
    while (std::getline(ss, item, ',')) { if (!item.empty()) result.push_back(item); }
    return result;
  };

  // cfg.mtbtAllowScripts = SplitCsv(appCfg.PORTAL_ALLOW_SCRIPTS); // Example
  // cfg.mtbtFoExpiries   = SplitCsv(appCfg.PORTAL_FO_EXPIRIES);   // Example

  std::cout << "Connecting to Portal...\n";
  portal->Start(cfg);
  std::cout << "Running... Press Ctrl+C to stop.\n";

  int ticks = 0, tuiTick = 0;
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    config->Update();

    if (debugMode) {
      if (++tuiTick % 10 == 0) {  // refresh every 2 seconds
        strategy->PrintTUI();
      }
    } else {
      if (++ticks % 5 == 0) std::cout << "." << std::flush;
    }
  }

  portal->Stop();
  Hermes::Portal::Destroy(portal);
  return 0;
}
