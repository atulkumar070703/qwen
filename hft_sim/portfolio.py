"""Position and PnL tracking."""

from __future__ import annotations

from dataclasses import dataclass, field

from hft_sim.events import ExecutionReport, OrderStatus, Side


@dataclass(slots=True)
class Position:
    quantity: int = 0
    cash: float = 0.0
    fills: int = 0

    def mark_to_market(self, mid_price: float) -> float:
        return self.cash + self.quantity * mid_price


@dataclass(slots=True)
class Portfolio:
    positions: dict[str, Position] = field(default_factory=dict)

    def on_execution(self, report: ExecutionReport) -> None:
        if report.status != OrderStatus.FILLED or report.fill_price is None:
            return
        pos = self.positions.setdefault(report.order.symbol, Position())
        signed_qty = report.filled_quantity if report.order.side == Side.BUY else -report.filled_quantity
        cash_delta = -signed_qty * report.fill_price
        pos.quantity += signed_qty
        pos.cash += cash_delta
        pos.fills += 1

    def quantity(self, symbol: str) -> int:
        return self.positions.get(symbol, Position()).quantity

    def equity(self, symbol: str, mid_price: float) -> float:
        return self.positions.get(symbol, Position()).mark_to_market(mid_price)
