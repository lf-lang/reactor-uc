#!/usr/bin/env python3
"""Turn two scaling reports into the markdown a PR comment shows.

    compare-scaling.py base.tsv head.tsv > scaling_results.md

The comment reports what changed and never fails the build..
"""
import csv
import sys
from collections.abc import Iterable

# One report, keyed by program name.
Report = dict[str, dict[str, str]]
# Per-program percentages
Changes = list[tuple[str, float | None]]

# The marker report.sh writes for a program it could not measure.
FAILED = "failed"

# Below this, a difference is the compiler laying the same work out slightly
# differently rather than the runtime doing more of it.
NOISE_PCT = 0.5

# Above this, the growth is called out in the opening line instead of being left
# for the reader to spot in the table.
NOTABLE_PCT = 5.0


def load(path: str) -> Report:
    """Read one TSV report, keyed by program name."""
    with open(path) as f:
        return {row["program"]: row for row in csv.DictReader(f, delimiter="\t")}


def measured(row: dict[str, str] | None) -> bool:
    """Did this branch get numbers out of the program at all?"""
    return row is not None and row["footprint"] != FAILED


def pct(old: float, new: float) -> float | None:
    """Percentage change, or None when there is no baseline to divide by."""
    return None if old == 0 else (new - old) / old * 100.0


def fmt_pct(change: float | None) -> str:
    """A percentage for a table cell, or a placeholder when it would mislead."""
    if change is None:
        return "n/a"
    if abs(change) < NOISE_PCT:
        return "—"
    return "%+.0f%%" % change


def programs_by_size(report: Report, names: Iterable[str]) -> list[str]:
    """Order program names by reaction count, so the table climbs as the benchmark does."""
    return sorted(names, key=lambda k: int(report[k]["reactions"]))


def summarize(label: str, changes: Changes) -> str:
    """One clause saying what happened to a metric across every program.

    Reports the size at which this PR costs the most rather than
    an average, which would hide it behind the sizes where nothing happened.
    """
    comparable = [(name, c) for name, c in changes if c is not None]
    if not comparable:
        return "%s could not be compared" % label

    moved = [(name, c) for name, c in comparable if abs(c) >= NOISE_PCT]
    if not moved:
        return "%s is unchanged" % label

    best = min(moved, key=lambda pair: pair[1])
    worst = max(moved, key=lambda pair: pair[1])

    if worst[1] > 0 and best[1] < 0:
        return "%s goes both ways, from %+.0f%% on `%s` to %+.0f%% on `%s`" % (
            label, best[1], best[0], worst[1], worst[0])
    if worst[1] > 0:
        return "%s grows by up to %.0f%% on `%s`" % (label, worst[1], worst[0])
    return "%s drops by up to %.0f%% on `%s`" % (label, abs(best[1]), best[0])


def verdict(footprints: Changes, instructions: Changes) -> str:
    """The opening line: what a reviewer needs to know without opening the table."""
    quiet = all(c is not None and abs(c) < NOISE_PCT
                for _, c in footprints + instructions)
    if quiet:
        return "neither footprint nor instruction count moved"
    return "%s, and %s" % (summarize("footprint", footprints),
                           summarize("instruction count", instructions))


def sentence(text: str) -> str:
    """Capitalise and close a clause so it can stand on its own."""
    return text[0].upper() + text[1:] + "."


def name_list(names: Iterable[str]) -> str:
    """Program names as inline code, comma separated."""
    return ", ".join("`%s`" % n for n in names)


def classify(base: Report, head: Report) -> tuple[list[str], list[str], list[str], list[str]]:
    """Sort every program either branch reported into what can be said about it.

    A program that builds on one branch but not the other is the runtime changing
    underneath it, which is exactly what this comment exists to notice. 
    Returns (compared, new, broken, gone), each ordered by size.
    """
    everything = {**base, **head}

    compared, new, broken, gone = [], [], [], []
    for name in programs_by_size(everything, everything):
        b, h = base.get(name), head.get(name)
        if h is None:
            gone.append(name)
        elif not measured(h):
            broken.append(name)
        elif measured(b):
            compared.append(name)
        else:
            new.append(name)
    return compared, new, broken, gone


def main(base_path: str, head_path: str) -> None:
    base, head = load(base_path), load(head_path)
    compared, new, broken, gone = classify(base, head)

    footprints: Changes = [(k, pct(int(base[k]["footprint"]), int(head[k]["footprint"])))
                           for k in compared]
    instructions: Changes = [(k, pct(int(base[k]["ir"]), int(head[k]["ir"])))
                             for k in compared]

    # A build that broke outranks any percentage, so it leads.
    regressed = [k for k in broken if measured(base.get(k))]
    still_broken = [k for k in broken if not measured(base.get(k))]

    headline = []
    if regressed:
        headline.append("%s no longer builds on this branch, but builds on the base "
                        "branch." % name_list(regressed))
    if new:
        headline.append("%s builds here but not on the base branch." % name_list(new))
    if compared:
        headline.append(sentence(verdict(footprints, instructions)))
    elif not regressed:
        headline.append("No program was measured on both branches, so there is "
                        "nothing to compare yet.")

    out = ["## Scaling benchmarks", "", " ".join(headline)]

    if compared or new:
        out += ["",
                "| program | reactions | footprint | Δ | instructions | Δ |",
                "|---|---:|---:|---:|---:|---:|"]
        for (name, d_footprint), (_, d_ir) in zip(footprints, instructions):
            out.append("| %s | %s | %s → %s | **%s** | %s → %s | **%s** |" % (
                name, head[name]["reactions"],
                f"{int(base[name]['footprint']):,}", f"{int(head[name]['footprint']):,}",
                fmt_pct(d_footprint),
                f"{int(base[name]['ir']):,}", f"{int(head[name]['ir']):,}",
                fmt_pct(d_ir)))
        # No baseline to subtract from, but the numbers still belong in the table:
        # they are what the next PR will be compared against.
        for name in new:
            out.append("| %s | %s | %s | **new** | %s | **new** |" % (
                name, head[name]["reactions"],
                f"{int(head[name]['footprint']):,}", f"{int(head[name]['ir']):,}"))

    if still_broken:
        out += ["", "%s builds on neither branch." % name_list(still_broken)]
    if gone:
        out += ["", "Measured on the base branch but absent here: %s." % name_list(gone)]

    # `default` matters: a report of nothing but zero baselines leaves no
    # percentage to take a maximum of, and a crash here costs the whole comment.
    growth = max((c for _, c in footprints + instructions if c is not None), default=0.0)
    if regressed:
        out += ["", "> A program that stopped building is worth more attention than "
                    "the table above. The build log is in the workflow run."]
    elif growth >= NOTABLE_PCT:
        out += ["", "> Worth a look before merging, if that growth was not the point "
                    "of the change."]

    out += ["", "<sub>Programs come from `benchmarks/scaling/gen.py`. Footprint is the "
                "sum of every ALLOC section, instructions are counted by callgrind.</sub>"]
    print("\n".join(out))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
