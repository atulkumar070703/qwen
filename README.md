# HFT Simulator Prototype

This repository contains an **educational high-frequency trading system prototype**. It is not connected to a real broker or exchange and must not be used for live trading.

The simulator shows the core shape of a trading stack:

```text
Market data -> order book -> strategy -> pre-trade risk -> paper gateway -> portfolio/PnL
```

## Components

- `hft_sim.market_data`: deterministic random-walk quote generator.
- `hft_sim.order_book`: in-memory top-of-book validation and state.
- `hft_sim.strategy`: simple mean-reversion strategy that emits order intents.
- `hft_sim.risk`: pre-trade checks for order size, notional, position, price bands, rate limits, and kill switch.
- `hft_sim.gateway`: paper gateway that fills marketable limit orders.
- `hft_sim.portfolio`: position, cash, fill count, and mark-to-market PnL tracking.
- `hft_sim.main`: command-line simulation runner.

## Run

```bash
python -m hft_sim.main --ticks 1000 --symbol DEMO
```

Example output:

```json
{
  "final_mid": 100.16,
  "final_position": -100,
  "marked_pnl": 7.2,
  "orders_rejected": 0,
  "orders_sent": 18,
  "symbol": "DEMO",
  "ticks": 1000
}
```

## Test

```bash
python -m pytest
```

## Safety notes

This is a teaching scaffold only. A production trading system needs exchange certification, audited controls, deterministic replay, drop-copy reconciliation, strong monitoring, disaster recovery, legal/compliance review, and operational supervision.
