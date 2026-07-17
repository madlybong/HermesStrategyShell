# AGENTS.md — HermesFutCashStrategy Agent Index

This is the entry point for AI agents and human developers.
Read the relevant file from `.agents/` for your task:

| If you are... | Read... |
|---|---|
| New to this project | `.agents/SKILLS.md` then `.agents/RULES.md` |
| Implementing strategy logic | `STRATEGY.md` |
| Working on ConfigLoader or .env | `.agents/RULES.md` §Config |
| Looking up MarketTick fields | `.agents/PORTAL_API.md` |
| Debugging token mismatches | `contract.csv` |

## Critical Rules (Always Apply)
> extern/ is READ-ONLY. Never modify submodules.
> Build via Git Bash: & "C:\Program Files\Git\bin\bash.exe" build.sh
> Dual Feeds: Must configure PORTAL_FEEDS to include both NSE:FO and NSE:EQ
> Anchor Leg: Futures leg (NSE:FO) is always placed first, Cash (NSE:EQ) follows.
> See `.agents/RULES.md` for the full constraint set.
