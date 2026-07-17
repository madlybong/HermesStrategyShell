# HermesFutCashStrategy — Reference Manual

> Version: v0.1.0 · Engine: HermesFutCashStrategy

## 1. Core Concept
Exploits the price convergence guarantee between an NSE equity (cash) stock and its current-month futures contract. At expiry, futures MUST settle at the cash price. Spread narrows to zero → profit realized.

## 2. Trade Types
**Forward Arbitrage:**
- **Entry:** Buy Cash (`NSE:EQ`) at Ask, Sell Future (`NSE:FO`) at Bid.
- **Profit Scenario:** Entered when `fut_bid > cash_ask`. Profit realized as spread compresses toward zero.

**Reverse Arbitrage:** *(Concept Supported, Implementation Pending)*
- **Entry:** Sell Cash (`NSE:EQ`) at Bid, Buy Future (`NSE:FO`) at Ask.
- **Profit Scenario:** Entered when `cash_bid > fut_ask`. Profit realized as spread compresses toward zero.

## 3. Mathematical Model
```
Forward Executable Spread = fut_bid - cash_ask
Gross PnL = Forward Executable Spread * lot_size
Margin = MARGIN_PER_TRADE (Flat, e.g. 500,000)

Cash Charges = (entry_cash * lot_size * EQUITY_STT_BUY_PCT / 100)
             + (exit_cash * lot_size * EQUITY_STT_SELL_PCT / 100)
             + 2 * (entry_cash * lot_size * (EQUITY_EXCHANGE_FEE + EQUITY_SEBI_FEE + EQUITY_STAMP_DUTY) / 1e7)
Fut Charges = (entry_fut * 2) * lot_size * (FUTURE_CHARGES / 1e7)
Slippage = SLIPPAGE_POINTS * 4 * lot_size (4 points per leg)

Net PnL = Gross PnL - Cash Charges - Fut Charges - Slippage
ROI = (Net PnL / Margin) * 100

Gate: ROI >= MIN_ROI_PCT
```

## 4. Signal Gating
1. Read cash BBO + fut BBO from marketStore_
2. Gate: Executable spread > 0 AND ROI >= MIN_ROI_PCT
3. No active position for this pair

## 5. Execution Flow
Sequential Phase Execution:
- **Phase 1 (Anchor):** Sell Future (`NSE:FO`). Wait `FUT_WAIT_MS`. Cancel if unfilled (unless DRY_RUN).
- **Phase 2 (Hedge):** Buy Cash (`NSE:EQ`). Wait `CASH_WAIT_MS`. On timeout → Upgrade to DPR (cash_ask + 15) for guaranteed fill.

## 6. Dashboard Calculations
- **Live Net PnL (Mark-to-Market):** Simulates closing the position instantly using current live order book data (e.g., exiting Cash at live Bid, Future at live Ask).
- **Expected Final PnL:** Projects PnL if the Future price eventually converges with the Entry Cash price (typically at Expiry). Assumes `exit_cash = entry_cash` and `exit_future = entry_cash`.

## 7. Exit Logic
- **TP (Take Profit):** `net_pnl > MARGIN_PER_TRADE * (EXIT_PROFIT_PCT / 100)`
- **SL (Stop Loss):** `net_pnl <= -(MARGIN_PER_TRADE * (STOP_LOSS_PCT / 100))`
- **Time Exit:** `trading_days_to_expiry <= TIME_EXIT_DAYS`
Exits fire both legs simultaneously at DPR (aggressive floor/ceiling).

## 8. Risk Controls
- Dividend Risk: Strategy does not auto-filter ex-dividend dates. Operators must manage this manually.
- Max Orders per Symbol from `FnoStocks.csv`
- Time Exit prevents physical delivery settlement on cash legs.
