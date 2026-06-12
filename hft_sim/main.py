"""Command-line runner for the educational HFT simulator."""

from __future__ import annotations

import argparse
import json

from hft_sim.gateway import PaperGateway
from hft_sim.market_data import RandomWalkMarketData
from hft_sim.order_book import TopOfBook
from hft_sim.portfolio import Portfolio
from hft_sim.risk import RiskEngine
from hft_sim.strategy import MeanReversionStrategy


def run_simulation(ticks: int, symbol: str) -> dict[str, float | int | str]:
    market_data = RandomWalkMarketData(symbol=symbol)
    book = TopOfBook(symbol=symbol)
    portfolio = Portfolio()
    risk = RiskEngine(portfolio=portfolio)
    gateway = PaperGateway()
    strategy = MeanReversionStrategy()
    sent_orders = 0
    rejected_orders = 0

    for quote in market_data.stream(ticks):
        book.apply(quote)
        for order in strategy.on_quote(quote, portfolio):
            decision = risk.check(order, reference_price=quote.mid)
            if not decision.accepted:
                rejected_orders += 1
                continue
            sent_orders += 1
            portfolio.on_execution(gateway.send(order, quote))

    final_quote = book.quote
    if final_quote is None:
        raise RuntimeError("simulation produced no market data")
    final_position = portfolio.quantity(symbol)
    final_equity = portfolio.equity(symbol, final_quote.mid)
    return {
        "symbol": symbol,
        "ticks": ticks,
        "orders_sent": sent_orders,
        "orders_rejected": rejected_orders,
        "final_position": final_position,
        "final_mid": round(final_quote.mid, 4),
        "marked_pnl": round(final_equity, 4),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the educational HFT simulator")
    parser.add_argument("--ticks", type=int, default=1_000, help="number of market data ticks to replay")
    parser.add_argument("--symbol", default="DEMO", help="symbol to simulate")
    args = parser.parse_args()
    print(json.dumps(run_simulation(ticks=args.ticks, symbol=args.symbol), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
