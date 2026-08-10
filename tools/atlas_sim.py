#!/usr/bin/env python3
"""
Glyph atlas allocator simulation.

Compares candidate atlas allocators on identical, seeded workloads so the choice
of allocator for GlyphAtlas can be made from measurements rather than intuition.
Backs the atlas decision in docs/amethyst/editor_support.md.

Allocators
  skyline    current Amethyst packer, and Ghostty's (Jylanki skyline). Cannot free.
  shelf      Alacritty's rows: left to right, next row at tallest-in-row. Cannot free.
  blockcell  two level: per (face,size) chunks of a uniform cell grid, free whole groups.

Dropped from the comparison, kept as classes: Quadtree (RAD's four-equal-quadrant
split) measured 183-300% overhead and still dropped glyphs after tens of thousands of
evictions, because freed cells never coalesce for a differently sized glyph. Guillotine
packs tightest but costs 2000+ ops/glyph; BlockCell uses it as its coarse allocator.

Scenarios
  uniform    one face set at one size, the terminal-emulator case
  ui         several faces at fixed sizes 11-16, no retirement
  zoom       ui plus editor faces walking a zoom ladder, retiring the previous size
  editing    fixed size, shifting character repertoire
  mixed      zoom and repertoire churn together
  extreme    6 faces, 40 zoom steps, large repertoire including full-width CJK

Eviction is reactive only: the sole trigger is an allocation that did not fit. There
is no periodic or cap-driven sweep, so idle memory is held rather than reclaimed.
On a miss the order is: reclaim groups untouched for --stale-frames, then commit a
new page, then fail. In-use groups are never evicted - their cells are referenced by
live glyph runs.

Metrics
  ink        glyph ink pixels actually rasterised (identical across allocators)
  reserved   pixels handed out, so reserved/ink - 1 is rounding overhead
  peak live  maximum simultaneously reserved, i.e. what the atlas must hold
  ops        allocator work units: nodes visited, rects scanned, merge comparisons
  evicts     eviction events performed under pressure
  fails      requests that could not be satisfied even after evicting

Usage
  python3 tools/atlas_sim.py
  python3 tools/atlas_sim.py --sims 10 --scenario zoom --atlas 2048
"""

from __future__ import annotations

import argparse
import random
import time
from collections import OrderedDict
from dataclasses import dataclass, field

PAD = 2  # GlyphAtlas packs bitmap+2, see glyph_atlas.cpp

FACES_UI = ["sans", "sans-bold", "sans-italic"]
FACES_MONO = ["mono", "mono-bold", "mono-italic"]
MONO = set(FACES_MONO)

# ------------------------------------------------------------------ glyph model

# Character classes drive the ink box. Widths are ratios of pixel size.
W_RATIO = {"narrow": 0.22, "normal": 0.48, "upper": 0.62, "wide": 0.85, "full": 1.0}
H_RATIO = {"small": 0.18, "x": 0.52, "tall": 0.72, "deep": 0.70, "both": 0.95, "full": 1.0}

ASCII_NARROW = "iljI.,;:'`|!"
ASCII_WIDE = "mwMW@%"
ASCII_TALL = "bdfhklABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789()[]{}|/\\"
ASCII_DEEP = "gjpqy(),;[]{}|/\\"
ASCII_SMALL = ".,'`"

FREQ_TEXT = (
    "etaoinshrdlcumwfgypbvkjxqz"
    "ETAOINSHRDLCUMWFGYPBVKJXQZ"
    "0123456789"
    ".,;:!?'\"()[]{}<>/\\|-_=+*&^%$#@~`"
)
FREQ_WEIGHTS = (
    [12.7, 9.1, 8.2, 7.5, 7.0, 6.7, 6.3, 6.1, 6.0, 4.3, 4.0, 2.8, 2.8, 2.4,
     2.4, 2.2, 2.0, 2.0, 1.9, 1.5, 1.0, 0.8, 0.2, 0.15, 0.1, 0.07]
    + [1.5] * 26
    + [3.0] * 10
    + [4.0] * 32
)


def classify_ascii(ch: str) -> tuple[str, str]:
    if ch in ASCII_NARROW:
        wc = "narrow"
    elif ch in ASCII_WIDE:
        wc = "wide"
    elif ch.isupper():
        wc = "upper"
    else:
        wc = "normal"

    if ch in ASCII_SMALL:
        hc = "small"
    elif ch in ASCII_TALL and ch in ASCII_DEEP:
        hc = "both"
    elif ch in ASCII_TALL:
        hc = "tall"
    elif ch in ASCII_DEEP:
        hc = "deep"
    else:
        hc = "x"
    return wc, hc


def glyph_box(face: str, size: int, gid: int, wc: str, hc: str) -> tuple[int, int]:
    """Ink box, deterministic in (face, size, gid) so every allocator sees the same."""
    jitter = random.Random((hash((face, size, gid)) & 0xFFFFFFFF))
    w_ratio = 0.55 if face in MONO and wc != "full" else W_RATIO[wc]
    if "bold" in face:
        w_ratio *= 1.08
    w = max(1, round(size * w_ratio) + jitter.randint(-1, 1))
    h = max(1, round(size * H_RATIO[hc]) + jitter.randint(-1, 1))
    return w, h


# ------------------------------------------------------------------- allocators


class Allocator:
    supports_free = False
    frees_by_group = False

    def __init__(self, size: int):
        self.size = size
        self.ops = 0

    def alloc(self, w: int, h: int, group=None):
        raise NotImplementedError

    def free(self, handle) -> None:
        raise NotImplementedError

    def footprint(self) -> int:
        """Atlas pixels actually consumed, including space no longer reachable.

        Distinct from summed reserved area: it counts a shelf's row-band slack and
        the gaps under a skyline frontier, which is where fragmentation hides.
        """
        raise NotImplementedError


class Skyline(Allocator):
    """Current Amethyst packer and Ghostty's: best fit at lowest y, no free."""

    def __init__(self, size: int):
        super().__init__(size)
        self.nodes = [(0, 0, size)]

    def _fit(self, idx: int, w: int, h: int):
        x, y, _ = self.nodes[idx]
        if x + w > self.size:
            return None
        left = w
        i = idx
        max_y = y
        while left > 0:
            self.ops += 1
            if i >= len(self.nodes):
                return None
            _, ny, nw = self.nodes[i]
            max_y = max(max_y, ny)
            if max_y + h > self.size:
                return None
            left -= nw
            i += 1
        return max_y

    def alloc(self, w, h, group=None):
        best = None
        for i in range(len(self.nodes)):
            self.ops += 1
            y = self._fit(i, w, h)
            if y is not None and (best is None or y < best[0]):
                best = (y, i)
        if best is None:
            return None
        y, idx = best
        x = self.nodes[idx][0]
        self.nodes.insert(idx, (x, y + h, w))
        i = idx + 1
        while i < len(self.nodes):
            self.ops += 1
            nx, ny, nw = self.nodes[i]
            px, _, pw = self.nodes[i - 1]
            if nx < px + pw:
                shrink = px + pw - nx
                if nw <= shrink:
                    self.nodes.pop(i)
                    continue
                self.nodes[i] = (nx + shrink, ny, nw - shrink)
            break
        merged = []
        for node in self.nodes:
            self.ops += 1
            if merged and merged[-1][1] == node[1]:
                last = merged[-1]
                merged[-1] = (last[0], last[1], last[2] + node[2])
            else:
                merged.append(node)
        self.nodes = merged
        return (x, y, w, h), w * h

    def footprint(self) -> int:
        # Area under the frontier: anything below a node's y is spent, reachable or not.
        return sum(y * width for _, y, width in self.nodes)


class Shelf(Allocator):
    """Alacritty rows: O(1) placement, no free."""

    def __init__(self, size: int):
        super().__init__(size)
        self.x = 0
        self.baseline = 0
        self.tallest = 0

    def alloc(self, w, h, group=None):
        self.ops += 1
        if w > self.size:
            return None
        if self.x + w > self.size:
            self.baseline += self.tallest
            self.x = 0
            self.tallest = 0
            self.ops += 1
        if self.baseline + h > self.size:
            return None
        pos = (self.x, self.baseline, w, h)
        self.x += w
        self.tallest = max(self.tallest, h)
        return pos, w * h

    def footprint(self) -> int:
        # Every row band is spent full width, including slack above shorter glyphs.
        return (self.baseline + self.tallest) * self.size


class QuadNode:
    __slots__ = ("x", "y", "size", "parent", "children", "taken", "free_max")

    def __init__(self, x, y, size, parent=None):
        self.x, self.y, self.size = x, y, size
        self.parent = parent
        self.children = None
        self.taken = False
        self.free_max = size  # largest square this subtree can still provide


class Quadtree(Allocator):
    """RAD: four equal quadrants, square cells, no sibling merge on release.

    Pruning uses a per-node largest-free-square, matching RAD's max_free_size.
    """

    supports_free = True

    def __init__(self, size: int):
        super().__init__(size)
        self.root = QuadNode(0, 0, size)

    def alloc(self, w, h, group=None):
        need = max(w, h)  # cells are square
        node = self._descend(self.root, need)
        if node is None:
            return None
        return node, node.size * node.size

    def _descend(self, node, need):
        self.ops += 1
        if node.free_max < need or node.size < need:
            return None

        child_size = node.size // 2
        if child_size >= need and child_size >= 1:
            if node.taken:
                return None
            if node.children is None:
                node.children = [
                    QuadNode(node.x, node.y, child_size, node),
                    QuadNode(node.x + child_size, node.y, child_size, node),
                    QuadNode(node.x, node.y + child_size, child_size, node),
                    QuadNode(node.x + child_size, node.y + child_size, child_size, node),
                ]
            for c in node.children:
                got = self._descend(c, need)
                if got is not None:
                    return got
            self._recompute(node)
            return None

        if node.taken or node.children is not None:
            return None
        node.taken = True
        node.free_max = 0
        self._propagate(node.parent)
        return node

    def _recompute(self, node):
        if node.children:
            node.free_max = max(c.free_max for c in node.children)
        else:
            node.free_max = 0 if node.taken else node.size

    def _propagate(self, node):
        while node is not None:
            self.ops += 1
            self._recompute(node)
            node = node.parent

    def free(self, handle):
        node = handle
        if not node.taken:
            return
        node.taken = False
        node.free_max = node.size
        self._propagate(node.parent)

    def footprint(self) -> int:
        total = 0
        stack = [self.root]
        while stack:
            n = stack.pop()
            if n.taken:
                total += n.size * n.size
            elif n.children:
                stack.extend(n.children)
        return total


class Guillotine(Allocator):
    """Split at the inserted rect's edges; free with a pairwise merge pass."""

    supports_free = True

    def __init__(self, size: int):
        super().__init__(size)
        self.free_rects = [(0, 0, size, size)]
        self.peak_free_list = 1
        self.merge_passes = 0

    def alloc(self, w, h, group=None):
        got = self._place(w, h)
        if got is None:
            # Compact on demand rather than on every free: merging eagerly is
            # O(n^2) per release and dominates everything else.
            self._merge()
            got = self._place(w, h)
        return got

    def _place(self, w, h):
        best_i = None
        best_area = None
        for i, (_, _, rw, rh) in enumerate(self.free_rects):
            self.ops += 1
            if rw >= w and rh >= h:
                area = rw * rh
                if best_area is None or area < best_area:
                    best_area, best_i = area, i
        if best_i is None:
            return None
        x, y, rw, rh = self.free_rects.pop(best_i)
        if rw - w > 0:
            self.free_rects.append((x + w, y, rw - w, h))
        if rh - h > 0:
            self.free_rects.append((x, y + h, rw, rh - h))
        self.peak_free_list = max(self.peak_free_list, len(self.free_rects))
        return (x, y, w, h), w * h

    def free(self, handle):
        self.free_rects.append(handle)
        self.peak_free_list = max(self.peak_free_list, len(self.free_rects))

    def footprint(self) -> int:
        return self.size * self.size - sum(w * h for _, _, w, h in self.free_rects)

    def _merge(self):
        """Coalesce by sweeping rows then columns: O(n log n) per pass.

        Pairwise scanning is O(n^2) per call and dominates the whole simulation
        once the free list reaches the thousands, which it does under eviction.
        """
        self.merge_passes += 1
        for _ in range(8):
            before = len(self.free_rects)
            self._sweep(horizontal=True)
            self._sweep(horizontal=False)
            if len(self.free_rects) == before:
                break

    def _sweep(self, horizontal: bool):
        groups: dict = {}
        for r in self.free_rects:
            self.ops += 1
            x, y, w, h = r
            key = (y, h) if horizontal else (x, w)
            groups.setdefault(key, []).append(r)

        out = []
        for key, rects in groups.items():
            rects.sort(key=(lambda r: r[0]) if horizontal else (lambda r: r[1]))
            current = rects[0]
            for nxt in rects[1:]:
                self.ops += 1
                cx, cy, cw, ch = current
                nx, ny, nw, nh = nxt
                if horizontal and cx + cw == nx:
                    current = (cx, cy, cw + nw, ch)
                elif not horizontal and cy + ch == ny:
                    current = (cx, cy, cw, ch + nh)
                else:
                    out.append(current)
                    current = nxt
            out.append(current)
        self.free_rects = out


class BlockCell(Allocator):
    """Two level: per (face,size) chunks of a uniform cell grid, freed as a group.

    Chunks come from any page's coarse allocator. Growth is explicit via add_page()
    so the caller controls the policy: reclaim stale groups before committing a new
    page, since a page is committed VRAM whether it is full or not.
    """

    supports_free = True
    frees_by_group = True
    CELLS_PER_CHUNK = 32
    CHUNK_COLS = 8
    MAX_PAGES = 8

    def __init__(self, size: int):
        super().__init__(size)
        self.pages = [Guillotine(size)]
        self.groups = {}

    def set_cell(self, group, cw, ch):
        self.groups.setdefault(group, {"cell": (cw, ch), "chunks": []})

    def alloc(self, w, h, group=None):
        entry = self.groups[group]
        cw, ch = entry["cell"]
        for chunk in entry["chunks"]:
            self.ops += 1
            if chunk["free"]:
                chunk["free"].pop()
                return (group, chunk), cw * ch

        rows = self.CELLS_PER_CHUNK // self.CHUNK_COLS
        for page in self.pages:
            self.ops += 1
            got = page.alloc(cw * self.CHUNK_COLS, ch * rows)
            if got is not None:
                rect, area = got
                chunk = {"rect": rect, "area": area, "page": page,
                         "free": list(range(self.CELLS_PER_CHUNK - 1))}
                entry["chunks"].append(chunk)
                return (group, chunk), cw * ch
        return None

    def add_page(self) -> bool:
        if len(self.pages) >= self.MAX_PAGES:
            return False
        self.pages.append(Guillotine(self.size))
        return True

    def free_group(self, group) -> int:
        entry = self.groups.get(group)
        if not entry:
            return 0
        freed = 0
        for chunk in entry["chunks"]:
            self.ops += 1
            chunk["page"].free(chunk["rect"])
            freed += chunk["area"]
        entry["chunks"] = []
        return freed

    @property
    def total_ops(self) -> int:
        return self.ops + sum(p.ops for p in self.pages)

    @property
    def page_count(self) -> int:
        return len(self.pages)

    def footprint(self) -> int:
        return sum(p.footprint() for p in self.pages)


class MultiPage(Allocator):
    """Fixed-size pages of an inner allocator, adding a page on exhaustion.

    What Alacritty and RAD actually do: RAD keeps a linked list of up to 64
    atlases and never frees a glyph region (fnt_atlas_region_release has no call
    sites), Alacritty allocates another atlas when one fills.

    With affinity=True each (font,size) group is pinned to one page, so a batched
    run - single font, single size - never spans pages and keeps one textureId.
    """

    supports_free = False
    MAX_PAGES = 8

    def __init__(self, size: int, inner=Shelf, affinity: bool = False):
        super().__init__(size)
        self.inner_ctor = inner
        self.affinity = affinity
        self.pages = [inner(size)]
        self.group_page: dict = {}

    def alloc(self, w, h, group=None):
        if self.affinity and group is not None:
            page_idx = self.group_page.get(group)
            if page_idx is not None:
                self.ops += 1
                got = self.pages[page_idx].alloc(w, h, group)
                if got is not None:
                    return got
                # Group outgrew its page; move it onto the next one.
            for idx in range(len(self.pages)):
                self.ops += 1
                got = self.pages[idx].alloc(w, h, group)
                if got is not None:
                    self.group_page[group] = idx
                    return got
            if len(self.pages) >= self.MAX_PAGES:
                return None
            self.pages.append(self.inner_ctor(self.size))
            self.group_page[group] = len(self.pages) - 1
            return self.pages[-1].alloc(w, h, group)

        for page in self.pages:
            self.ops += 1
            got = page.alloc(w, h, group)
            if got is not None:
                return got
        if len(self.pages) >= self.MAX_PAGES:
            return None
        page = self.inner_ctor(self.size)
        self.pages.append(page)
        return page.alloc(w, h, group)

    @property
    def total_ops(self) -> int:
        return self.ops + sum(p.ops for p in self.pages)

    @property
    def page_count(self) -> int:
        return len(self.pages)

    def footprint(self) -> int:
        return sum(p.footprint() for p in self.pages)


# Guillotine is kept as a class because BlockCell uses it as its coarse allocator,
# but it is out of the comparison: 2000+ ops/glyph makes it unusable regardless of
# its density.
ALLOCATORS = {
    "skyline": Skyline,
    "shelf": Shelf,
    "blockcell": BlockCell,
    "shelf-pages": lambda size: MultiPage(size, inner=Shelf),
    "skyline-pages": lambda size: MultiPage(size, inner=Skyline),
    "skyline-affin": lambda size: MultiPage(size, inner=Skyline, affinity=True),
}

# -------------------------------------------------------------------- workload


FRAMES_PER_PHASE = 60  # one second at 60fps, so stale thresholds are in real time


@dataclass
class Request:
    face: str
    size: int
    gid: int
    wc: str
    hc: str
    frame: int = 0


@dataclass
class Workload:
    requests: list[Request] = field(default_factory=list)
    retire_at: dict[int, list] = field(default_factory=dict)
    live_cap: int | None = None
    stale_frames: int = 120  # a group unused this long is an eviction candidate
    evict_in_use: bool = False  # last resort only, and unsafe in the real thing


def _ascii_stream(rng: random.Random, n: int) -> list[str]:
    return rng.choices(FREQ_TEXT, weights=FREQ_WEIGHTS, k=n)


def _repertoire(rng: random.Random, kind: str, count: int) -> list[tuple[int, str, str]]:
    """Synthetic non-ASCII glyphs: (gid, width class, height class)."""
    out = []
    for i in range(count):
        gid = hash((kind, i)) & 0xFFFFF
        if kind == "latin1":
            out.append((gid, rng.choice(["normal", "upper"]), rng.choice(["tall", "both"])))
        elif kind == "symbols":
            out.append((gid, rng.choice(["normal", "wide"]), rng.choice(["x", "tall"])))
        else:  # cjk, full width and full height
            out.append((gid, "full", "full"))
    return out


def interleave(wl: Workload, seed: int) -> Workload:
    """Shuffle arrival order within each phase, keeping retirement points intact.

    build_workload emits every glyph of one (face,size) contiguously, which is
    shelf packing's best case: a row then holds one size, so its height is
    uniform. Interleaved arrival is what exposes row-height waste, and it is just
    as realistic - glyphs trickle in across components at different sizes.
    """
    rng = random.Random(seed)
    bounds = sorted(wl.retire_at.keys())
    spans = []
    start = 0
    for b in bounds + [len(wl.requests)]:
        if b > start:
            spans.append((start, b))
        start = b

    out = list(wl.requests)
    for lo, hi in spans:
        chunk = out[lo:hi]
        rng.shuffle(chunk)
        out[lo:hi] = chunk

    return Workload(requests=out, retire_at=wl.retire_at, live_cap=wl.live_cap)


def build_workload(scenario: str, seed: int) -> Workload:
    rng = random.Random(seed)
    wl = Workload()
    text = _ascii_stream(rng, 30000)
    ascii_set = list(dict.fromkeys(text))

    phase = [0]  # frame clock, advanced once per workload phase

    def push(face, size, chars):
        for ch in chars:
            if ch == " ":
                continue
            wc, hc = classify_ascii(ch)
            wl.requests.append(
                Request(face, size, ord(ch), wc, hc, phase[0] * FRAMES_PER_PHASE))

    if scenario == "uniform":
        for face in ["mono", "mono-bold"]:
            push(face, 14, ascii_set)
        return wl

    if scenario == "ui":
        for face in FACES_UI:
            for size in (11, 12, 13, 14, 15, 16):
                push(face, size, ascii_set)
        return wl

    if scenario == "extreme":
        # Long session: 6 faces, 40 zoom steps across the full ladder, and a large
        # repertoire including full-width CJK. Probes monotonic growth without
        # eviction and sets the page ceiling.
        wl.live_cap = 20000
        ladder = [0.6, 0.75, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0]
        base = 14
        faces = FACES_UI + FACES_MONO
        live_size = None
        for step in range(40):
            size = max(6, round(base * ladder[step % len(ladder)]))
            for face in faces:
                push(face, size, ascii_set)
            if step % 2 == 0:
                for face in FACES_MONO:
                    for gid, wc, hc in _repertoire(rng, "cjk", 400):
                        wl.requests.append(
                            Request(face, size, gid, wc, hc,
                                    phase[0] * FRAMES_PER_PHASE))
                for face in FACES_UI:
                    for gid, wc, hc in _repertoire(rng, "latin1", 200):
                        wl.requests.append(
                            Request(face, size, gid, wc, hc,
                                    phase[0] * FRAMES_PER_PHASE))
            if live_size is not None and live_size != size:
                wl.retire_at.setdefault(len(wl.requests), []).extend(
                    (f, live_size) for f in FACES_MONO
                )
            live_size = size
        return wl

    if scenario == "editing":
        # One size, but the repertoire keeps shifting, under a live-glyph cap.
        wl.live_cap = 2400
        for face in FACES_MONO:
            push(face, 14, ascii_set)
        for kind, count in (("latin1", 220), ("symbols", 260), ("cjk", 900)):
            for face in FACES_MONO:
                for gid, wc, hc in _repertoire(rng, kind, count):
                    wl.requests.append(Request(face, 14, gid, wc, hc))
        return wl

    # zoom, and mixed = zoom + repertoire churn
    for face in FACES_UI:
        for size in (11, 12, 13, 14, 15, 16):
            push(face, size, ascii_set)

    ladder = [0.8, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0]
    idx = ladder.index(1.0)
    base = 14
    live_size = None
    steps = 14
    for step in range(steps):
        size = max(6, round(base * ladder[idx]))
        for face in FACES_MONO:
            push(face, size, ascii_set)
        if scenario == "mixed" and step % 3 == 2:
            for face in FACES_MONO:
                for gid, wc, hc in _repertoire(rng, "cjk", 120):
                    wl.requests.append(
                            Request(face, size, gid, wc, hc,
                                    phase[0] * FRAMES_PER_PHASE))
        if live_size is not None and live_size != size:
            wl.retire_at.setdefault(len(wl.requests), []).extend(
                (f, live_size) for f in FACES_MONO
            )
        live_size = size
        idx = max(0, min(len(ladder) - 1, idx + rng.choice([-2, -1, 1, 1, 2])))

    if scenario == "mixed":
        wl.live_cap = 6000
    return wl


# ---------------------------------------------------------------------- runner


@dataclass
class Result:
    ink: int = 0
    reserved: int = 0
    peak_live: int = 0
    live: int = 0
    ops: int = 0
    evicts: int = 0
    fails: int = 0
    glyphs: int = 0
    seconds: float = 0.0
    peak_free_list: int = 0
    peak_footprint: int = 0
    pages: int = 1
    stale_evicts: int = 0
    pages_added: int = 0


def run(name: str, atlas: int, wl: Workload) -> Result:
    alloc = ALLOCATORS[name](atlas)
    res = Result()
    started = time.perf_counter()

    live: OrderedDict = OrderedDict()  # glyph key -> (handle, reserved, group)
    group_glyphs: dict = {}
    group_lru: OrderedDict = OrderedDict()
    group_last_frame: dict = {}

    cell_cache: dict = {}
    for req in wl.requests:
        w, h = glyph_box(req.face, req.size, req.gid, req.wc, req.hc)
        key = (req.face, req.size)
        cw, ch = cell_cache.get(key, (1, 1))
        cell_cache[key] = (max(cw, w + PAD), max(ch, h + PAD))

    def cell_for(face, size):
        # A real implementation reads the face's scaled bounding box; here that is
        # the max over the glyphs this group actually requests.
        return cell_cache[(face, size)]

    def drop_group(group) -> bool:
        if isinstance(alloc, BlockCell):
            freed = alloc.free_group(group)
            if not freed:
                return False
            res.live -= freed
            for gk in group_glyphs.pop(group, ()):
                live.pop(gk, None)
            group_lru.pop(group, None)
            res.evicts += 1
            return True
        keys = group_glyphs.get(group)
        if not keys:
            return False
        for gk in list(keys):
            entry = live.pop(gk, None)
            if entry:
                alloc.free(entry[0])
                res.live -= entry[1]
        group_glyphs[group] = set()
        group_lru.pop(group, None)
        res.evicts += 1
        return True

    def evict_one() -> bool:
        if not alloc.supports_free:
            return False
        if isinstance(alloc, BlockCell):
            for group in list(group_lru):
                if drop_group(group):
                    return True
            return False
        if not live:
            return False
        gk, (handle, reserved, group) = live.popitem(last=False)
        alloc.free(handle)
        res.live -= reserved
        group_glyphs.get(group, set()).discard(gk)
        res.evicts += 1
        return True

    def evict_stale(now: int) -> bool:
        """Reclaim the least recently used group untouched for stale_frames.

        Only stale groups are candidates: a group still referenced by live glyph
        runs cannot be freed without those runs pointing at cells that have been
        handed to another font. In the real thing eviction happens at a frame
        boundary and marks the affected runs dirty so they re-shape.
        """
        if not alloc.supports_free:
            return False
        cutoff = now - wl.stale_frames
        for group in list(group_lru):
            if group_last_frame.get(group, 0) <= cutoff:
                if drop_group(group):
                    res.stale_evicts += 1
                    return True
        return False

    for i, req in enumerate(wl.requests):
        for group in wl.retire_at.get(i, ()):
            if alloc.supports_free:
                drop_group(group)
            else:
                group_glyphs[group] = set()

        group = (req.face, req.size)
        gk = (req.face, req.size, req.gid)
        if gk in live:
            live.move_to_end(gk)
            group_lru[group] = True
            group_lru.move_to_end(group)
            group_last_frame[group] = req.frame
            continue

        w, h = glyph_box(req.face, req.size, req.gid, req.wc, req.hc)
        pw, ph = w + PAD, h + PAD

        if isinstance(alloc, BlockCell):
            cw, ch = cell_for(req.face, req.size)
            alloc.set_cell(group, cw, ch)

        got = alloc.alloc(pw, ph, group=group)

        # Shipping policy: reclaim groups that have gone stale first, since a page
        # is committed VRAM whether it is full or not; commit a new page only when
        # everything live is genuinely in use; evict in-use groups never.
        attempts = 0
        while got is None and attempts < 256:
            attempts += 1
            if evict_stale(req.frame):
                got = alloc.alloc(pw, ph, group=group)
                continue
            if hasattr(alloc, "add_page") and alloc.add_page():
                res.pages_added += 1
                got = alloc.alloc(pw, ph, group=group)
                continue
            if wl.evict_in_use and evict_one():
                got = alloc.alloc(pw, ph, group=group)
                continue
            break

        if got is None:
            res.fails += 1
            continue

        handle, reserved = got
        res.ink += w * h
        res.reserved += reserved
        res.live += reserved
        res.peak_live = max(res.peak_live, res.live)
        res.peak_footprint = max(res.peak_footprint, alloc.footprint())
        res.glyphs += 1
        live[gk] = (handle, reserved, group)
        group_glyphs.setdefault(group, set()).add(gk)
        group_lru[group] = True
        group_lru.move_to_end(group)
        group_last_frame[group] = req.frame

        # No cap-driven or periodic eviction: reclamation is purely reactive, driven
        # only by an allocation that did not fit. Idle memory is held, not swept.

    res.ops = getattr(alloc, "total_ops", alloc.ops)
    res.pages = getattr(alloc, "page_count", 1)
    res.seconds = time.perf_counter() - started
    if isinstance(alloc, Guillotine):
        res.peak_free_list = alloc.peak_free_list
    elif isinstance(alloc, BlockCell):
        res.peak_free_list = max(p.peak_free_list for p in alloc.pages)
    return res


def main():
    global PAD
    ap = argparse.ArgumentParser()
    ap.add_argument("--sims", type=int, default=5)
    ap.add_argument("--atlas", type=int, action="append")
    ap.add_argument("--scenario", action="append")
    ap.add_argument("--pad", type=int, default=PAD,
                    help="glyph padding in px; 0 isolates rounding waste from padding")
    ap.add_argument("--chunk", type=int, default=BlockCell.CELLS_PER_CHUNK,
                    help="blockcell cells per chunk")
    ap.add_argument("--chunk-sweep", action="store_true",
                    help="sweep blockcell chunk sizes instead of comparing allocators")
    ap.add_argument("--interleave", action="store_true",
                    help="shuffle arrival order within each phase; exposes shelf row-height waste")
    ap.add_argument("--max-pages", type=int, default=MultiPage.MAX_PAGES,
                    help="page ceiling for the multi-page allocators")
    ap.add_argument("--stale-frames", type=int, default=120,
                    help="frames a group must go untouched before it may be evicted")
    ap.add_argument("--evict-in-use", action="store_true",
                    help="allow evicting in-use groups as a last resort (unsafe; for comparison)")
    ap.add_argument("--stale-sweep", action="store_true",
                    help="sweep stale-frames for the shipping policy")
    args = ap.parse_args()

    PAD = args.pad
    BlockCell.CELLS_PER_CHUNK = args.chunk
    MultiPage.MAX_PAGES = args.max_pages
    BlockCell.MAX_PAGES = args.max_pages

    atlases = args.atlas or [1024, 2048]
    scenarios = args.scenario or ["uniform", "ui", "zoom", "editing", "mixed"]

    if args.stale_sweep:
        for scenario in scenarios:
            for atlas in atlases:
                print(f"\n=== stale sweep: {scenario} atlas {atlas} "
                      f"max-pages {args.max_pages} ===")
                print(f"{'stale':<8}{'pages':>7}{'vramMB':>8}{'used%':>7}"
                      f"{'raster':>9}{'staleEv':>9}{'fails':>7}")
                for stale in (0, 30, 60, 120, 300, 1200, 10**9):
                    acc = []
                    for s in range(args.sims):
                        wl = build_workload(scenario, seed=1000 + s)
                        wl.stale_frames = stale
                        acc.append(run("blockcell", atlas, wl))
                    n = len(acc)
                    pages = sum(r.pages for r in acc) / n
                    fp = sum(r.peak_footprint for r in acc) / n
                    label = "never" if stale > 10**8 else str(stale)
                    print(f"{label:<8}{pages:>7.1f}"
                          f"{pages * atlas * atlas / 1e6:>8.1f}"
                          f"{fp / (pages * atlas * atlas) * 100:>6.0f}%"
                          f"{sum(r.glyphs for r in acc)/n:>9.0f}"
                          f"{sum(r.stale_evicts for r in acc)/n:>9.0f}"
                          f"{sum(r.fails for r in acc)/n:>7.0f}")
        return

    if args.chunk_sweep:
        for scenario in scenarios:
            for atlas in atlases:
                print(f"\n=== blockcell chunk sweep: {scenario} atlas {atlas} "
                      f"pad {PAD} ===")
                print(f"{'cells':<8}{'reserved':>10}{'over':>7}{'peak live':>11}"
                      f"{'ops/glyph':>11}{'evicts':>8}{'fails':>7}")
                for cells in (8, 16, 32, 64, 128):
                    BlockCell.CELLS_PER_CHUNK = cells
                    BlockCell.CHUNK_COLS = min(8, cells)
                    acc = []
                    for s in range(args.sims):
                        wl = build_workload(scenario, seed=1000 + s)
                        if args.interleave:
                            wl = interleave(wl, seed=2000 + s)
                        acc.append(run("blockcell", atlas, wl))
                    n = len(acc)
                    ink = sum(r.ink for r in acc) / n
                    reserved = sum(r.reserved for r in acc) / n
                    peak = sum(r.peak_live for r in acc) / n
                    ops = sum(r.ops for r in acc) / n
                    glyphs = max(sum(r.glyphs for r in acc) / n, 1)
                    over = (reserved / ink - 1) * 100 if ink else 0.0
                    print(f"{cells:<8}{reserved/1e6:>10.2f}{over:>6.0f}%"
                          f"{peak/1e6:>11.2f}{ops/glyphs:>11.1f}"
                          f"{sum(r.evicts for r in acc)/n:>8.0f}"
                          f"{sum(r.fails for r in acc)/n:>7.0f}")
        return

    for scenario in scenarios:
        for atlas in atlases:
            order = "interleaved" if args.interleave else "grouped"
            print(f"\n=== {scenario}  atlas {atlas}x{atlas} "
                  f"({atlas * atlas / 1e6:.1f}M px)  pad {PAD}  {order} ===")
            print(f"{'allocator':<14}{'glyphs':>7}{'ink':>7}{'resv':>7}{'over':>7}"
                  f"{'footprint':>10}{'used%':>7}{'pages':>7}{'vramMB':>8}"
                  f"{'ops/gl':>8}{'evicts':>8}{'fails':>7}{'ms':>8}")
            for name in ALLOCATORS:
                acc = []
                for s in range(args.sims):
                    wl = build_workload(scenario, seed=1000 + s)
                    acc.append(run(name, atlas, wl))
                n = len(acc)
                glyphs = sum(r.glyphs for r in acc) / n
                ink = sum(r.ink for r in acc) / n
                reserved = sum(r.reserved for r in acc) / n
                peak = sum(r.peak_live for r in acc) / n
                ops = sum(r.ops for r in acc) / n
                evicts = sum(r.evicts for r in acc) / n
                fails = sum(r.fails for r in acc) / n
                ms = sum(r.seconds for r in acc) / n * 1000
                fp = sum(r.peak_footprint for r in acc) / n
                pages = sum(r.pages for r in acc) / n
                over = (reserved / ink - 1) * 100 if ink else 0.0
                vram = pages * atlas * atlas / 1e6  # R8, so 1 byte per pixel
                used = fp / (pages * atlas * atlas) * 100 if pages else 0.0
                print(f"{name:<14}{glyphs:>7.0f}{ink/1e6:>7.2f}{reserved/1e6:>7.2f}"
                      f"{over:>6.0f}%{fp/1e6:>10.2f}{used:>6.0f}%{pages:>7.1f}"
                      f"{vram:>8.1f}{ops/max(glyphs,1):>8.1f}"
                      f"{evicts:>8.0f}{fails:>7.0f}{ms:>8.1f}")


if __name__ == "__main__":
    main()
