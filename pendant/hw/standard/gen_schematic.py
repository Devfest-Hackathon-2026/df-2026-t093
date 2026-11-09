#!/usr/bin/env python3
"""Generate the Standard board schematic (standard.kicad_sch) from parts.py (nets) and layout.py (placement).

Drawing rules, like a hand-drawn sheet:
- nets in parts.POWER become power symbols (GND, or a VCC-style symbol named after the rail);
- other nets are wired inside a block (parts in the same `group`); a net that leaves its block gets a
  net label at one pin per block;
- unused pins get no-connect flags.
Wires are routed as a stub out of each pin and then an L, trying stub lengths until the path touches
no other net and crosses no part body. The result is checked against parts.py through kicad-cli's
netlist, so a routing short fails the run instead of reaching the drawing.

  ~/.venvs/kicad/bin/python hw/standard/gen_schematic.py

ponytail: greedy router with a fixed set of candidate paths. If a net cannot be routed, the run stops
and names it; move that part in layout.py.
"""
import math, os, subprocess, sys, tempfile, uuid
import xml.etree.ElementTree as ET
import sexpdata as S

HERE = os.path.dirname(os.path.abspath(__file__))
KLIB = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols"
G = 2.54
NS = uuid.UUID("6c696665-4f53-5065-6e64-616e74000001")  # stable UUIDs: same input, same file

def uid(*k):
    return str(uuid.uuid5(NS, "/".join(map(str, k))))

def q(s):
    return '"' + str(s).replace("\\", "\\\\").replace('"', '\\"') + '"'

def k(v):  # coordinate key at 0.01 mm
    return int(round(v * 100))

# ---------- symbols ----------

_libs = {}

def stock(lib_id):
    """Stock library symbol renamed to lib_id, and its pins {number: (x, y, angle)}."""
    lib, name = lib_id.split(":")
    if lib not in _libs:
        _libs[lib] = S.loads(open(f"{KLIB}/{lib}.kicad_sym", encoding="utf-8").read())
    node = next(n for n in _libs[lib][1:] if isinstance(n, list) and n[0] == S.Symbol("symbol") and n[1] == name)
    node = S.loads(S.dumps(node))
    assert not any(isinstance(n, list) and n[0] == S.Symbol("extends") for n in node), lib_id + " is derived"
    node[1] = lib_id
    return node, pins_of(node)

def pins_of(node):
    out = {}
    def walk(n):
        if not isinstance(n, list) or not n:
            return
        if n[0] == S.Symbol("pin"):
            at = next(x for x in n if isinstance(x, list) and x[0] == S.Symbol("at"))
            num = next(x for x in n if isinstance(x, list) and x[0] == S.Symbol("number"))[1]
            out[str(num)] = (float(at[1]), float(at[2]), int(at[3]) if len(at) > 3 else 0)
        for c in n:
            walk(c)
    walk(node)
    return out

def custom(name, left, right, ref="U"):
    """Box symbol. left/right: [(number, name, type)] top to bottom; None leaves a gap.
    A number like "1 15 31" stacks several pins (same name) on one row."""
    rows = max(len(left), len(right))
    h = math.ceil((rows - 1) / 2) * G  # top pin row; every pin stays on the 2.54 grid
    w = max(7.62, (max([len(p[1]) for p in left + right if p] + [4]) + 2) * 1.27)
    w = math.ceil(w / G) * G
    pins, pos = [], {}
    for side, lst in ((-1, left), (1, right)):
        for i, p in enumerate(lst):
            if not p:
                continue
            nums, nm, typ = p
            x, y, a = side * (w + G), h - G * i, (0 if side < 0 else 180)
            for num in nums.split():
                pos[num] = (x, y, a)
            num = nums if " " not in nums else "[" + ",".join(nums.split()) + "]"  # KiCad stacked-pin notation
            pins.append(f'(pin {typ} line (at {x:g} {y:g} {a}) (length {G}) (name {q(nm)} (effects (font (size 1.27 1.27)))) '
                        f'(number {q(num)} (effects (font (size 1.27 1.27)))))')
    bot = h - G * (rows - 1) - G
    txt = (f'(symbol "pendant:{name}" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes) '
           f'(property "Reference" {q(ref)} (at 0 {h + 2.54:g} 0) (effects (font (size 1.27 1.27)))) '
           f'(property "Value" {q(name)} (at 0 {bot - 1.27:g} 0) (effects (font (size 1.27 1.27)))) '
           f'(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes))) '
           f'(property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes))) '
           f'(symbol "{name}_0_1" (rectangle (start {-w:g} {h + G:g}) (end {w:g} {bot:g}) (stroke (width 0.254) (type default)) (fill (type background)))) '
           f'(symbol "{name}_1_1" {" ".join(pins)}))')
    return S.loads(txt), pos

# ---------- geometry ----------

def rot(x, y, r):
    c, s = round(math.cos(math.radians(r))), round(math.sin(math.radians(r)))
    return x * c - y * s, x * s + y * c

def sdir(a):  # sheet unit vector for a library angle (library y points up, sheet y points down)
    return round(math.cos(math.radians(a))), -round(math.sin(math.radians(a)))

class Board:
    def __init__(self, parts, layout, power):
        self.parts = {p["ref"]: p for p in parts}
        self.order = [p["ref"] for p in parts]
        self.power = set(power)
        self.sym, self.lpins = {}, {}
        for r, p in self.parts.items():
            node, pins = stock(p["sym"]) if isinstance(p["sym"], str) else p["sym"]
            self.sym[r], self.lpins[r] = node, pins
            missing = set(pins) - set(p["nets"])
            assert not missing, f"{r}: pins without a net or None: {sorted(missing)}"
        self.at = {}
        todo = dict(layout)
        assert set(todo) == set(self.parts), f"layout missing {set(self.parts) - set(todo)}, extra {set(todo) - set(self.parts)}"
        while todo:
            done = []
            for r, spec in todo.items():
                if spec[0] == "at":
                    _, x, y, rr = spec
                    self.at[r] = (x * G, y * G, rr)
                else:  # ("attach", mypin, other, otherpin, dx, dy, rot): put my pin dx, dy grid from the other pin
                    _, mp, o, op, dx, dy, rr = spec
                    if o not in self.at:
                        continue
                    tx, ty = self.pin(o, op)[:2]
                    px, py, _ = self.lpins[r][mp]
                    ox, oy = rot(px, py, rr)
                    self.at[r] = (round((tx + dx * G - ox) / 1.27) * 1.27, round((ty + dy * G + oy) / 1.27) * 1.27, rr)
                done.append(r)
            assert done, f"cannot place {sorted(todo)}"
            for r in done:
                del todo[r]

    def pin(self, r, n):
        """(x, y, outward unit vector) of pin n of part r on the sheet."""
        cx, cy, rr = self.at[r]
        px, py, a = self.lpins[r][n]
        ox, oy = rot(px, py, rr)
        dx, dy = sdir(a + rr)
        return cx + ox, cy - oy, (-dx, -dy)

    def body(self, r):
        """Grid points inside the box spanned by a part's pins, minus the pins (wires must not cross them)."""
        pts = {(k(x), k(y)) for x, y, _ in (self.pin(r, n) for n in self.lpins[r])}
        if len(pts) < 2:
            return set()
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        out = set()
        for i in range(min(xs), max(xs) + 1, 127):
            for j in range(min(ys), max(ys) + 1, 127):
                inside_x = min(xs) < i < max(xs) or min(xs) == max(xs)
                inside_y = min(ys) < j < max(ys) or min(ys) == max(ys)
                if inside_x and inside_y and (i, j) not in pts:
                    out.add((i, j))
        return out

# ---------- drawing ----------

DIRS = {(1, 0): 0, (0, -1): 90, (-1, 0): 180, (0, 1): 270}

def build(parts, layout, power, out_sch, out_lib, title, paper="A2", titles=(), name_pins=()):
    """name_pins: refs whose signal pins get a net label on their wire (e.g. the MCU), for readability."""
    b = Board(parts, layout, power)
    occ, blocked = {}, set()  # point -> {net: kind}; kind: "pin", "end", "h" or "v" (inside a wire)
    wires, labels, ncs, psyms = [], [], [], []

    def seg_pts(a, c):
        n = max(abs(k(c[0]) - k(a[0])), abs(k(c[1]) - k(a[1]))) // 127
        return [(a[0] + (c[0] - a[0]) * i / max(n, 1), a[1] + (c[1] - a[1]) * i / max(n, 1)) for i in range(n + 1)]

    def mark(key, net, kind):
        cur = occ.setdefault(key, {})
        old = cur.get(net)
        cur[net] = kind if old is None or old == kind else ("pin" if "pin" in (old, kind) else "end")

    def free(path, net):
        for a, c in zip(path, path[1:]):
            pts = seg_pts(a, c)
            horiz = k(a[1]) == k(c[1])
            for i, (x, y) in enumerate(pts):
                key = (k(x), k(y))
                if key in blocked:
                    return False
                other = [kd for n_, kd in occ.get(key, {}).items() if n_ != net]
                if other:  # only a clean perpendicular crossing through the middle of both wires
                    if i in (0, len(pts) - 1) or len(pts) < 3 or any(kd != ("v" if horiz else "h") for kd in other):
                        return False
        return True

    def add_path(path, net):
        for a, c in zip(path, path[1:]):
            if (k(a[0]), k(a[1])) == (k(c[0]), k(c[1])):
                continue
            wires.append((a, c))
            pts = seg_pts(a, c)
            horiz = k(a[1]) == k(c[1])
            for i, (x, y) in enumerate(pts):
                mark((k(x), k(y)), net, "end" if i in (0, len(pts) - 1) else ("h" if horiz else "v"))

    def length(path):
        return sum(abs(c[0] - a[0]) + abs(c[1] - a[1]) for a, c in zip(path, path[1:]))

    for r in b.order:
        blocked |= b.body(r)
    for r in b.order:
        for n, net in b.parts[r]["nets"].items():
            x, y, _ = b.pin(r, n)
            mark((k(x), k(y)), net if net else f"__nc_{r}_{n}", "pin")
            blocked.discard((k(x), k(y)))

    failed = []
    W, Hh = {"A2": (594, 420), "A1": (841, 594)}[paper]
    NX, NY = int(W / G) - 2, int(Hh / G) - 2

    lanes = {}  # grid point -> net: the stretch of row in front of an MCU pin belongs to that pin's net
    for r in name_pins:
        for n, net in b.parts[r]["nets"].items():
            if net and net not in b.power:
                x, y, d = b.pin(r, n)
                for i in range(1, 17):
                    lanes.setdefault((int(round(x / G)) + d[0] * i, int(round(y / G)) + d[1] * i), net)

    def route_net(net, ends):
        """Dijkstra on the 2.54 grid from each pin to the net's tree. A wire may cross another net's wire
        only straight through the middle of it; it never turns there or touches another net's end or pin."""
        import heapq
        ends = sorted(ends, key=lambda m: (m[0] != "U3", not m[0].startswith("U"), b.order.index(m[0])))
        gx = lambda v: int(round(v / G))
        free_pin = lambda r_: len(b.lpins[r_]) <= 2  # passives, test points: a wire may leave any side
        hx, hy, hd = b.pin(*ends[0])
        tree = {(gx(hx), gx(hy)): None if free_pin(ends[0][0]) else hd}  # grid point -> required arrival outward vector (pins) or None
        rest = sorted(ends[1:], key=lambda m: abs(b.pin(*m)[0] - hx) + abs(b.pin(*m)[1] - hy))
        for r, n in rest:
            x, y, d = b.pin(r, n)
            start = (gx(x), gx(y))
            if start in tree:
                continue
            def state_ok(pt, dirn, turning):
                key = (k(pt[0] * G), k(pt[1] * G))
                if key in blocked or not (0 < pt[0] < NX and 0 < pt[1] < NY):
                    return None
                other = [kd for n_, kd in occ.get(key, {}).items() if n_ != net]
                if other:
                    if turning or pt in tree or any(kd != ("v" if dirn[1] == 0 else "h") for kd in other):
                        return None
                    return "cross"
                return "ok"
            starts = [(1, 0), (-1, 0), (0, 1), (0, -1)] if free_pin(r) else [d]
            dist = {(start, sd): 0 for sd in starts}
            prev = {}
            q = [(0, start, sd) for sd in starts]
            goal = None
            while q:
                c, pt, dirn = heapq.heappop(q)
                if dist.get((pt, dirn), 1e9) < c:
                    continue
                if pt in tree and pt != start:
                    need = tree[pt]
                    if need is None or (dirn[0] == -need[0] and dirn[1] == -need[1]):
                        goal = (pt, dirn)
                        break
                    continue
                for nd in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    if nd == (-dirn[0], -dirn[1]):
                        continue
                    turning = nd != dirn
                    if turning:
                        if pt == start or state_ok(pt, nd, True) != "ok":
                            continue  # leave a pin straight out; never turn on a crossing
                    nxt = (pt[0] + nd[0], pt[1] + nd[1])
                    lane = lanes.get(nxt)
                    if lane and lane != net and (nd[1] == 0 or turning):
                        continue  # another net's lane: cross it, never run along it
                    if turning and lanes.get(pt) not in (None, net):
                        continue
                    st = state_ok(nxt, nd, False)
                    if st is None:
                        if nxt in tree and not (occ.get((k(nxt[0] * G), k(nxt[1] * G)), {}).keys() - {net}):
                            pass  # arriving on our own tree is always allowed
                        else:
                            continue
                    nc = c + 1 + (3 if turning else 0) + (1 if st == "cross" else 0)
                    if nc < dist.get((nxt, nd), 1e9):
                        dist[(nxt, nd)] = nc
                        prev[(nxt, nd)] = (pt, dirn)
                        heapq.heappush(q, (nc, nxt, nd))
            if not goal:
                failed.append(f"{net} -> {r}.{n}")
                if os.environ.get("ROUTE_DEBUG") == "row":
                    hx_, hy_ = gx(hx), gx(hy)
                    print("ROW", hy_, [(xx, occ.get((k(xx * G), k(hy_ * G)))) for xx in range(hx_, hx_ + 30) if occ.get((k(xx * G), k(hy_ * G)))])
                if os.environ.get("ROUTE_DEBUG") == "1":
                    print("FAIL", net, r, n, "start", start, "dir", d, "tree", sorted(tree.items())[:6], "explored", len(dist))
                    for yy in range(start[1] - 3, start[1] + 4):
                        print("  ", yy, [(xx, {n_: kd for n_, kd in occ.get((k(xx * G), k(yy * G)), {}).items()}, (k(xx * G), k(yy * G)) in blocked)
                                         for xx in range(start[0] - 3, start[0] + 4) if occ.get((k(xx * G), k(yy * G))) or (k(xx * G), k(yy * G)) in blocked])
                continue
            pts, cur = [], goal
            while cur in prev:
                pts.append(cur[0]); cur = prev[cur]
            pts.append(start)
            pts.reverse()
            path = [pts[0]]  # keep only the corners
            for i in range(1, len(pts) - 1):
                a_, c_, e_ = pts[i - 1], pts[i], pts[i + 1]
                if (c_[0] - a_[0], c_[1] - a_[1]) != (e_[0] - c_[0], e_[1] - c_[1]):
                    path.append(c_)
            path.append(pts[-1])
            add_path([(px * G, py * G) for px, py in path], net)
            for px, py in pts:
                tree.setdefault((px, py), None)

    # power pins: a stub out of each pin; neighbours on one side share a symbol
    runs = {}
    for r in b.order:
        for n, net in b.parts[r]["nets"].items():
            if net in b.power:
                x, y, d = b.pin(r, n)
                pts = runs.setdefault((r, net, d), [])
                if (x, y) not in pts:  # stacked pins (e.g. DRV8837 GND + EP) share one stub
                    pts.append((x, y))
    for (r, net, d), pts in runs.items():
        vertical_side = d[1] == 0
        pts.sort(key=lambda p: (p[1], p[0]) if vertical_side else (p[0], p[1]))
        groups = [[pts[0]]]
        for p in pts[1:]:
            last = groups[-1][-1]
            step = abs(k(p[1]) - k(last[1])) if vertical_side else abs(k(p[0]) - k(last[0]))
            if step == 254:
                groups[-1].append(p)
            else:
                groups.append([p])
        for grp in groups:
            ends = []
            stub = 2 if len(b.lpins[r]) == 2 and d[1] == 0 else 1
            for x, y in grp:
                e = (x + d[0] * G * stub, y + d[1] * G * stub)
                assert free([(x, y), e], net), f"power stub of {r} ({net}) lands on another net at {e}; move a part"
                add_path([(x, y), e], net)
                ends.append(e)
            if len(ends) > 1:
                assert free([ends[0], ends[-1]], net), f"power run of {r} ({net}) touches another net; move a part"
                add_path([ends[0], ends[-1]], net)
                gnd = net == "GND"
                e = ends[-1] if gnd else ends[0]
                psyms.append((e, net, ((0, 1) if gnd else (0, -1)) if vertical_side else d))
            else:
                psyms.append((ends[0], net, d if d[0] == 0 else ((0, 1) if net == "GND" else (0, -1))))

    # reserve the grid point just outside every IC signal pin for that pin's net, so no other wire takes it
    for r in b.order:
        if len(b.lpins[r]) <= 2:
            continue
        for n, net in b.parts[r]["nets"].items():
            if net and net not in b.power:
                x, y, d = b.pin(r, n)
                key = (k(x + d[0] * G), k(y + d[1] * G))
                if not (occ.get(key, {}).keys() - {net}) and key not in blocked:
                    mark(key, net, "end")

    # signal nets: every net is wired, longest first so the short local ones fit around them
    nets = {}
    for r in b.order:
        for n, net in b.parts[r]["nets"].items():
            if net and net not in b.power:
                nets.setdefault(net, []).append((r, n))
    def spread(item):
        pts = [b.pin(*m) for m in item[1]]
        return -(max(p[0] for p in pts) - min(p[0] for p in pts) + max(p[1] for p in pts) - min(p[1] for p in pts))
    for net, ends in sorted(nets.items(), key=spread):
        route_net(net, ends)

    if failed:
        print("UNROUTED:", "; ".join(failed))

    # net names on the wires leaving the named parts (the MCU), where the wire runs straight
    for r in name_pins:
        for n, net in b.parts[r]["nets"].items():
            if not net or net in b.power:
                continue
            x, y, d = b.pin(r, n)
            for s in (2, 3, 1, 4):
                key = (k(x + d[0] * G * s), k(y + d[1] * G * s))
                if occ.get(key, {}).get(net) in ("h", "v") and len(occ[key]) == 1:
                    labels.append(((x + d[0] * G * s, y + d[1] * G * s), net, d))
                    break

    for r in b.order:
        for n, net in b.parts[r]["nets"].items():
            if net is None:
                x, y, _ = b.pin(r, n)
                ncs.append((x, y, r, n))

    # split wires wherever a pin or another wire's end sits on them, so every connection is end-to-end
    cut = {(k(x), k(y)) for r in b.order for x, y, _ in (b.pin(r, n) for n in b.lpins[r])}
    cut |= {(k(e[0]), k(e[1])) for a, c in wires for e in (a, c)}
    split = []
    for a, c in wires:
        pts = seg_pts(a, c)
        start = a
        for x, y in pts[1:-1]:
            if (k(x), k(y)) in cut:
                split.append((start, (x, y)))
                start = (x, y)
        split.append((start, c))
    wires[:] = split

    # junctions: three or more wire ends at a point, or a wire end on the middle of another wire
    deg = {}
    for a, c in wires:
        for e in (a, c):
            deg[(k(e[0]), k(e[1]))] = deg.get((k(e[0]), k(e[1])), 0) + 1
    for a, c in wires:
        for x, y in seg_pts(a, c)[1:-1]:
            if (k(x), k(y)) in deg:
                deg[(k(x), k(y))] += 2
    junctions = [key for key, n in deg.items() if n >= 3]

    write(b, wires, labels, ncs, psyms, junctions, out_sch, out_lib, title, paper, titles)

def write(b, wires, labels, ncs, psyms, junctions, out_sch, out_lib, title, paper, titles=()):
    root = uid("root")
    lib_syms, insts = {}, []
    fnt = "(effects (font (size 1.27 1.27))"

    def inst(lid, ref, value, x, y, r, props=(), pins=(), dnp=False, fp="", ds="", vpos=None, rpos=None, ang=0, just="", hide_value=False):
        hidden = ref.startswith("#")
        vx, vy = vpos or (x, y + 3.81)
        rx, ry = rpos or (x, y - 3.81)
        j = f" (justify {just})" if just else ""
        ptxt = [f'(property "Reference" {q(ref)} (at {rx:g} {ry:g} {ang}) {fnt}{j}' + (" (hide yes)" if hidden else "") + "))",
                f'(property "Value" {q(value)} (at {vx:g} {vy:g} {ang}) {fnt}{j}' + (" (hide yes)" if hide_value else "") + "))",
                f'(property "Footprint" {q(fp)} (at {x:g} {y:g} 0) {fnt} (hide yes)))',
                f'(property "Datasheet" {q(ds)} (at {x:g} {y:g} 0) {fnt} (hide yes)))']
        ptxt += [f'(property {q(kk)} {q(v)} (at {x:g} {y:g} 0) {fnt} (hide yes)))' for kk, v in props]
        pin_txt = " ".join(f'(pin {q(n)} (uuid "{uid(ref, n, "pin")}"))' for n in pins)
        insts.append(f'(symbol (lib_id {q(lid)}) (at {x:g} {y:g} {r}) (unit 1) (exclude_from_sim no) '
                     f'(in_bom {"no" if hidden else "yes"}) (on_board {"no" if hidden else "yes"}) (dnp {"yes" if dnp else "no"}) '
                     f'(uuid "{uid(ref)}") {" ".join(ptxt)} {pin_txt} '
                     f'(instances (project "standard" (path "/{root}" (reference {q(ref)}) (unit 1)))))')

    for r in b.order:
        p = b.parts[r]
        lid = b.sym[r][1]
        lib_syms[lid] = b.sym[r]
        x, y, rr = b.at[r]
        ends = [b.pin(r, n)[:2] for n in b.lpins[r]]
        ang, just, hide_value = rr % 180, "", False  # field angles are stored before the symbol's rotation
        if r.startswith("#FLG"):  # value beside the flag, clear of the power symbol
            rpos, vpos, just = (x, y), (x + 1.905, y + (-1.905 if rr == 0 else 1.905)), "left"
        elif r.startswith("TP") and rr in (90, 270):  # sideways test point: reference past its body
            side = 1 if rr == 270 else -1  # above the test point's body, clear of the wire
            rpos, vpos, just, hide_value = (x + 2.54 * side, y - 1.905), (x, y), "", True
        elif r.startswith("TP"):
            rpos, vpos, just, hide_value = (x + 1.905, y - (1.27 if rr == 0 else -1.27)), (x, y), "left", True
        elif len(ends) == 2 and k(ends[0][0]) == k(ends[1][0]):  # vertical R/C/L: text to the right (or left)
            side = -1 if p.get("text_left") else 1
            rpos, vpos, just = (x + 1.905 * side, y - 1.27), (x + 1.905 * side, y + 1.27), "left" if side > 0 else "right"
        elif len(ends) == 2:  # horizontal: reference and value above the body
            rpos, vpos = (x, y - 3.81), (x, y - 2.03)
        else:  # ICs and connectors: above and below the pin rows
            xs, ys = [e[0] for e in ends], [e[1] for e in ends]
            rpos, vpos = ((min(xs) + max(xs)) / 2, min(ys) - 3.81), ((min(xs) + max(xs)) / 2, max(ys) + 3.81)
            ang = 0
        if rr % 180 and (len(ends) == 2 or r.startswith(("TP", "#FLG"))):
            ang = 90
        inst(lid, r, p["value"], x, y, rr, p.get("fields", {}).items(), b.lpins[r], p.get("dnp"), p.get("fp", ""), p.get("ds", ""),
             vpos, rpos, ang, just, hide_value)

    for lid in ("power:GND", "power:VCC"):
        lib_syms[lid] = stock(lid)[0]
    for i, ((x, y), net, out) in enumerate(psyms):
        base, lid = (270, "power:GND") if net == "GND" else (90, "power:VCC")
        r = (DIRS[out] - base) % 360
        inst(lid, f"#PWR{i + 1:03d}", net, x, y, r, pins=["1"], vpos=(x + 3.81 * out[0], y + 3.81 * out[1]))

    out = [f'(wire (pts (xy {a[0]:g} {a[1]:g}) (xy {c[0]:g} {c[1]:g})) (stroke (width 0) (type default)) (uuid "{uid("w", i)}"))'
           for i, (a, c) in enumerate(wires)]
    for i, ((x, y), net, d) in enumerate(labels):
        ang = DIRS[d]
        just = "left" if ang in (0, 90) else "right"
        out.append(f'(label {q(net)} (at {x:g} {y:g} {ang}) (fields_autoplaced yes) (effects (font (size 1.27 1.27)) (justify {just})) (uuid "{uid("l", i)}"))')
    out += [f'(no_connect (at {x:g} {y:g}) (uuid "{uid(r, n, "nc")}"))' for x, y, r, n in ncs]
    out += [f'(text {q(t)} (exclude_from_sim no) (at {x * G:g} {y * G:g} 0) (effects (font (size 2 2) (thickness 0.3) (bold yes)) (justify left bottom)) (uuid "{uid("t", t)}"))'
            for x, y, t in titles]
    out += [f'(junction (at {a / 100:g} {c / 100:g}) (diameter 0) (color 0 0 0 0) (uuid "{uid("j", a, c)}"))' for a, c in junctions]

    sch = (f'(kicad_sch (version 20231120) (generator "gen_schematic") (generator_version "2") (uuid "{root}") '
           f'(paper {q(paper)}) (title_block (title {q(title)})) '
           f'(lib_symbols {" ".join(S.dumps(n) for n in lib_syms.values())}) '
           + " ".join(insts + out) + ' (sheet_instances (path "/" (page "1"))))')
    open(out_sch, "w", encoding="utf-8").write(sch + "\n")
    customs = [n for kk, n in lib_syms.items() if kk.startswith("pendant:")]
    open(out_lib, "w", encoding="utf-8").write("(kicad_symbol_lib (version 20231120) (generator \"gen_schematic\") " + " ".join(
        S.dumps([n[0], n[1].split(":", 1)[1]] + n[2:]) for n in customs) + ")\n")
    d = os.path.dirname(out_sch)
    open(os.path.join(d, "sym-lib-table"), "w").write(
        '(sym_lib_table (version 7) (lib (name "pendant") (type "KiCad") (uri "${KIPRJMOD}/%s") (options "") (descr "")))\n'
        % os.path.basename(out_lib))
    pro = os.path.join(d, os.path.basename(out_sch).replace(".kicad_sch", ".kicad_pro"))
    if not os.path.exists(pro):
        open(pro, "w").write('{"meta": {"filename": "%s", "version": 1}}\n' % os.path.basename(pro))

# ---------- check against parts.py ----------

def check(parts, sch):
    want = {}
    for p in parts:
        for n, net in p["nets"].items():
            if net and not p["ref"].startswith("#"):
                want.setdefault(net, set()).add((p["ref"], n))
    xml = os.path.join(tempfile.mkdtemp(), "net.xml")
    subprocess.run(["kicad-cli", "sch", "export", "netlist", "--format", "kicadxml", "-o", xml, sch],
                   check=True, capture_output=True)
    got = {}
    for n in ET.parse(xml).getroot().iter("net"):
        nodes = {(x.get("ref"), x.get("pin")) for x in n.iter("node") if not x.get("ref").startswith("#")}
        name = n.get("name").lstrip("/")
        if nodes and not name.startswith("unconnected-"):
            got[name] = nodes
    # wired-only nets get automatic names (Net-(U2-VOS)), so compare the groups of pins, not the names
    got_sets = {frozenset(v): kk for kk, v in got.items()}
    bad = [net for net, v in want.items() if frozenset(v) not in got_sets]
    extra = [kk for v, kk in got_sets.items() if v not in {frozenset(x) for x in want.values()}]
    for net in sorted(bad):
        near = [kk for kk, v in got.items() if v & want[net]]
        print(f"MISMATCH {net}: want {sorted(want[net])}; pins sit in {near}")
    for kk in sorted(extra):
        print(f"EXTRA {kk}: {sorted(got[kk])}")
    return not bad and not extra

if __name__ == "__main__":
    sys.path.insert(0, HERE)
    from parts import PARTS, TITLE, POWER
    from layout import LAYOUT, TITLES
    sch = os.path.join(HERE, "standard.kicad_sch")
    build(PARTS, LAYOUT, POWER, sch, os.path.join(HERE, "pendant.kicad_sym"), TITLE, titles=TITLES, name_pins=["U3"])
    ok = check(PARTS, sch)
    print("wrote standard.kicad_sch;", "netlist matches parts.py" if ok else "NETLIST MISMATCH")
    sys.exit(0 if ok else 1)
