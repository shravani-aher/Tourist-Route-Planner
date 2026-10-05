"""Real HTTP regressions; isolated ephemeral loopback server, fixtures only."""
import json, socket, subprocess, sys, time, urllib.request, urllib.error
binary, data, web = sys.argv[1:]
with socket.socket() as sock:
    sock.bind(('127.0.0.1', 0)); port = sock.getsockname()[1]
p = subprocess.Popen([binary, '--data', data, '--web', web, '--port', str(port)], stdout=subprocess.DEVNULL)
base = f'http://127.0.0.1:{port}'
checks = 0
def request(path, body=None, raw=None, expected=200):
    global checks
    payload = raw.encode() if raw is not None else json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(base + path, data=payload, headers={'Content-Type':'application/json'})
    try:
        with urllib.request.urlopen(req, timeout=5) as r: status, value = r.status, json.load(r)
    except urllib.error.HTTPError as e:
        status, value = e.code, json.load(e)
    assert status == expected, (path, body, raw, status, value)
    if expected == 400: assert isinstance(value['error'], str)
    checks += 1
    return value
q = {'start':'SW', 'end':'SB', 'mode':'fastest'}
try:
    for attempt in range(100):
        if p.poll() is not None: raise RuntimeError(f'Server exited: {p.returncode}')
        try: request('/api/graph'); break
        except (ConnectionError, urllib.error.URLError): time.sleep(.05)
    else: raise RuntimeError('Server did not start')
    request('/api/route', raw=' ' * 65537, expected=413)
    request('/api/missing', expected=404)
    result = request('/api/route', q)
    assert abs(result['best_route']['total_travel_time_min'] - 22.8) < 1e-6
    for mode in ['balanced','shortest','fastest','scenic','least_crowded']:
        result = request('/api/route', dict(q, mode=mode))
        assert result['best_route']['found']
    for raw in ['[]','null','1','{} trailing','{"start":"SW","end":"SB","k":1.5}',
                '{"start":"SW","end":"SB","k":1e999}', '{"start":"SW","end":"SB","k":01}',
                '{"start":"SW","end":"SB","mode":"fastest","mode":"scenic"}',
                '{"start":"SW","end":"SB","k":1.}', '{"start":"SW","end":"SB","k":1e+}',
                '{"start":"SW","end":"SB","mode":"bad\\q"}', '['*70 + '0' + ']'*70]:
        request('/api/route', raw=raw, expected=400)
    invalids = {'mode':[3,'FASTEST',''], 'start':[1,None,[]], 'end':[False,{}],
        'k':[0,-1,21,1e50,True,'5'], 'weights':[None,[],{'wd':'bad'},{'wd':-1},{'wd':2},{'wd':0,'wt':0,'ws':0,'wc':0,'wp':0}],
        'maxTimeMin':[0,-2,'100',False], 'maxDistanceKm':[0,-3,None],
        'maxDetourRatio':[0,.5,'2'], 'mustVisit':['DG',[1],[None]], 'avoid':[{},[False]], 'interests':[1,['']]}
    for key, values in invalids.items():
        for value in values: request('/api/route', dict(q, **{key:value}), expected=400)
    for bad_id in ['bad"id', 'bad\\id', 'bad\nid', 'पुणे', 'bad\tid']:
        request('/api/route', dict(q, mustVisit=[bad_id]), expected=400)
        request('/api/update', {'type':'block','roadId':bad_id}, expected=400)
    for body in [{'type':'bad"type','roadId':'e01'}, {'type':'traffic','roadId':'e01','value':'5'},
        {'type':'traffic','roadId':'e01'}, {'type':'block','placeId':'SW'},
        {'type':'place_crowd','placeId':'SW','value':1.2}, {'type':'traffic','roadId':'e01','value':11}]:
        request('/api/update', body, expected=400)
    request('/api/route', dict(q, end='RZ', mode='scenic', mustVisit=['DG'], maxTimeMin=100), expected=400)
    request('/api/route', dict(q, end='RZ', mustVisit=['DG'], maxDistanceKm=1), expected=400)
    zero = request('/api/route', dict(q, end='SW'))['best_route']
    assert zero['total_time_min'] == zero['total_visit_time_min'] == zero['stops'][0]['visit_minutes'] == 0
    tour = request('/api/route', dict(q, end='SW', mustVisit=['DG'], maxTimeMin=200))['best_route']
    assert sum(s['visit_minutes'] for s in tour['stops']) == tour['total_visit_time_min']
    # Limits apply to optional visits too.
    tour = request('/api/route', dict(q, end='RZ', mustVisit=['DG'], maxTimeMin=140, maxDistanceKm=14))['best_route']
    assert tour['total_time_min'] <= 140 and tour['total_distance_km'] <= 14
    # Recalculation and undo must use the same bounded solver.
    initial = request('/api/route', dict(q, maxTimeMin=23))['best_route']
    upd = request('/api/update', {'type':'traffic','roadId':initial['roads'][0],'value':10})
    current = upd['current_route']
    assert not current['found'] or current['total_travel_time_min'] <= 23
    undo = request('/api/undo', {})['current_route']
    assert undo['found'] and abs(undo['total_travel_time_min'] - 22.8) < 1e-6
    request('/api/simulate', {})
    request('/api/undo', {})
    print(f'{checks} HTTP regression requests passed')
finally:
    p.terminate()
    try: p.wait(timeout=5)
    except subprocess.TimeoutExpired: p.kill(); p.wait()
    if p.returncode not in (0, -15): raise RuntimeError(f'Server exited unexpectedly: {p.returncode}')
