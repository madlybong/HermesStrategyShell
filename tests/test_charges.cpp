#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <iostream>

double CalcEquityCharges(double cashPrice, int lotSize) {
    double EQUITY_STT_BUY_PCT = 0.1;
    double EQUITY_STT_SELL_PCT = 0.1;
    double EQUITY_EXCHANGE_FEE = 33.5;
    double EQUITY_SEBI_FEE = 1.0;
    double EQUITY_STAMP_DUTY = 15.0;

    double turnover = cashPrice * lotSize;
    double stt = (turnover * EQUITY_STT_BUY_PCT / 100.0) + (turnover * EQUITY_STT_SELL_PCT / 100.0);
    double exchange = 2 * (turnover * EQUITY_EXCHANGE_FEE / 10000000.0);
    double sebi = 2 * (turnover * EQUITY_SEBI_FEE / 10000000.0);
    double stamp = 2 * (turnover * EQUITY_STAMP_DUTY / 10000000.0);
    return stt + exchange + sebi + stamp;
}

double CalcFuturesCharges(double futPrice, int lotSize) {
    double FUTURE_CHARGES = 3000.0;
    double turnover = futPrice * lotSize;
    return (turnover * 2) * (FUTURE_CHARGES / 10000000.0); // round trip
}

TEST_CASE("Charge Calculations", "[charges]") {
    // Example: RELIANCE EQ @ 2900, lot size 250
    double cashPrice = 2900.0;
    int lotSize = 250;
    double equity_charge = CalcEquityCharges(cashPrice, lotSize);
    
    REQUIRE(std::abs(equity_charge - 1457.1775) <= 0.001);
    
    double futPrice = 2920.0;
    double fut_charge = CalcFuturesCharges(futPrice, lotSize);
    
    REQUIRE(std::abs(fut_charge - 438.0) <= 0.001);
}
