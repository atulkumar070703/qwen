"""Shared domain objects for the HFT simulator."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import StrEnum
from time import time_ns
from typing import Literal
from uuid import uuid4


class Side(StrEnum):
    BUY = "BUY"
    SELL = "SELL"


class OrderStatus(StrEnum):
    NEW = "NEW"
    ACKED = "ACKED"
    FILLED = "FILLED"
    REJECTED = "REJECTED"
    CANCELLED = "CANCELLED"


@dataclass(frozen=True, slots=True)
class Quote:
    symbol: str
    bid: float
    ask: float
    bid_size: int
    ask_size: int
    sequence: int
    exchange_ts_ns: int = field(default_factory=time_ns)
    receive_ts_ns: int = field(default_factory=time_ns)

    @property
    def mid(self) -> float:
        return (self.bid + self.ask) / 2.0

    @property
    def spread(self) -> float:
        return self.ask - self.bid


@dataclass(frozen=True, slots=True)
class OrderIntent:
    symbol: str
    side: Side
    price: float
    quantity: int
    strategy_id: str
    order_type: Literal["LIMIT"] = "LIMIT"
    client_order_id: str = field(default_factory=lambda: uuid4().hex)
    created_ts_ns: int = field(default_factory=time_ns)

    @property
    def notional(self) -> float:
        return self.price * self.quantity


@dataclass(frozen=True, slots=True)
class ExecutionReport:
    order: OrderIntent
    status: OrderStatus
    filled_quantity: int = 0
    fill_price: float | None = None
    reason: str | None = None
    exchange_order_id: str | None = None
    event_ts_ns: int = field(default_factory=time_ns)
