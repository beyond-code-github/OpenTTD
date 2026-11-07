"""Terse one-line-per-item output."""
import json


def compact(obj):
    return json.dumps(obj, separators=(",", ":"), sort_keys=True)


def vehicle_line(v):
    return "%d @%d,%d %d km/h %s" % (v["id"], v["x"], v["y"], v["speed"], v["state"])


def vehicles(reply):
    return [vehicle_line(v) for v in reply.get("vehicles", [])]


def watch_summary(vid, samples):
    sp = [s[0] for s in samples]
    if not sp:
        return "vehicle %d: no samples" % vid
    stopped = any(s == 0 for s in sp)
    return "vehicle %d: n=%d speed min=%d max=%d avg=%.1f km/h stopped=%s" % (
        vid, len(sp), min(sp), max(sp), sum(sp) / len(sp), "yes" if stopped else "no")
