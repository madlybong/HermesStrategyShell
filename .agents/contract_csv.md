# contract.csv — Column Conventions [CRITICAL]

> [!CAUTION]
> Use ExchangeToken (col 1) as the instrument token in ALL portal APIs.
> NEVER use GreekToken (col 0) — it is a broker-internal ID and will never
> match any token seen on the multicast wire.

## Column Map

| Col | Header | Example | Use |
|---|---|---|---|
| 0 | `GreekToken` | `102035036` | Broker internal — **IGNORED** |
| 1 | `ExchangeToken` | `35036` | **Library token** — primary key for all portal APIs |
| 2 | `ExchangeSegMent` | `NSEFO` | Segment identification |
| 3 | `Series/InstType` | `OPTIDX` | `OPTIDX`, `OPTSTK`, `FUTIDX`, `FUTSTK`, `EQ` |
| 4 | `Symbol` | `NIFTY` | Underlying symbol |
| 5 | `Description` | `NIFTY 26MAY26 CE 30700` | Human-readable label |
| 6 | `ExpiryDate` | `26MAY2026` | Parse → expiryEpochSec (NSE expiry = 15:30 IST) |
| 7 | `OptionType` | `CE` | `CE` → isCall=true, `PE` → isCall=false, `XX` = non-option |
| 8 | `StrikePrice` | `30700.00` | strike in GreeksConfig |

## Underlying Token Resolution
For Greeks: underlyingToken = ExchangeToken of nearest-expiry FUTIDX row with same Symbol.
Pre-build at load time: `g_underlyingBySymbol["NIFTY"] = ExchangeToken of nearest NIFTY FUTIDX`
