# HermesPortal API Reference

## 1. Portal Lifecycle
```cpp
Portal::Create() → SetListener() → Start(cfg) → [ticks flow] → Stop() → Destroy()
```

## 2. IListener Interface
```cpp
class IListener {
public:
    virtual ~IListener() = default;

    // Primary market data — fires for every tick. MUST implement. Do NOT block.
    virtual void onMarketTick(const MarketTick& tick) = 0;

    // Index broadcast (NSE 7207/7216/7203). Default: no-op.
    virtual void onIndexTick(const IndexTick& idx) { (void)idx; }

    // Greeks computed for a registered option token. Default: no-op.
    virtual void onGreeksData(const GreeksData& gd) { (void)gd; }
    
    // Raw packet diagnostic hook. Default: no-op.
    virtual void onDebugPacket(...) {}
};
```
> ⚠️ **Thread Safety**: Callbacks fire from receiver threads (one per feed). If you have multiple feeds, callbacks may fire concurrently from different threads. Protect shared state.

## 3. MarketTick Fields
| Field | Type | Description |
|---|---|---|
| `token` | `uint32_t` | Exchange-native instrument token |
| `exchange` | `Exchange` | `NSE`, `BSE`, or `MCX` |
| `segment` | `Segment` | `EQ`, `FO`, `CDS`, or `COM` |
| `ltp` | `float` | Last Traded Price |
| `atp` | `float` | Average Traded Price |
| `volume` | `uint64_t` | Total Traded Quantity |
| `openInterest` | `uint64_t` | Open Interest |
| `bidPrices[10]` | `float` | Bid prices (descending) |
| `askPrices[10]` | `float` | Ask prices (ascending) |
| `bidQtys[10]` | `int64_t` | Bid quantities |
| `askQtys[10]` | `int64_t` | Ask quantities |
| `exchangeTime` | `uint64_t` | Exchange timestamp (ns) |
| `localTime` | `uint64_t` | Local arrival timestamp (ns) |
| `flags` | `uint8_t` | bit0 = seq gap, bit1 = MTBT reconstructed |
| `depthLevels` | `uint8_t` | Number of valid depth entries (0-10) |

## 4. Config Setup
Key config fields via `.env` / `AppConfig`:
- `PORTAL_FEEDS`: `EXCHANGE:SEGMENT:MCAST_IP:PORT[:CPU_CORE]`
- `PORTAL_INTERFACE_IP`: NIC bind IP
- `PORTAL_DATA_MODE`: `COMPRESSED` or `MTBT`
- `PORTAL_FO_EXPIRIES`: Subscription expiry filter

## 5. Greeks API
```cpp
// 1. Configure token
Hermes::GreeksConfig gc;
gc.token = 35008; 
gc.underlyingToken = 26000;
gc.strike = 24000.0f;
gc.expiryEpochSec = 1748524200;
gc.isCall = true;
gc.riskFreeRate = 0.065f;
cfg.greeksConfigs.push_back(gc);

// 2. Poll (thread-safe, non-blocking)
Hermes::GreeksData gd;
if (portal->PeekGreeks(35008, gd)) {
    // gd.iv, gd.delta, gd.gamma, gd.theta, gd.vega
}
```
