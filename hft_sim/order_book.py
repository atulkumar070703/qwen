"""In-memory top-of-book state."""

from __future__ import annotations

from dataclasses import dataclass

from hft_sim.events import Quote


@dataclass(slots=True)
class TopOfBook:
    """Tracks the latest valid bid/ask quote for one symbol."""

    symbol: str
    quote: Quote | None = None

    def apply(self, quote: Quote) -> None:
        if quote.symbol != self.symbol:
            raise ValueError(f"quote symbol {quote.symbol!r} does not match book {self.symbol!r}")
        if quote.bid <= 0 or quote.ask <= 0:
            raise ValueError("bid and ask must be positive")
        if quote.bid >= quote.ask:
            raise ValueError("crossed or locked quote rejected")
        if self.quote and quote.sequence <= self.quote.sequence:
            raise ValueError("stale quote sequence rejected")
        self.quote = quote

    @property
    def mid(self) -> float:
        if self.quote is None:
            raise RuntimeError("book has no quote yet")
        return self.quote.mid
