"""Example strategy module."""

from __future__ import annotations

from dataclasses import dataclass

from hft_sim.events import OrderIntent, Quote, Side
from hft_sim.portfolio import Portfolio


@dataclass(slots=True)
class MeanReversionStrategy:
    """Tiny educational strategy that trades when price moves away from an EMA."""

    strategy_id: str = "mean-reversion-demo"
    order_qty: int = 10
    threshold_bps: float = 2.0
    ema_alpha: float = 0.2
    inventory_soft_limit: int = 100
    _ema: float | None = None

    def on_quote(self, quote: Quote, portfolio: Portfolio) -> list[OrderIntent]:
        mid = quote.mid
        self._ema = mid if self._ema is None else self.ema_alpha * mid + (1.0 - self.ema_alpha) * self._ema
        deviation_bps = (mid - self._ema) / self._ema * 10_000.0
        inventory = portfolio.quantity(quote.symbol)
        if deviation_bps <= -self.threshold_bps and inventory < self.inventory_soft_limit:
            return [
                OrderIntent(
                    symbol=quote.symbol,
                    side=Side.BUY,
                    price=quote.ask,
                    quantity=self.order_qty,
                    strategy_id=self.strategy_id,
                )
            ]
        if deviation_bps >= self.threshold_bps and inventory > -self.inventory_soft_limit:
            return [
                OrderIntent(
                    symbol=quote.symbol,
                    side=Side.SELL,
                    price=quote.bid,
                    quantity=self.order_qty,
                    strategy_id=self.strategy_id,
                )
            ]
        return []
