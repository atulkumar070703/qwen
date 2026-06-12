"""Fast pre-trade risk controls."""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
from time import time

from hft_sim.events import OrderIntent, Side
from hft_sim.portfolio import Portfolio


@dataclass(frozen=True, slots=True)
class RiskDecision:
    accepted: bool
    reason: str = "OK"


@dataclass(slots=True)
class RiskEngine:
    """Applies simple pre-trade controls before orders reach the gateway."""

    portfolio: Portfolio
    max_order_qty: int = 100
    max_symbol_position: int = 500
    max_order_notional: float = 25_000.0
    max_orders_per_second: int = 20
    price_band_bps: float = 100.0
    kill_switch_enabled: bool = False
    _recent_order_times: deque[float] = field(default_factory=deque)

    def check(self, order: OrderIntent, reference_price: float) -> RiskDecision:
        if self.kill_switch_enabled:
            return RiskDecision(False, "global kill switch enabled")
        if order.quantity <= 0:
            return RiskDecision(False, "quantity must be positive")
        if order.quantity > self.max_order_qty:
            return RiskDecision(False, "max order quantity exceeded")
        if order.notional > self.max_order_notional:
            return RiskDecision(False, "max order notional exceeded")
        current_qty = self.portfolio.quantity(order.symbol)
        projected_qty = current_qty + (order.quantity if order.side == Side.BUY else -order.quantity)
        if abs(projected_qty) > self.max_symbol_position:
            return RiskDecision(False, "max symbol position exceeded")
        max_deviation = reference_price * self.price_band_bps / 10_000.0
        if abs(order.price - reference_price) > max_deviation:
            return RiskDecision(False, "price outside allowed band")
        now = time()
        while self._recent_order_times and now - self._recent_order_times[0] > 1.0:
            self._recent_order_times.popleft()
        if len(self._recent_order_times) >= self.max_orders_per_second:
            return RiskDecision(False, "order rate limit exceeded")
        self._recent_order_times.append(now)
        return RiskDecision(True)
