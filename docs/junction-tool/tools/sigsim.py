"""Faithful-ish port of OpenTTD signal.cpp (ExploreSegment / UpdateSignalsAroundSegment) for a junction layout JSON.

Occupancy model: a set of (tile, track) pairs; a bridge is occupied via its head tiles (track 'X'/'Y').
Signal states are iterated to a fixpoint (what OpenTTD's _globset propagation converges to).
"""
import json, sys

NE, SE, SW, NW = 0, 1, 2, 3
OFF = {NE: (-1, 0), SE: (0, 1), SW: (1, 0), NW: (0, -1)}
REV = {NE: SW, SW: NE, SE: NW, NW: SE}
# track -> the two edges it touches
EDGES = {'X': (NE, SW), 'Y': (NW, SE), 'Upper': (NW, NE), 'Lower': (SE, SW), 'Left': (NW, SW), 'Right': (NE, SE)}
TD = {  # trackdir name -> (track, exit edge)
    'X_NE': ('X', NE), 'X_SW': ('X', SW), 'Y_SE': ('Y', SE), 'Y_NW': ('Y', NW),
    'Upper_E': ('Upper', NE), 'Upper_W': ('Upper', NW), 'Lower_E': ('Lower', SE), 'Lower_W': ('Lower', SW),
    'Left_S': ('Left', SW), 'Left_N': ('Left', NW), 'Right_S': ('Right', SE), 'Right_N': ('Right', NE),
}
TDNAME = {v: k for k, v in TD.items()}


def other_edge(track, e):
    a, b = EDGES[track]
    return b if e == a else a


class Layout:
    def __init__(self, path):
        d = json.load(open(path))
        self.tracks = {(t['x'], t['y']): set(t['bits']) for t in d['tracks']}
        self.bridge = {}  # head tile -> (other head, dir into bridge, axis track)
        for b in d['bridges']:
            s, e = tuple(b['start']), tuple(b['end'])
            if s[1] == e[1]:
                ds, de, axis = (SW, NE, 'X') if e[0] > s[0] else (NE, SW, 'X')
            else:
                ds, de, axis = (SE, NW, 'Y') if e[1] > s[1] else (NW, SE, 'Y')
            self.bridge[s] = (e, ds, axis)
            self.bridge[e] = (s, de, axis)
        self.sig = {}  # (tile, track) -> {exitedge: type}
        for s in d['signals']:
            track, ex = TD[s['facing']]
            self.sig.setdefault(((s['x'], s['y']), track), {})[ex] = s['type']
        self.state = {}  # (tile, track, exitedge) -> 'G'/'R'
        for (tile, track), m in self.sig.items():
            for ex in m:
                self.state[(tile, track, ex)] = 'G'

    def has_signals(self, tile):
        return any((tile, t) in self.sig for t in self.tracks.get(tile, ()))

    def explore(self, starts, occ):
        """ExploreSegment. starts: list of (tile, enterdir or None). Returns (flags set, signals around [(tile,track,exit)])."""
        flags = set()
        around = []
        todo = list(starts)
        todoset = set(starts)
        done = set()
        occ_tiles = {t for t, _ in occ}

        def add(t1, d1, t2, d2):
            # MaybeAddToTodoSet: if (t2,d2) is pending, drop it and do not add (t1,d1)
            if (t2, d2) in todoset:
                todoset.discard((t2, d2))
                return
            if (t1, d1) in done or (t1, d1) in todoset:
                return
            todo.append((t1, d1)); todoset.add((t1, d1))

        while todo:
            tile, enterdir = todo.pop()
            if (tile, enterdir) not in todoset:
                continue
            todoset.discard((tile, enterdir)); done.add((tile, enterdir))
            if tile in self.bridge:
                other, d, axis = self.bridge[tile]
                if enterdir is None:  # from the wormhole
                    if tile in occ_tiles: flags.add('Train')
                    exitdir = REV[d]
                    nt = (tile[0] + OFF[exitdir][0], tile[1] + OFF[exitdir][1])
                    add(nt, REV[exitdir], tile, exitdir)
                else:
                    if REV[enterdir] != d: continue
                    if tile in occ_tiles: flags.add('Train')
                    add(other, None, tile, None)
                continue
            if tile not in self.tracks:
                continue
            tracks = self.tracks[tile]
            masked = {t for t in tracks if enterdir in EDGES[t]}
            if tracks in ({'Upper', 'Lower'}, {'Left', 'Right'}):
                tracks = masked
                if any((tile, t) in occ for t in tracks): flags.add('Train')
            else:
                if not masked: continue
                if tile in occ_tiles: flags.add('Train')
            if len(tracks) > 1: flags.add('Split')
            if self.has_signals(tile):
                (track,) = masked
                if (tile, track) in self.sig:
                    m = self.sig[(tile, track)]
                    ex = other_edge(track, enterdir)  # exploration trackdir exits here
                    rev_ex = enterdir  # reverse trackdir exits at enter edge
                    if rev_ex in m:
                        flags.add('MultiEnter' if 'Enter' in flags else 'Enter')
                        around.append((tile, track, rev_ex))
                    if ex in m and m[ex] in ('exit', 'combo'):
                        if 'Exit' in flags: flags.add('MultiExit')
                        flags.add('Exit')
                        if self.state[(tile, track, ex)] == 'G':
                            if 'Green' in flags: flags.add('MultiGreen')
                            flags.add('Green')
                    continue
            for dd in (NE, SE, SW, NW):
                if dd != enterdir and any(dd in EDGES[t] for t in tracks):
                    nt = (tile[0] + OFF[dd][0], tile[1] + OFF[dd][1])
                    add(nt, REV[dd], tile, dd)
        return flags, around

    def segment_ahead(self, tile, track, ex, occ):
        nt = (tile[0] + OFF[ex][0], tile[1] + OFF[ex][1])
        return self.explore([(tile, ex), (nt, REV[ex])], occ)

    def new_state(self, tile, track, ex, flags):
        typ = self.sig[(tile, track)][ex]
        if 'Train' in flags: return 'R'
        if typ in ('entry', 'combo') and 'Exit' in flags and 'Green' not in flags: return 'R'
        return 'G'

    def settle(self, occ):
        for k in self.state: self.state[k] = 'G'
        for _ in range(200):
            changed = False
            for (tile, track, ex) in list(self.state):
                flags, _ = self.segment_ahead(tile, track, ex, occ)
                ns = self.new_state(tile, track, ex, flags)
                if ns != self.state[(tile, track, ex)]:
                    self.state[(tile, track, ex)] = ns; changed = True
            if not changed: return
        raise RuntimeError('no fixpoint')

    def segment_tiles(self, tile, track, ex):
        """Tiles a train can be detected on in the segment ahead of this signal (by probing each tile)."""
        res = []
        for t, trs in self.tracks.items():
            for tr in trs:
                f, _ = self.segment_ahead(tile, track, ex, {(t, tr)})
                if 'Train' in f: res.append((t, tr))
        for t, (o, d, axis) in self.bridge.items():
            f, _ = self.segment_ahead(tile, track, ex, {(t, axis)})
            if 'Train' in f: res.append((t, axis))
        return res
