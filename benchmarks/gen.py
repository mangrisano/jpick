#!/usr/bin/env python3
"""Generate a deterministic JSON array used by the jpick benchmark."""

import json
import sys

count = int(sys.argv[1]) if len(sys.argv) > 1 else 100_000
out_path = sys.argv[2] if len(sys.argv) > 2 else "big.json"

items = [
    {
        "id": i,
        "name": f"item-{i}",
        "value": i * 2,
        "active": i % 2 == 0,
        "tags": ["alpha", "beta", "gamma"],
    }
    for i in range(count)
]

with open(out_path, "w", encoding="utf-8") as handle:
    json.dump(items, handle)
