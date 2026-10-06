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
 for path,obj in [('/api/route',{'start':'SW','end':'SB','hour':24}),('/api/route',{'start':'SW','end':'SB','weekend':'x'}),('/api/simulate',{}),('/api/update',{'type':'traffic'}),('/api/route',{'start':'SW','end':'SB','mode':'shortest','k':6})]:
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
