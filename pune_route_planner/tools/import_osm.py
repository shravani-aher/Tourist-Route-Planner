#!/usr/bin/env python3
"""Offline driving graph compiler. Python 3 + osmium==4.1.1. No online APIs."""
import argparse, hashlib, json, math, os, tempfile
import osmium
BBOX=(18.485,73.825,18.565,73.915)
SPEED={'motorway':80,'trunk':60,'primary':40,'secondary':35,'tertiary':30,'residential':20,'unclassified':20,'living_street':10,'service':10}
CAT={'SW':('w',1145089204,'Shaniwar Wada'), 'LM':('w',217271037,'Lal Mahal'), 'DG':('w',264276391,'Dagadusheth Halwai Ganapati Temple'), 'PT':('w',218078556,'Pataleshwar'), 'KM':('w',264281528,'Raja Dinkar Kelkar Museum'), 'SB':('w',53272975,'Sarasbaug'), 'PH':('n',2207888162,'Parvati trail approach'), 'PG':('w',197954185,'P L Deshpande Garden'), 'AK':('w',215998826,'Aga Khan Palace')}
def inside(p):return BBOX[0]<=p[1]<=BBOX[2] and BBOX[1]<=p[0]<=BBOX[3]
def km(a,b):
 lat1,lat2=map(math.radians,(a[1],b[1])); dl=math.radians(b[0]-a[0]);dt=lat2-lat1
 return 6371.0088*2*math.asin(min(1,math.sqrt(math.sin(dt/2)**2+math.cos(lat1)*math.cos(lat2)*math.sin(dl/2)**2)))
def permitted(t):
 h=t.get('highway','');base=h.removesuffix('_link')
 if base not in SPEED or t.get('area')=='yes':return False
 # More specific access overrides generic access. Destination/private/conditional roads
 # are conservatively omitted; do not invent entitlement or timed access.
 access=next((t[k] for k in ('motorcar','motor_vehicle','vehicle','access') if k in t),'yes')
 if access not in ('yes','permissive','designated'):return False
 if any('conditional' in k or k.startswith('barrier') or k.endswith(':forward') or k.endswith(':backward') or k.startswith('oneway:') for k in t):return False
 if t.get('oneway') not in (None,'yes','1','true','no','0','false','-1'):return False
 return True
class Importer(osmium.SimpleHandler):
 def __init__(self):
  super().__init__();self.ways={};self.poi={};self.rules=[];self.barriers=set()
 def node(self,n):
  if not n.location.valid():return
  p=[n.location.lon,n.location.lat]
  if not inside(p):return
  t=dict(n.tags)
  if (t.get('barrier') and t.get('barrier') not in ('cattle_grid',)) or any(t.get(k) in ('no','private','destination','customers') for k in ('access','vehicle','motor_vehicle','motorcar')) or any('conditional' in k for k in t):self.barriers.add(n.id)
  for code,(kind,ref,name) in CAT.items():
   if kind=='n' and n.id==ref:self.poi[code]=(p,f'node/{ref}')
 def way(self,w):
  t=dict(w.tags)
  if not t.get('highway') and not any(kind=='w' and w.id==ref for kind,ref,_ in CAT.values()):return
  pts=[(n.ref,[n.lon,n.lat]) for n in w.nodes if n.location.valid()]
  if not pts or not any(inside(p) for _,p in pts):return
  for code,(kind,ref,name) in CAT.items():
   if kind=='w' and w.id==ref:
    # Boundary vertex nearest a routable street is selected below, not a made-up centroid.
    self.poi[code]=([p for _,p in pts],f'way/{ref}')
  if permitted(t):self.ways[w.id]=(pts,t)
 def relation(self,r):
  if r.tags.get('type')=='restriction':self.rules.append((r.id,dict(r.tags),[(m.type,m.ref,m.role) for m in r.members]))
def file_sha256(path):
 digest=hashlib.sha256()
 with open(path,'rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):digest.update(chunk)
 return digest.hexdigest()
def compile_file(path,source_url):
 reader=osmium.io.Reader(path);snapshot=reader.header().get('osmosis_replication_timestamp');reader.close()
 if not snapshot:raise ValueError('OSM snapshot timestamp required')
 h=Importer();h.apply_file(path,locations=True,filters=[osmium.filter.KeyFilter("highway","name","barrier","type","access","vehicle","motor_vehicle","motorcar","access:conditional")])
 excluded=set();turns=[];conservative=[]
 for rid,t,members in h.rules:
  frm=[ref for kind,ref,role in members if kind=='w' and role=='from'];to=[ref for kind,ref,role in members if kind=='w' and role=='to'];via=[(kind,ref) for kind,ref,role in members if role=='via']
  if not any(w in h.ways for w in frm):continue
  if 'motorcar' in t.get('except','').split(';') or 'motor_vehicle' in t.get('except','').split(';'):continue
  rule=t.get('restriction:motorcar',t.get('restriction:motor_vehicle',t.get('restriction','')))
  if not rule and 'except' in t:continue
  if len(frm)==len(to)==len(via)==1 and via[0][0]=='n' and (rule.startswith('no_') or rule.startswith('only_')) and not any('conditional' in k for k in t):
   turns.append({'from_way':str(frm[0]),'to_way':str(to[0]),'via':str(via[0][1]),'only':rule.startswith('only_'),'osm_relation':str(rid)})
  else:
   # Fail closed for complex via-way or timed restrictions: remove the approach way.
   excluded.update(frm);conservative.append(str(rid))
 nodes={};roads=[]
 for wid,(pts,t) in sorted(h.ways.items()):
  if wid in excluded:continue
  base=t['highway'].removesuffix('_link');speed=SPEED[base];basis='profile estimate'
  raw=t.get('maxspeed','')
  try:
   tagged=float(raw.replace(' mph',''));tagged*=1.609344 if 'mph' in raw else 1
   if 0<tagged<=130:speed=min(speed,tagged);basis='min(tagged limit, profile estimate)'
  except ValueError:pass
  one=t.get('oneway','yes' if t.get('junction')=='roundabout' or base=='motorway' else 'no');rev=one=='-1'
  for i,((u,a),(v,b)) in enumerate(zip(pts,pts[1:])):
   if not inside(a) or not inside(b) or u in h.barriers or v in h.barriers or u==v:continue
   d=km(a,b)
   if d<=0:continue
   nodes[str(u)]=a;nodes[str(v)]=b
   roads.append({'id':f'{wid}:{i}','u':str(v if rev else u),'v':str(u if rev else v),'directed':one in ('yes','1','true','-1'),'distance_km':d,'base_time_min':d/speed*60,'osm_way':str(wid),'name':t.get('name',''),'speed_kph':speed,'time_basis':basis})
 # Catalog stays separate. Snaps are endpoints, no drivable imaginary connector.
 attractions=[]
 for code,(_,_,name) in CAT.items():
  if code not in h.poi:raise ValueError('Missing catalog OSM feature '+code)
  pos,ref=h.poi[code];points=pos if isinstance(pos[0],list) else [pos]
  lo=min(p[0] for p in points)-.006;hi=max(p[0] for p in points)+.006;bottom=min(p[1] for p in points)-.006;top=max(p[1] for p in points)+.006
  candidates=[(nid,q) for nid,q in nodes.items() if lo<q[0]<hi and bottom<q[1]<top]
  dist,node,p=min((km(p,q),nid,p) for p in points for nid,q in candidates)
  if dist>.5:raise ValueError('No safe bounded road snap '+code)
  attractions.append({'id':code,'name':name,'osm_feature':ref,'lon':p[0],'lat':p[1],'node_id':node,'snap_distance_m':round(dist*1000,2),'snap_method':'nearest mapped boundary/trail approach to eligible street vertex; entrance/parking unverified'})
 node_list=[{'id':k,'lon':v[0],'lat':v[1]} for k,v in sorted(nodes.items(),key=lambda x:int(x[0]))]
 valid_turns=[r for r in turns if r['via'] in nodes]
 return {'schema_version':2,'manifest':{'kind':'osm_driving','profile':'motorcar','source_url':source_url,'source_sha256':file_sha256(path),'snapshot':snapshot,'bbox':list(BBOX),'attribution':'© OpenStreetMap contributors','license':'ODbL-1.0','license_url':'https://www.openstreetmap.org/copyright','importer':'pyosmium 4.1.1 / importer v1','time_model':'Class-based free-flow estimate capped by parseable tagged maxspeed. Not live traffic or promised arrival time.','unavailable':['traffic','crowd','scenic','visit_duration'],'conservative_restriction_relations':conservative,'excluded_approach_ways':list(map(str,sorted(excluded)))},'nodes':node_list,'roads':roads,'attractions':attractions,'turn_restrictions':valid_turns}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('input');p.add_argument('output');p.add_argument('--source-url',required=True);a=p.parse_args();data=compile_file(a.input,a.source_url)
 folder=os.path.dirname(os.path.abspath(a.output));fd,tmp=tempfile.mkstemp(dir=folder)
 try:
  with os.fdopen(fd,'w') as f:json.dump(data,f,separators=(',',':'),ensure_ascii=False)
  os.replace(tmp,a.output)
 finally:
  if os.path.exists(tmp):os.unlink(tmp)
 print(len(data['nodes']),'nodes;',len(data['roads']),'segments;',len(data['turn_restrictions']),'node turns')
