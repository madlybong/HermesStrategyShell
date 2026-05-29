---
name: Strategy Initialization
about: Use this template to kick off the design phase of a new strategy
title: '[INIT] Strategy Design Document'
labels: enhancement, design
assignees: ''

---

## 1. Core Concept
Briefly explain the market inefficiency this strategy attempts to capture.

## 2. Requirements Checklist
- [ ] Define entry logic (prices, sizes, indicators)
- [ ] Define exit logic (TP, SL, timeout)
- [ ] Define the expense model (Brokerage, STT, Exchange charges)
- [ ] List new `.env` parameters required
- [ ] Outline specific risk controls needed (e.g., max open positions)

## 3. Next Steps
- Fill out `STRATEGY.md` with detailed mathematical models.
- Review `.agents/RULES.md` before implementation.
