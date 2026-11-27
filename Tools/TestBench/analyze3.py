"""Reduces a scripted session log to the four tells that make a companion read as a machine.

Reads the csv rows the evaluator writes behind Viper.Companion.Csv and the script | STEP
markers, and reports per step plus a total.  Usage: py -I analyze3.py <log> [<log2> ...]
Columns come from the csvhead row in the log itself, so this needs no agreement with the C++.
"""
import io
import math
import os
import re
import sys

# The evaluator writes a csvhead row, but the tree can start before the console variable is
# set, and then the rows arrive with no header.  Fall back on the order the C++ writes.
FALLBACK_COLS = "t,lx,ly,lyaw,lspd,cx,cy,cyaw,cspd,dist,aimerr,aimthr,eye,intent,mode".split(",")
FALLBACK_COLS_19 = FALLBACK_COLS + ["gx", "gy", "sdx", "sdy"]

STEP_RE = re.compile(r"script \| STEP (\d+) \[(\w+)\] target=\[([^\]]*)\] cap=([\d.]+)s")
CSV_RE = re.compile(r"(?:^|\s)csv,([-0-9.,]+)\s*$")
HEAD_RE = re.compile(r"(?:^|\s)csvhead,(.+?)\s*$")
EVENT_RE = re.compile(r"(follow|idlebreak|wander|lookat) \| (ENTER|PARKED|ARRIVED)([^\n]*)")


def angle_delta(a, b):
    d = (b - a) % 360.0
    if d > 180.0:
        d -= 360.0
    return d


def read(path):
    cols, rows, steps, events = None, [], [], []
    with io.open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            h = HEAD_RE.search(line)
            if h and cols is None:
                cols = h.group(1).split(",")
                continue
            m = CSV_RE.search(line)
            if m:
                parts = m.group(1).split(",")
                if cols is None:
                    if len(parts) == len(FALLBACK_COLS_19):
                        cols = list(FALLBACK_COLS_19)
                    elif len(parts) == len(FALLBACK_COLS):
                        cols = list(FALLBACK_COLS)
                if cols and len(parts) >= len(cols):
                    try:
                        rows.append({c: float(parts[i]) for i, c in enumerate(cols)})
                    except ValueError:
                        pass
                continue
            s = STEP_RE.search(line)
            if s:
                steps.append({"n": int(s.group(1)), "action": s.group(2), "target": s.group(3),
                              "t": rows[-1]["t"] if rows else 0.0})
                continue
            e = EVENT_RE.search(line)
            if e:
                events.append((rows[-1]["t"] if rows else 0.0, e.group(1), e.group(2), e.group(3).strip()))
    return cols, rows, steps, events


def tells(rows):
    """The same four numbers the browser tuner shows, computed the same way."""
    out = {"n": len(rows)}
    if len(rows) < 3:
        return out

    gaps = [r["dist"] for r in rows]
    out["gap_min"], out["gap_max"] = min(gaps), max(gaps)
    out["gap_mean"] = sum(gaps) / len(gaps)

    orbit = 0.0           # arc swept around the leader while the leader stands still
    peak_swing = 0.0      # fastest swing of the companion heading
    pace_flips = 0        # how often the companion speed changes its mind
    walked = 0.0
    standing = 0.0
    last_sign = 0
    # A count of flips says nothing without their size: a pace that wobbles by 5cm/s is
    # invisible, one that wobbles by 80 is the surging everybody notices.  Same for yaw:
    # one frame at the turn cap is nothing, a tenth of the walk spent there is snatching.
    flip_amp = []
    busy_yaw = 0.0
    moving_time = 0.0
    last_extreme = rows[0]["cspd"]
    # 0 standing, 1 steering straight at the slot, 2 following a path.  Flipping between 1
    # and 2 cancels one with the other every time, so the count matters on its own.
    mode_flips = 0
    mode_time = {0: 0.0, 1: 0.0, 2: 0.0}
    for a, b in zip(rows, rows[1:]):
        dt = b["t"] - a["t"]
        if dt <= 1e-4 or dt > 0.5:
            continue
        walked += math.hypot(b["cx"] - a["cx"], b["cy"] - a["cy"])
        if b["lspd"] <= 20.0:
            standing += dt
            ba = math.degrees(math.atan2(a["cy"] - a["ly"], a["cx"] - a["lx"]))
            bb = math.degrees(math.atan2(b["cy"] - b["ly"], b["cx"] - b["lx"]))
            orbit += abs(angle_delta(ba, bb))
        swing = abs(angle_delta(a["cyaw"], b["cyaw"])) / dt
        peak_swing = max(peak_swing, swing)
        if b["cspd"] > 25.0:
            moving_time += dt
            if swing > 200.0:
                busy_yaw += dt
        sign = (b["cspd"] > a["cspd"] + 2) - (b["cspd"] < a["cspd"] - 2)
        if sign and last_sign and sign != last_sign:
            pace_flips += 1
            flip_amp.append(abs(a["cspd"] - last_extreme))
            last_extreme = a["cspd"]
        if sign:
            last_sign = sign
        ma, mb = int(round(a["mode"])), int(round(b["mode"]))
        mode_time[mb] = mode_time.get(mb, 0.0) + dt
        if ma != mb:
            mode_flips += 1

    span = rows[-1]["t"] - rows[0]["t"]
    out["orbit"] = orbit
    out["standing"] = standing
    out["peak_swing"] = peak_swing
    out["pace_per10"] = pace_flips * 10.0 / span if span > 0 else 0.0
    out["pace_amp"] = sum(flip_amp) / len(flip_amp) if flip_amp else 0.0
    out["yaw_busy"] = busy_yaw / moving_time if moving_time > 0.1 else 0.0
    out["mode_per10"] = mode_flips * 10.0 / span if span > 0 else 0.0
    tot = sum(mode_time.values()) or 1.0
    out["mode_mix"] = "stand %.0f%% steer %.0f%% path %.0f%%" % (
        100 * mode_time.get(0, 0) / tot, 100 * mode_time.get(1, 0) / tot, 100 * mode_time.get(2, 0) / tot)
    out["walked"] = walked
    out["span"] = span
    out["eye_frac"] = sum(1 for r in rows if r["eye"] > 0.5) / len(rows)
    out["aim_min"] = min(r["aimerr"] for r in rows)
    crowd = sum(1 for r in rows if r["dist"] < 150.0)
    out["crowd_frac"] = crowd / len(rows)
    return out


def fmt(label, t):
    if t.get("n", 0) < 3:
        return "    (too few samples)"
    return (
        "    gap {gap_min:4.0f}..{gap_max:4.0f} mean {gap_mean:4.0f}   orbit {orbit:5.0f}deg"
        " over {standing:4.1f}s standing\n"
        "    swing peak {peak_swing:5.0f}deg/s  above 200 for {yaw_busy:4.0%} of the walk\n"
        "    pace flips {pace_per10:4.1f}/10s by {pace_amp:4.0f}cm/s"
        "   walked {walked:5.0f}   too close {crowd_frac:4.0%}\n"
        "    mode {mode_mix}, changing {mode_per10:5.1f}/10s\n"
        "    aim best {aim_min:5.1f}deg   eye contact {eye_frac:4.0%} of samples"
    ).format(**t)


def brief(paths):
    """One line per log, so several runs of the same build can be compared at a glance.
    Single runs differ a lot in the tail - the scenario has random jitter, random wander points
    and its own frame timing - so a change is only real when it shows across repeats."""
    print("%-14s %6s %6s %6s %6s %7s %6s %6s %6s %6s" % (
        "log", "gapavg", "gapmax", "orbit", "swing%", "pace/10s", "path%", "close%", "jam", "walked"))
    for path in paths:
        cols, rows, steps, events = read(path)
        t = tells(rows) if rows else {"n": 0}
        # A run that was cut short leaves a handful of rows, and tells() then returns the count
        # alone.  Say so instead of raising: a batch of six must not be lost to one short log.
        if "gap_mean" not in t:
            print("%-14s  only %d csv rows - run cut short?" % (os.path.basename(path)[:14], t.get("n", 0)))
            continue
        jam = sum(1 for r in rows if int(round(r["mode"])) == 1 and r["cspd"] < 10 and r["intent"] > 0.5)
        pathfrac = 0.0
        m = re.search(r"path (\d+)%", t["mode_mix"])
        if m:
            pathfrac = float(m.group(1))
        print("%-14s %6.0f %6.0f %6.0f %5.0f%% %8.1f %5.0f%% %5.0f%% %6d %6.0f" % (
            os.path.basename(path)[:14], t["gap_mean"], t["gap_max"], t["orbit"],
            100 * t["yaw_busy"], t["pace_per10"], pathfrac, 100 * t["crowd_frac"], jam, t["walked"]))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if "--brief" in sys.argv:
        brief(args)
        return
    for path in args:
        cols, rows, steps, events = read(path)
        print("=" * 78)
        print(path)
        if not cols:
            print("  no csvhead row - was Viper.Companion.Csv on?")
            continue
        if not rows:
            print("  csvhead found but no csv rows")
            continue
        print("  %d samples over %.1fs, %d steps" % (len(rows), rows[-1]["t"] - rows[0]["t"], len(steps)))
        print("-" * 78)
        print("WHOLE RUN")
        print(fmt("all", tells(rows)))
        bounds = [(s["t"], s) for s in steps]
        for i, (t0, s) in enumerate(bounds):
            t1 = bounds[i + 1][0] if i + 1 < len(bounds) else rows[-1]["t"] + 1
            seg = [r for r in rows if t0 <= r["t"] < t1]
            print("-" * 78)
            print("STEP %d [%s] %s   %.1fs" % (s["n"], s["action"], s["target"], t1 - t0))
            print(fmt("step", tells(seg)))
            for (et, who, what, rest) in events:
                if t0 <= et < t1:
                    print("    > %s %s %s" % (who, what, rest[:90]))


if __name__ == "__main__":
    main()
