"""Paper order gateway and simple matching model."""

from __future__ import annotations

from dataclasses import dataclass
from uuid import uuid4

from hft_sim.events import ExecutionReport, OrderIntent, OrderStatus, Quote, Side


@dataclass(slots=True)
class PaperGateway:
    """Immediately acknowledges and fills marketable limit orders."""

    def send(self, order: OrderIntent, quote: Quote) -> ExecutionReport:
        if order.side == Side.BUY and order.price >= quote.ask:
            return ExecutionReport(
                order=order,
                status=OrderStatus.FILLED,
                filled_quantity=min(order.quantity, quote.ask_size),
                fill_price=quote.ask,
                exchange_order_id=uuid4().hex,
            )
        if order.side == Side.SELL and order.price <= quote.bid:
            return ExecutionReport(
                order=order,
                status=OrderStatus.FILLED,
                filled_quantity=min(order.quantity, quote.bid_size),
                fill_price=quote.bid,
                exchange_order_id=uuid4().hex,
            )
        return ExecutionReport(order=order, status=OrderStatus.ACKED, exchange_order_id=uuid4().hex)
