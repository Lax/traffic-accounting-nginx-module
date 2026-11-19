#!/usr/bin/env python3
"""Parse traffic-accounting exported log lines.

Line shape (HTTP):
  2026/11/19 13:44:59 [notice] 123#0: pid:123|from:1|to:2|accounting_id:NAME|\
requests:5|bytes_in:10|bytes_out:20|latency_ms:0|upstream_latency_ms:0|200:5

Commands:
  lines                     all parsed lines as JSON
  ids                       sorted unique accounting ids
  sum [--id ID] --field F   sum of a field (requests/bytes_in/...) [over one id]
  count --id ID             number of exported lines for id
  status --id ID --code C   total count of status C for id
  spread                    per (pid,from) export batches: wall-clock seconds
                            between first and last line written in that batch
                            (detects event-loop stalls during export)
  raw                       raw lines containing accounting_id: (byte-faithful)
"""
import json
import re
import sys
from datetime import datetime

LINE_RE = re.compile(
    r"^(?P<ts>\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}) \[\w+\] \d+#\d+: (?P<body>.*)$"
)


def parse_line(raw: str):
    m = LINE_RE.match(raw.rstrip("\n"))
    if not m or "accounting_id:" not in m.group("body"):
        return None
    d = {
        "ts": m.group("ts"),
        "line": m.group("body"),
        "raw": raw.rstrip("\n"),
    }
    parts = m.group("body").split("|")
    statuses = {}
    for kv in parts:
        k, _, v = kv.partition(":")
        if k == "accounting_id":
            d["id"] = v
        elif k.isdigit():
            statuses[int(k)] = int(v) if v.isdigit() else 0
        else:
            try:
                d[k] = int(v)
            except ValueError:
                d[k] = v
    d["statuses"] = statuses
    return d


def load(path):
    out = []
    with open(path, "rb") as f:
        for raw in f:
            try:
                p = parse_line(raw.decode("utf-8", "replace"))
            except Exception:
                p = None
            if p:
                out.append(p)
    return out


def main():
    path, cmd = sys.argv[1], sys.argv[2]
    args = sys.argv[3:]
    opts = {}
    i = 0
    while i < len(args):
        if args[i].startswith("--"):
            opts[args[i][2:]] = args[i + 1]
            i += 2
        else:
            i += 1

    lines = load(path)

    if cmd == "lines":
        print(json.dumps(lines, indent=1))
    elif cmd == "ids":
        for x in sorted({l["id"] for l in lines}):
            print(x)
    elif cmd == "raw":
        for l in lines:
            print(l["raw"])
    elif cmd == "sum":
        field = opts["field"]
        sel = [l for l in lines if not opts.get("id") or l["id"] == opts["id"]]
        print(sum(l.get(field, 0) for l in sel))
    elif cmd == "count":
        print(sum(1 for l in lines if l["id"] == opts["id"]))
    elif cmd == "status":
        code = int(opts["code"])
        print(sum(l["statuses"].get(code, 0) for l in lines if l["id"] == opts["id"]))
    elif cmd == "spread":
        batches = {}
        for l in lines:
            key = (l.get("pid"), l.get("from"))
            batches.setdefault(key, []).append(
                datetime.strptime(l["ts"], "%Y/%m/%d %H:%M:%S")
            )
        worst = 0.0
        for key, tss in sorted(batches.items(), key=lambda kv: str(kv[0])):
            lo, hi = min(tss), max(tss)
            span = (hi - lo).total_seconds()
            worst = max(worst, span)
            print(f"pid={key[0]} from={key[1]} lines={len(tss)} spread_s={span:.0f}")
        print(f"worst_s={worst:.0f}")
    else:
        print(f"unknown command: {cmd}", file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
