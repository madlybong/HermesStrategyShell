# AGENTS.md — HermesStrategyShell Agent Index

This is the entry point for AI agents and human developers.
Read the relevant file from `.agents/` for your task:

| If you are... | Read... |
|---|---|
| New to this project | `.agents/SKILLS.md` then `.agents/RULES.md` |
| Implementing strategy logic | `.agents/STRATEGY_TEMPLATE.md`, `.agents/PORTAL_API.md` |
| Working on ConfigLoader or .env | `.agents/RULES.md` §Config |
| Looking up MarketTick fields | `.agents/PORTAL_API.md` |
| Debugging token mismatches | `.agents/contract_csv.md` |
| Building or deploying | `.agents/RULES.md` §Build |

## Critical Rules (Always Apply)
> extern/ is READ-ONLY. Never modify submodules.
> Build via Git Bash: & "C:\Program Files\Git\bin\bash.exe" build.sh
> See `.agents/RULES.md` for the full constraint set.
