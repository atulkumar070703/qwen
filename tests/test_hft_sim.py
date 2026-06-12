from hft_sim.events import OrderIntent, Quote, Side
from hft_sim.gateway import PaperGateway
from hft_sim.main import run_simulation
from hft_sim.order_book import TopOfBook
from hft_sim.portfolio import Portfolio
from hft_sim.risk import RiskEngine


def test_order_book_rejects_stale_quotes() -> None:
    book = TopOfBook(symbol="DEMO")
    book.apply(Quote(symbol="DEMO", bid=99.99, ask=100.01, bid_size=100, ask_size=100, sequence=1))
    try:
        book.apply(Quote(symbol="DEMO", bid=100.0, ask=100.02, bid_size=100, ask_size=100, sequence=1))
    except ValueError as exc:
        assert "stale" in str(exc)
    else:
        raise AssertionError("stale quote was accepted")


def test_risk_engine_enforces_kill_switch() -> None:
    risk = RiskEngine(portfolio=Portfolio(), kill_switch_enabled=True)
    order = OrderIntent(symbol="DEMO", side=Side.BUY, price=100.01, quantity=10, strategy_id="test")
    decision = risk.check(order, reference_price=100.0)
    assert not decision.accepted
    assert "kill switch" in decision.reason


def test_gateway_fills_marketable_buy() -> None:
    quote = Quote(symbol="DEMO", bid=99.99, ask=100.01, bid_size=100, ask_size=100, sequence=1)
    order = OrderIntent(symbol="DEMO", side=Side.BUY, price=100.01, quantity=10, strategy_id="test")
    report = PaperGateway().send(order, quote)
    assert report.filled_quantity == 10
    assert report.fill_price == 100.01


def test_simulation_runs_end_to_end() -> None:
    result = run_simulation(ticks=200, symbol="DEMO")
    assert result["ticks"] == 200
    assert result["symbol"] == "DEMO"
    assert result["orders_sent"] >= 0
    assert "marked_pnl" in result
