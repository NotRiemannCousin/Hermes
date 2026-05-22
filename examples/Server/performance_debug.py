"""
AsyncServer Stress Test
Measures avg performance as concurrent connections decrease.
Requires: pip install aiohttp pandas numpy
"""

import asyncio
import time
import sys
from dataclasses import dataclass

import aiohttp
import numpy as np
import pandas as pd

URL = "http://localhost:8080"

CONCURRENCY_LEVELS = [200, 150, 100, 75, 50, 25, 10, 5, 1]
REQUESTS_PER_LEVEL = 300
WARMUP_REQUESTS    = 20
REQUEST_TIMEOUT_S  = 10.0


# ──────────────────────────────────────────────
# Colect
# ──────────────────────────────────────────────

@dataclass
class Result:
    concurrency: int
    latency_ms: float
    success: bool
    status: int = 0
    error: str = ""


async def _single(session: aiohttp.ClientSession,
                  sem: asyncio.Semaphore,
                  concurrency: int,
                  out: list[Result]) -> None:
    async with sem:
        t0 = time.perf_counter()
        try:
            async with session.get(URL) as resp:
                await resp.read()
                ms = (time.perf_counter() - t0) * 1000
                if resp.status >= 400:
                    out.append(Result(concurrency, ms, False, resp.status, error=f"HTTP {resp.status}"))
                else:
                    out.append(Result(concurrency, ms, True, resp.status))
        except asyncio.TimeoutError:
            ms = (time.perf_counter() - t0) * 1000
            out.append(Result(concurrency, ms, False, error="timeout"))
        except Exception as exc:
            ms = (time.perf_counter() - t0) * 1000
            out.append(Result(concurrency, ms, False, error=f"{type(exc).__name__}: {str(exc)}"))


async def _run_level(concurrency: int, n: int) -> tuple[list[Result], float]:
    results: list[Result] = []
    sem = asyncio.Semaphore(concurrency)

    connector = aiohttp.TCPConnector(limit=concurrency, limit_per_host=concurrency)
    timeout   = aiohttp.ClientTimeout(total=REQUEST_TIMEOUT_S)

    async with aiohttp.ClientSession(connector=connector, timeout=timeout) as session:
        # warmup
        warmup_tasks = [_single(session, sem, concurrency, []) for _ in range(WARMUP_REQUESTS)]
        await asyncio.gather(*warmup_tasks)

        # medição real
        t_start = time.perf_counter()
        tasks = [_single(session, sem, concurrency, results) for _ in range(n)]
        await asyncio.gather(*tasks)
        elapsed = time.perf_counter() - t_start

    return results, elapsed


# ──────────────────────────────────────────────
# Analysis
# ──────────────────────────────────────────────

def _percentile(series: pd.Series, p: float) -> float:
    return float(np.percentile(series.dropna(), p))


def analyze(rows: list[Result]) -> tuple[pd.DataFrame, pd.DataFrame]:
    df = pd.DataFrame([r.__dict__ for r in rows])

    agg = (
        df.groupby("concurrency")["latency_ms"]
        .agg(
            count="count",
            avg=lambda x: round(x.mean(), 2),
            median=lambda x: round(float(np.percentile(x, 50)), 2),
            p95=lambda x: round(_percentile(x, 95), 2),
            p99=lambda x: round(_percentile(x, 99), 2),
            min="min",
            max="max",
            std=lambda x: round(x.std(), 2),
        )
        .reset_index()
    )

    success = (
        df.groupby("concurrency")["success"]
        .mean()
        .mul(100)
        .round(2)
        .rename("success_%")
        .reset_index()
    )

    summary = agg.merge(success, on="concurrency").sort_values(
        "concurrency", ascending=False
    )

    summary[["min", "max"]] = summary[["min", "max"]].round(2)

    return df, summary


# ──────────────────────────────────────────────
# Relatory
# ──────────────────────────────────────────────

def _bar(value: float, max_val: float, width: int = 30) -> str:
    filled = int(round(value / max_val * width)) if max_val else 0
    return "█" * filled + "░" * (width - filled)


def print_report(summary: pd.DataFrame, throughputs: dict[int, float]) -> None:
    summary = summary.copy()
    summary["req/s"] = summary["concurrency"].map(
        lambda c: round(throughputs.get(c, 0), 1)
    )

    max_avg = summary["avg"].max()
    max_rps = summary["req/s"].max()

    print("\n" + "═" * 80)
    print("  ASYNC SERVER — STRESS TEST RESULTS")
    print("═" * 80)
    print(
        f"  {'conc':>5}  {'avg ms':>7}  {'p50':>7}  {'p95':>7}  {'p99':>7}"
        f"  {'req/s':>7}  {'ok%':>6}  latency"
    )
    print("─" * 80)

    for _, row in summary.iterrows():
        bar = _bar(row["avg"], max_avg)
        print(
            f"  {int(row['concurrency']):>5}  "
            f"{row['avg']:>7.2f}  "
            f"{row['median']:>7.2f}  "
            f"{row['p95']:>7.2f}  "
            f"{row['p99']:>7.2f}  "
            f"{row['req/s']:>7.1f}  "
            f"{row['success_%']:>5.1f}%  "
            f"{bar}"
        )

    print("═" * 80)
    print(f"  URL: {URL}  |  {REQUESTS_PER_LEVEL} req/level  |  warmup: {WARMUP_REQUESTS}")
    print("═" * 80 + "\n")

    # Tabela completa via pandas
    print("── Tabela completa ──────────────────────────────────────────────────────────")
    pd.set_option("display.float_format", "{:.2f}".format)
    pd.set_option("display.max_columns", 20)
    pd.set_option("display.width", 120)
    print(summary.to_string(index=False))
    print()


# ──────────────────────────────────────────────
# Entry point
# ──────────────────────────────────────────────

async def main() -> None:
    print(f"Connecting to {URL} ...")
    print(f"Levels: {CONCURRENCY_LEVELS}  |  {REQUESTS_PER_LEVEL} requests each\n")

    all_results: list[Result] = []
    throughputs: dict[int, float] = {}

    for concurrency in CONCURRENCY_LEVELS:
        print(f"  → {concurrency:>4} concurrent connections ...", end=" ", flush=True)
        results, elapsed = await _run_level(concurrency, REQUESTS_PER_LEVEL)

        ok = sum(1 for r in results if r.success)
        throughputs[concurrency] = ok / elapsed if elapsed > 0 else 0
        all_results.extend(results)

        avg_ms = sum(r.latency_ms for r in results) / len(results) if results else 0
        print(f"avg={avg_ms:.1f}ms  ok={ok}/{len(results)}  rps={throughputs[concurrency]:.1f}")

        await asyncio.sleep(0.3)

    df, summary = analyze(all_results)
    print_report(summary, throughputs)

    # Export CSV
    summary["req/s"] = summary["concurrency"].map(
        lambda c: round(throughputs.get(c, 0), 1)
    )
    out = "stress_results.csv"
    summary.to_csv(out, index=False)
    print(f"Results saved to {out}")

    # Exporta e imprime resumo dos erros
    errors_df = df[~df["success"]]
    if not errors_df.empty:
        error_out = "stress_errors.csv"
        errors_df.to_csv(error_out, index=False)
        print(f"Errors saved to {error_out}")
        
        print("\n── Resumo de Erros ────────────────────────────────────────────────────────")
        error_counts = errors_df.groupby(["concurrency", "status", "error"]).size().reset_index(name="count")
        print(error_counts.to_string(index=False))
    else:
        print("\nNenhum erro ocorreu durante o teste.")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nInterrupted.")
        sys.exit(0)
