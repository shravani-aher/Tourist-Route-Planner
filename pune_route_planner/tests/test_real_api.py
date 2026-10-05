import json,subprocess,sys,time,urllib.request,urllib.error,tempfile,os
binary,data,web=sys.argv[1:];port=18089
p=subprocess.Popen([binary,'--data',data,'--web',web,'--port',str(port)],stdout=subprocess.DEVNULL)
try:
 for _ in range(100):
  try:
   g=json.load(urllib.request.urlopen(f'http://127.0.0.1:{port}/api/graph'));break
  except Exception:time.sleep(.1)
 assert g['real_data'] and g['manifest']['license']=='ODbL-1.0' and len(g['attractions'])==9
 road={r['id']:r for r in g['roads']};nodes={n['id']:n for n in g['places']}
 assert all(n['crowd'] is None and n['visit_minutes'] is None for n in g['places'])
 def request(path,obj):return json.load(urllib.request.urlopen(urllib.request.Request(f'http://127.0.0.1:{port}'+path,json.dumps(obj).encode(),{'Content-Type':'application/json'})))
 for a,b in [('SW','SB'),('SB','SW'),('SW','AK'),('SW','PH'),('SW','PG')]:
  result=request('/api/route',{'start':a,'end':b,'mode':'shortest','k':1})['best_route']
  assert result['found'] and result['avg_crowd'] is None and result['avg_scenic'] is None and result['demo_index'] is None
  assert abs(sum(road[x]['distance_km'] for x in result['roads'])-result['total_distance_km'])<1e-7
  for leg in result['legs']:
   r=road[leg['road_id']];assert (r['u'],r['v'])==(leg['from_id'],leg['to_id']) or not r['directed'] and (r['v'],r['u'])==(leg['from_id'],leg['to_id'])
 for path,obj in [('/api/route',{'start':'SW','end':'SB','mode':'scenic'}),('/api/simulate',{}),('/api/update',{'type':'traffic'}),('/api/route',{'start':'SW','end':'SB','mode':'shortest','k':2})]:
  try:request(path,obj);raise AssertionError('Should reject')
  except urllib.error.HTTPError as e:assert e.code==400 and json.load(e)['error']
finally:p.terminate();p.wait(timeout=10)
for bad in ['missing-file.json',os.path.join(os.path.dirname(data),'../tests/fixtures/pune_demo.json')]:
 r=subprocess.run([binary,'--data',bad],capture_output=True,timeout=20);assert r.returncode!=0
print('Real graph, paths, unavailable metrics, production rejection passed')

source=json.load(open(data))
for mutate in [lambda d:d['nodes'].append(d['nodes'][0]),lambda d:d['roads'][0].update(u='unknown'),lambda d:d['roads'][0].update(distance_km=-1),lambda d:d['roads'][0].update(distance_km=1),lambda d:d['manifest'].update(source_sha256='x'*64),lambda d:d['attractions'][0].update(node_id='unknown')]:
 clone=json.loads(json.dumps(source));mutate(clone)
 with tempfile.NamedTemporaryFile(mode='w',suffix='.json',delete=False) as f:json.dump(clone,f);name=f.name
 try:
  r=subprocess.run([binary,'--data',name],capture_output=True,timeout=30);assert r.returncode!=0
 finally:os.unlink(name)
print('Malformed datasets rejected')
