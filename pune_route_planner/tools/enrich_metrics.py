#!/usr/bin/env python3
"""Adds model-estimated metrics (schema v3) to the base v2 OSM graph. Offline, deterministic.
Usage: enrich_metrics.py base_v2.json source.osm.pbf out_v3.json
Scenic: per segment, from OSM parks/water/heritage proximity, minus busy-road penalty (0..10).
Catalog: category, visit minutes, hours and crowd curves from attractions_meta.json (estimates)."""
import json, math, os, sys, tempfile, pathlib
import osmium
HERE = pathlib.Path(__file__).parent
B = (18.485, 73.825, 18.565, 73.915)
PAD = .01
W = {'park': (60.0, .45), 'nature': (90.0, .35), 'heritage': (120.0, .30)}
CLASS_PENALTY = {'motorway': .25, 'trunk': .2, 'primary': .15, 'secondary': .08}
def km(a, b):
    la1, la2 = math.radians(a[1]), math.radians(b[1]); dl = math.radians(b[0] - a[0]); dt = la2 - la1
    return 6371.0088 * 2 * math.asin(min(1, math.sqrt(math.sin(dt/2)**2 + math.cos(la1)*math.cos(la2)*math.sin(dl/2)**2)))
def near(lon, lat): return B[0]-PAD <= lat <= B[2]+PAD and B[1]-PAD <= lon <= B[3]+PAD
class Scan(osmium.SimpleHandler):
    def __init__(s, wanted):
        super().__init__(); s.feat = []; s.hw = {}; s.wanted = wanted; s.geom = {}
    @staticmethod
    def kind(t):
        if t.get('leisure') in ('park', 'garden', 'nature_reserve') or t.get('landuse') in ('forest', 'grass'): return 'park'
        if t.get('natural') in ('water', 'wood', 'scrub', 'peak') or t.get('waterway') in ('river', 'stream', 'canal'): return 'nature'
        if t.get('historic') or t.get('tourism') in ('attraction', 'museum', 'viewpoint', 'zoo', 'gallery'): return 'heritage'
    def node(s, n):
        if not n.location.valid() or not near(n.location.lon, n.location.lat): return
        t = n.tags; k = s.kind(t)
        if k: s.feat.append((k, n.location.lon, n.location.lat))
        if ('n', n.id) in s.wanted: s.geom[('n', n.id)] = [[n.location.lon, n.location.lat]]
    def way(s, w):
        t = w.tags; pts = [[n.lon, n.lat] for n in w.nodes if n.location.valid()]
        if t.get('highway'): s.hw[str(w.id)] = t['highway'].removesuffix('_link')
        if ('w', w.id) in s.wanted and pts: s.geom[('w', w.id)] = pts
        k = s.kind(t)
        if k:
            for p in pts:
                if near(*p): s.feat.append((k, p[0], p[1]))
def crowd_curve(meta, weekend):
    lo, hi = meta['peak']; pop = meta['popularity']; out = []
    for h in range(24):
        if h < meta['open'] or h >= meta['close']: out.append(0); continue
        base = pop * .35
        d = 0 if lo <= h < hi else min(abs(h - lo), abs(h - hi + 1))
        v = base + (pop - base) * max(0.0, 1 - d / 4)
        if weekend: v = min(10.0, v * 1.3)
        out.append(round(min(10, v)))
    return out
def main(base_path, pbf, out):
    base = json.load(open(base_path)); meta = json.load(open(HERE / 'attractions_meta.json'))
    wanted = {(k, r) for k, r, _ in meta['new_features'].values()}
    sc = Scan(wanted); sc.apply_file(pbf, locations=True, filters=[osmium.filter.KeyFilter('highway','leisure','landuse','natural','waterway','historic','tourism')])
    cell = .002; grid = {}
    for k, lo, la in sc.feat: grid.setdefault((int(lo / cell), int(la / cell)), []).append((k, lo, la))
    nodes = {n['id']: (n['lon'], n['lat']) for n in base['nodes']}
    def prox(mid):
        best = {}
        cx, cy = int(mid[0] / cell), int(mid[1] / cell)
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                for k, lo, la in grid.get((cx + dx, cy + dy), ()):
                    d = km(mid, (lo, la)) * 1000
                    if d < best.get(k, 1e9): best[k] = d
        return best
    for r in base['roads']:
        a, z = nodes[r['u']], nodes[r['v']]; mid = ((a[0]+z[0])/2, (a[1]+z[1])/2)
        p = prox(mid); s = .05
        for k, (rad, wt) in W.items():
            if k in p and p[k] < rad: s += wt * (1 - p[k] / rad)
        hw = sc.hw.get(r['osm_way'], 'residential')
        s -= CLASS_PENALTY.get(hw, 0)
        r['highway'] = hw; r['scenic'] = round(max(0.0, min(1.0, s)) * 10, 2)
    for a in base['attractions']:
        m = meta['attractions'][a['id']]; a.update(category=m['category'], visit_minutes=m['visit_minutes'], open_hour=m['open'], close_hour=m['close'], crowd_weekday=crowd_curve(m, False), crowd_weekend=crowd_curve(m, True), estimated=True)
    existing = {a['node_id'] for a in base['attractions']}
    cand_nodes = nodes
    for code, (kind, ref, name) in meta['new_features'].items():
        pts = sc.geom.get((kind, ref))
        if not pts: raise SystemExit('missing OSM feature ' + code)
        lo = min(p[0] for p in pts) - .006; hi = max(p[0] for p in pts) + .006; bo = min(p[1] for p in pts) - .006; to = max(p[1] for p in pts) + .006
        used = {r['u'] for r in base['roads']} | {r['v'] for r in base['roads']}
        cands = [(nid, q) for nid, q in cand_nodes.items() if nid in used and nid not in existing and lo < q[0] < hi and bo < q[1] < to]
        d, nid, p = min((km(p, q), nid, p) for p in pts for nid, q in cands)
        if d > .5: raise SystemExit('no safe snap ' + code)
        existing.add(nid); m = meta['attractions'][code]
        base['attractions'].append({'id': code, 'name': name, 'osm_feature': ('node/' if kind == 'n' else 'way/') + str(ref), 'lon': p[0], 'lat': p[1], 'node_id': nid, 'snap_distance_m': round(d * 1000, 2), 'snap_method': 'nearest mapped boundary/trail approach to eligible street vertex; entrance/parking unverified',
            'category': m['category'], 'visit_minutes': m['visit_minutes'], 'open_hour': m['open'], 'close_hour': m['close'], 'crowd_weekday': crowd_curve(m, False), 'crowd_weekend': crowd_curve(m, True), 'estimated': True})
    base['schema_version'] = 3
    mf = base['manifest']; mf['importer'] += ' + enrich_metrics v1'
    mf['unavailable'] = ['live_traffic', 'live_crowd']
    mf['estimated_metrics'] = {'scenic': 'Per-segment 0-10 from OSM parks/water/heritage proximity minus busy-road penalty. Model estimate, not a rating.',
        'crowd': 'Per-attraction hourly 0-10 weekday/weekend curves from attractions_meta.json. Curated planning estimate, not live.',
        'visit_minutes': 'Curated planning estimate.', 'hours': 'Curated planning estimate; not verified with venues.'}
    folder = os.path.dirname(os.path.abspath(out)); fd, tmp = tempfile.mkstemp(dir=folder)
    try:
        with os.fdopen(fd, 'w') as f: json.dump(base, f, separators=(',', ':'), ensure_ascii=False)
        os.replace(tmp, out)
    finally:
        if os.path.exists(tmp): os.unlink(tmp)
    print(len(base['attractions']), 'attractions;', len(base['roads']), 'roads enriched')
if __name__ == '__main__': main(*sys.argv[1:4])
