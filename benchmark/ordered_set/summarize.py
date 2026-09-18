#!/usr/bin/env python3
"""Print median CPU ns/op from Google Benchmark JSON; reject failed runs."""

import argparse
import json
import statistics
from collections import defaultdict


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("json_file")
    args = parser.parse_args()
    with open(args.json_file, encoding="utf-8") as stream:
        report = json.load(stream)
    samples = defaultdict(list)
    medians = {}
    units = {"ns": 1, "us": 1_000, "ms": 1_000_000, "s": 1_000_000_000}
    for row in report["benchmarks"]:
        if row.get("error_occurred") or row.get("skipped"):
            raise SystemExit(f"Invalid benchmark: {row['name']}: {row.get('error_message', row.get('skipped'))}")
        name = row.get("run_name", row["name"])
        parts = name.split("/")
        if len(parts) != 3:
            raise SystemExit(f"Unexpected benchmark name: {name}")
        container, operation, size = parts
        key = (int(size), operation, container)
        if row.get("run_type") == "aggregate":
            if row.get("aggregate_name") == "median":
                medians[key] = row["cpu_time"] * units[row["time_unit"]] / int(size)
        else:
            samples[key].append(row["cpu_time"] * units[row["time_unit"]] / int(size))
    values = {key: statistics.median(v) for key, v in samples.items()}
    values.update(medians)
    if not values:
        raise SystemExit("No benchmark results")
    containers = sorted({key[2] for key in values})
    print("CPU ns/op；各次重复的中位数，数值越小越好。\n")
    print("| N | 操作 | " + " | ".join(containers) + " |")
    print("| ---: | --- | " + " | ".join("---:" for _ in containers) + " |")
    order = {"insert": 0, "find_hit": 1, "find_miss": 2, "find_mixed": 3, "erase": 4}
    pairs = sorted({key[:2] for key in values}, key=lambda k: (k[0], order[k[1]]))
    for n, operation in pairs:
        cells = [f"{values[(n, operation, c)]:.2f}" if (n, operation, c) in values else "—"
                 for c in containers]
        print(f"| {n:,} | {operation} | " + " | ".join(cells) + " |")


if __name__ == "__main__":
    main()
