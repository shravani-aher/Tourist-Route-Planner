import json,subprocess,sys,time,urllib.request,urllib.error,tempfile,os
binary,data,web=sys.argv[1:];port=18089
p=subprocess.Popen([binary,'--data',data,'--web',web,'--port',str(port)],stdout=subprocess.DEVNULL)
try:
 for _ in range(100):
  try:
   g=json.load(urllib.request.urlopen(f'http://127.0.0.1:{port}/api/graph'));break
  except Exception:time.sleep(.1)
 assert g['real_data'] and g['manifest']['license']=='ODbL-1.0' and len(g['attractions'])==18 and g['manifest']['estimated_metrics']
 road={r['id']:r for r in g['roads']};nodes={n['id']:n for n in g['places']}
 assert all((n['crowd'] is None)!=n['metrics_estimated'] for n in g['places'])
 assert all(0<=r['scenic']<=10 and r['highway'] for r in g['roads'])
 assert all(len(a['crowd_weekday'])==24 and a['estimated'] and a['visit_minutes']>0 and a['open_hour']<a['close_hour'] for a in g['attractions'])
 def request(path,obj):return json.load(urllib.request.urlopen(urllib.request.Request(f'http://127.0.0.1:{port}'+path,json.dumps(obj).encode(),{'Content-Type':'application/json'})))
 for a,b in [('SW','SB'),('SB','SW'),('SW','AK'),('SW','PH'),('SW','PG')]:
  result=request('/api/route',{'start':a,'end':b,'mode':'shortest','k':1})['best_route']
  assert result['found'] and result['avg_crowd'] is not None and result['avg_scenic'] is not None and result['demo_index'] is not None
  assert abs(sum(road[x]['distance_km'] for x in result['roads'])-result['total_distance_km'])<1e-7
  for leg in result['legs']:
   r=road[leg['road_id']];assert (r['u'],r['v'])==(leg['from_id'],leg['to_id']) or not r['directed'] and (r['v'],r['u'])==(leg['from_id'],leg['to_id'])
 modes=request('/api/route',{'start':'SW','end':'AK','mode':'balanced','hour':18,'weekend':True})['mode_routes']
 assert set(modes)=={'balanced','shortest','fastest','scenic','least_crowded'} and all(m['found'] for m in modes.values())
 assert modes['shortest']['total_distance_km']<=min(m['total_distance_km'] for m in modes.values())+1e-9
 assert modes['fastest']['total_travel_time_min']<=min(m['total_travel_time_min'] for m in modes.values())+1e-9
 assert modes['scenic']['avg_scenic']>=modes['shortest']['avg_scenic']-1e-9
 assert modes['least_crowded']['avg_crowd']<=modes['fastest']['avg_crowd']+1e-9
 night=request('/api/route',{'start':'SW','end':'AK','mode':'least_crowded','hour':3})['best_route']
 assert night['avg_crowd']<=modes['least_crowded']['avg_crowd']
 print({k:(round(v['total_distance_km'],2),round(v['total_travel_time_min'],1),round(v['avg_scenic'],2),round(v['avg_crowd'],2)) for k,v in modes.items()})
 alt=request('/api/route',{'start':'SW','end':'AK','mode':'fastest','k':3})
 rr=alt['ranked_routes'];assert 1<=len(rr)<=3 and rr[0]['found']
 assert rr[0]['total_travel_time_min']<=min(r['total_travel_time_min'] for r in rr)+1e-9
 sets=[set(r['roads']) for r in rr]
 for i in range(len(sets)):
  for j in range(i):assert len(sets[i]&sets[j])/len(sets[i]|sets[j])<.7
 assert len({tuple(r['roads']) for r in rr})==len(rr)
 print('alternatives',len(rr),[round(r['total_travel_time_min'],1) for r in rr])
 tour=request('/api/route',{'start':'SW','end':'SB','mustVisit':['AK','KM'],'hour':9})
 assert tour['is_tour'];t=tour['best_route'];st=t['stops']
 nid={a['id']:a['node_id'] for a in g['attractions']};ids=[x['place_id'] for x in st];assert ids[0]==nid['SW'] and ids[-1]==nid['SB'] and {nid['AK'],nid['KM']}<=set(ids)
 for a,b in zip(st,st[1:]):assert b['arrive_min']>=a['depart_min']
 for x in st[1:-1]:assert x['arrive_min']+x['wait_min']>=0 and x['visit_minutes']>0
 assert abs(t['total_time_min']-(t['total_travel_time_min']+t['total_visit_time_min']))<1e-6
 print('tour',ids,[(x['arrive_min'],x['wait_min']) for x in st])
 try:request('/api/route',{'start':'SW','end':'SB','mustVisit':['AK'],'hour':17});raise AssertionError('closed stop must be rejected')
 except urllib.error.HTTPError as e:assert e.code==400 and 'close' in json.load(e)['error']
 base=request('/api/route',{'start':'SW','end':'AK','mode':'fastest','hour':10})['best_route']
 victim=base['roads'][len(base['roads'])//2]
 upd=request('/api/update',{'type':'block','roadId':victim})
 assert upd['success'] and victim not in upd['current_route']['roads'] and upd['diff']['roads_removed'] and 'graph_version' in upd['graph'] and 'places' not in upd['graph']
 assert victim in upd['diff']['roads_removed']
 undo=request('/api/undo',{})
 assert undo['current_route']['roads']==base['roads'] and abs(undo['current_route']['total_travel_time_min']-base['total_travel_time_min'])<1e-9
 sp=request('/api/update',{'type':'traffic','roadId':victim,'value':10});assert sp['current_route']['total_travel_time_min']>=base['total_travel_time_min']-1e-9
 request('/api/undo',{})
 for bad in [{'type':'place_crowd','placeId':'SW','value':5},{'type':'block','roadId':'nope'}]:
  try:request('/api/update',bad);raise AssertionError('should reject')
  except urllib.error.HTTPError as e:assert e.code==400
 print('dynamic update, reroute diff, undo ok')
 import concurrent.futures as cf
 ref=request('/api/route',{'start':'SW','end':'AK','mode':'scenic','hour':9})['best_route']['roads']
 def one(i):return request('/api/route',{'start':'SW','end':'AK','mode':'scenic','hour':9})['best_route']['roads']
 with cf.ThreadPoolExecutor(8) as ex:assert all(r==ref for r in ex.map(one,range(24)))
 m=json.load(urllib.request.urlopen(f'http://127.0.0.1:{port}/api/map'));assert len(m['nodes'])==len(g['places']) and len(m['roads'])==len(g['roads'])
 for raw,ctype,code in [(b'{',  'application/json',400),(b'[]','application/json',400),(b'{"start":1,"end":2}','application/json',400),(b'x'*70000,'application/json',413),(b'{"start":"SW","end":"SB"}','text/plain',415),(b'{"start":"SW","end":"SB"}','application/x-www-form-urlencoded',415)]:
  try:urllib.request.urlopen(urllib.request.Request(f'http://127.0.0.1:{port}/api/route',raw,{'Content-Type':ctype}));raise AssertionError('should reject '+ctype)
  except urllib.error.HTTPError as e:assert e.code==code,(e.code,code,raw[:10])
 assert 'access-control-allow-origin' not in {k.lower() for k in urllib.request.urlopen(f'http://127.0.0.1:{port}/api/stats').headers}
 print('concurrency, malformed input, content-type and CORS checks ok')
 for path,obj in [('/api/route',{'start':'SW','end':'SB','hour':24}),('/api/route',{'start':'SW','end':'SB','weekend':'x'}),('/api/simulate',{}),('/api/route',{'start':'SW','end':'SB','mode':'shortest','k':6})]:
  try:request(path,obj);raise AssertionError('Should reject')
  except urllib.error.HTTPError as e:assert e.code==400 and json.load(e)['error']
finally:p.terminate();p.wait(timeout=10)
for bad in ['missing-file.json',os.path.join(os.path.dirname(data),'../tests/fixtures/pune_demo.json')]:
 r=subprocess.run([binary,'--data',bad],capture_output=True,timeout=20);assert r.returncode!=0
print('Real graph, paths, unavailable metrics, production rejection passed')

source=json.load(open(data))
for mutate in [lambda d:d['nodes'].append(d['nodes'][0]),lambda d:d['roads'][0].update(u='unknown'),lambda d:d['roads'][0].update(distance_km=-1),lambda d:d['roads'][0].update(distance_km=1),lambda d:d['manifest'].update(source_sha256='x'*64),lambda d:d['attractions'][0].update(node_id='unknown'),lambda d:d['roads'][0].update(scenic=11),lambda d:d['roads'][0].pop('highway'),lambda d:d['attractions'][0].update(crowd_weekday=[1]),lambda d:d['attractions'][0].update(open_hour=20,close_hour=8)]:
 clone=json.loads(json.dumps(source));mutate(clone)
 with tempfile.NamedTemporaryFile(mode='w',suffix='.json',delete=False) as f:json.dump(clone,f);name=f.name
 try:
  r=subprocess.run([binary,'--data',name],capture_output=True,timeout=30);assert r.returncode!=0
 finally:os.unlink(name)
print('Malformed datasets rejected')
