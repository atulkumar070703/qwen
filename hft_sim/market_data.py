"""Market data source used by the educational simulator."""

from __future__ import annotations

import random
from collections.abc import Iterator
from dataclasses import dataclass

from hft_sim.events import Quote


@dataclass(slots=True)
class RandomWalkMarketData:
    """Generate deterministic top-of-book quotes with a configurable seed."""

    symbol: str = "DEMO"
    start_price: float = 100.0
    spread: float = 0.02
    size: int = 100
    seed: int = 7
    volatility_ticks: int = 3
    tick_size: float = 0.01

    def stream(self, count: int) -> Iterator[Quote]:
        rng = random.Random(self.seed)
        mid = self.start_price
        for sequence in range(1, count + 1):
            move_ticks = rng.randint(-self.volatility_ticks, self.volatility_ticks)
            mid = max(self.tick_size, mid + move_ticks * self.tick_size)
            half_spread = self.spread / 2.0
            bid = round(mid - half_spread, 2)
            ask = round(mid + half_spread, 2)
            yield Quote(
                symbol=self.symbol,
                bid=bid,
                ask=ask,
                bid_size=self.size + rng.randint(0, 50),
                ask_size=self.size + rng.randint(0, 50),
                sequence=sequence,
            )
