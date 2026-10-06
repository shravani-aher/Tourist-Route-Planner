'use strict';
let graph,nodes=new Map(),roads=new Map(),route=null,options=[],selected=0,view=null,fit=null,blocked=new Set(),edits=0;
const $=id=>document.getElementById(id),canvas=$('map'),COLORS=['#227251','#c2571a','#2b6cb0','#8a4fbf'];
const post=async(path,body)=>{const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}),d=await r.json();if(!r.ok)throw Error(d.error||'Request failed');return d;};
const hm=m=>`${String(Math.floor(m/60)%24).padStart(2,'0')}:${String(m%60).padStart(2,'0')}`;
function setupView(){const b=graph.manifest.bbox,cos=Math.cos((b[0]+b[2])/2*Math.PI/180),box=canvas.getBoundingClientRect(),dx=(b[3]-b[1])*cos,dy=b[2]-b[0],scale=Math.min((box.width-20)/dx,(box.height-20)/dy);fit={cos,b,scale,ox:(box.width-dx*scale)/2,oy:(box.height-dy*scale)/2,w:box.width,h:box.height};if(!view)view={z:1,tx:0,ty:0};}
const base=n=>[fit.ox+(n.x-fit.b[1])*fit.cos*fit.scale,fit.h-fit.oy-(n.y-fit.b[0])*fit.scale];
const toScreen=n=>{const p=base(n);return[p[0]*view.z+view.tx,p[1]*view.z+view.ty];};
function draw(){if(!graph)return;const box=canvas.getBoundingClientRect(),ratio=devicePixelRatio||1;canvas.width=box.width*ratio;canvas.height=box.height*ratio;if(!fit||Math.abs(fit.w-box.width)>1||Math.abs(fit.h-box.height)>1)setupView();const c=canvas.getContext('2d');c.scale(ratio,ratio);
 const line=r=>{const a=nodes.get(r.u),b=nodes.get(r.v);if(!a||!b)return;c.moveTo(...toScreen(a));c.lineTo(...toScreen(b));};
 c.beginPath();graph.roads.forEach(line);c.strokeStyle='#bac8bd';c.lineWidth=.6;c.stroke();
 if(blocked.size){c.beginPath();blocked.forEach(id=>{const r=roads.get(id);if(r)line(r);});c.strokeStyle='#b3261e';c.lineWidth=4;c.stroke();}
 options.forEach((o,i)=>{if(i===selected)return;c.beginPath();o.roads.forEach(id=>line(roads.get(id)));c.strokeStyle=COLORS[i%4]+'88';c.lineWidth=2.5;c.stroke();});
 if(options[selected]){c.beginPath();options[selected].roads.forEach(id=>line(roads.get(id)));c.strokeStyle=COLORS[selected%4];c.lineWidth=4;c.stroke();}
 const show=view.z>=1.6;graph.attractions.forEach(a=>{const p=toScreen(nodes.get(a.node_id));c.beginPath();c.arc(...p,5,0,Math.PI*2);c.fillStyle='#23483b';c.fill();c.fillStyle='#173b30';c.font='bold 11px system-ui';c.fillText(show?a.name:a.id,p[0]+7,p[1]-6);});}
function resetView(){view={z:1,tx:0,ty:0};draw();}
function zoomAt(f,x,y){const z=Math.max(1,Math.min(40,view.z*f)),k=z/view.z;view.tx=x-(x-view.tx)*k;view.ty=y-(y-view.ty)*k;view.z=z;draw();}
let drag=null,moved=false;
canvas.addEventListener('pointerdown',e=>{drag={x:e.clientX,y:e.clientY,tx:view.tx,ty:view.ty};moved=false;canvas.setPointerCapture(e.pointerId);});
canvas.addEventListener('pointermove',e=>{if(!drag)return;const dx=e.clientX-drag.x,dy=e.clientY-drag.y;if(Math.abs(dx)+Math.abs(dy)>4)moved=true;view.tx=drag.tx+dx;view.ty=drag.ty+dy;draw();});
canvas.addEventListener('pointerup',e=>{drag=null;if(!moved&&$('blockmode').checked)clickBlock(e);});
canvas.addEventListener('wheel',e=>{e.preventDefault();const r=canvas.getBoundingClientRect();zoomAt(e.deltaY<0?1.25:.8,e.clientX-r.left,e.clientY-r.top);},{passive:false});
canvas.addEventListener('keydown',e=>{const r=canvas.getBoundingClientRect();if(e.key==='+'||e.key==='=')zoomAt(1.25,r.width/2,r.height/2);else if(e.key==='-')zoomAt(.8,r.width/2,r.height/2);else if(e.key.startsWith('Arrow')){view.tx+=e.key==='ArrowLeft'?30:e.key==='ArrowRight'?-30:0;view.ty+=e.key==='ArrowUp'?30:e.key==='ArrowDown'?-30:0;draw();}});
$('zin').onclick=()=>{const r=canvas.getBoundingClientRect();zoomAt(1.4,r.width/2,r.height/2);};$('zout').onclick=()=>{const r=canvas.getBoundingClientRect();zoomAt(.7,r.width/2,r.height/2);};$('zfit').onclick=resetView;
function nearestRoad(px,py){let best=null,bd=1e9;for(const r of graph.roads){const a=toScreen(nodes.get(r.u)),b=toScreen(nodes.get(r.v)),vx=b[0]-a[0],vy=b[1]-a[1],l=vx*vx+vy*vy||1,t=Math.max(0,Math.min(1,((px-a[0])*vx+(py-a[1])*vy)/l)),dx=a[0]+t*vx-px,dy=a[1]+t*vy-py,d=dx*dx+dy*dy;if(d<bd){bd=d;best=r;}}return bd<=14*14?best:null;}
async function clickBlock(e){const rc=canvas.getBoundingClientRect(),r=nearestRoad(e.clientX-rc.left,e.clientY-rc.top);if(!r){$('edit').textContent='No road near that point. Zoom in and click closer to a street.';return;}
 const type=blocked.has(r.id)?'unblock':'block';try{const d=await post('/api/update',{type,roadId:r.id});type==='block'?blocked.add(r.id):blocked.delete(r.id);edits++;$('undo').disabled=false;applyEdit(d,`${type==='block'?'Blocked':'Unblocked'} ${r.name||'unnamed road'}.`);}catch(err){$('edit').textContent=err.message;}}
function applyEdit(d,msg){$('edit').textContent=msg+(d.diff&&d.diff.summary?' '+d.diff.summary:'');if(d.current_route){options=[d.current_route];selected=0;cards([]);render();}draw();}
$('undo').onclick=async()=>{try{const d=await post('/api/undo',{});edits=Math.max(0,edits-1);await syncBlocked();applyEdit(d,'Undid last edit.');if(!edits)$('undo').disabled=true;}catch(err){$('edit').textContent=err.message;}};
async function syncBlocked(){try{const g=await (await fetch('/api/graph')).json();blocked=new Set(g.roads.filter(r=>r.blocked).map(r=>r.id));}catch(e){}}
function names(r){const out=[];for(const l of r.legs){const n=roads.get(l.road_id).name||'unnamed road';const last=out[out.length-1];if(last&&last.n===n)last.km+=l.distance_km;else out.push({n,km:l.distance_km});}return out;}
function render(){const r=options[selected];if(!r)return;route=r;const s=r.stops&&r.stops.length>2&&r.stops.some(x=>x.arrive_min!==undefined);
 $('summary').textContent=`${r.total_distance_km.toFixed(2)} km · ${r.total_travel_time_min.toFixed(1)} min driving (estimate)${r.total_visit_time_min?` · ${Math.round(r.total_visit_time_min)} min visiting/waiting`:''}${r.avg_scenic!=null?` · scenic ${r.avg_scenic.toFixed(1)}/10 · crowd ${r.avg_crowd.toFixed(1)}/10 (estimates)`:''}`;
 $('steps').innerHTML='';names(r).forEach(x=>{const li=document.createElement('li');li.textContent=`${x.n} - ${x.km.toFixed(2)} km`;$('steps').append(li);});
 $('itinerary').innerHTML='';if(s)r.stops.forEach(x=>{const li=document.createElement('li');const a=x.arrive_min!==undefined?`${hm(x.arrive_min)}${x.wait_min?` (waits ${x.wait_min} min for opening)`:''}`:'';li.innerHTML='';const b=document.createElement('b');b.textContent=x.name;li.append(b,document.createTextNode(` ${a}`));if(x.visit_minutes&&!x.is_end){const sm=document.createElement('small');sm.textContent=` · ${x.visit_minutes} min visit${x.crowd_at_arrival!=null?`, crowd ~${x.crowd_at_arrival}/10`:''}`;li.append(sm);}$('itinerary').append(li);});
 document.querySelectorAll('.card').forEach((el,i)=>el.classList.toggle('on',i===selected));draw();}
function cards(list){const el=$('cards');el.innerHTML='';if(list.length<2)return;list.forEach((o,i)=>{const b=document.createElement('button');b.className='card';b.type='button';const t=document.createElement('b'),sp=document.createElement('span');t.textContent=o.label;sp.textContent=`${o.route.total_distance_km.toFixed(1)} km · ${o.route.total_travel_time_min.toFixed(0)} min${o.route.avg_scenic!=null?` · scenic ${o.route.avg_scenic.toFixed(1)} · crowd ${o.route.avg_crowd.toFixed(1)}`:''}`;b.append(t,sp);b.onclick=()=>{selected=i;render();};el.append(b);});}
const LABELS={balanced:'Balanced',shortest:'Shortest',fastest:'Fastest',scenic:'Most scenic',least_crowded:'Least crowded'};
async function plan(){$('plan').disabled=true;$('status').textContent='Finding route...';try{const [h]=$('time').value.split(':'),stops=[...document.querySelectorAll('#stops input:checked')].map(i=>i.value).filter(v=>v!==$('start').value&&v!==$('end').value);
 const body={start:$('start').value,end:$('end').value,mode:$('mode').value,k:+$('k').value,hour:+h||0,weekend:$('weekend').checked};if(stops.length)body.mustVisit=stops;
 const data=await post('/api/route',body);edits=0;$('undo').disabled=true;blocked=new Set();$('edit').textContent='';selected=0;
 if(data.is_tour){options=[data.best_route];cards([]);}
 else{const list=[];const seen=new Set();(data.ranked_routes||[]).forEach((r,i)=>{list.push({label:i===0?`${LABELS[$('mode').value]} (best)`:`Alternative ${i+1}`,route:r});seen.add(r.roads.join(','));});
  if(data.mode_routes&&list.length===1)Object.entries(data.mode_routes).forEach(([m,r])=>{const key=r.roads.join(',');if(r.found&&!seen.has(key)&&m!==$('mode').value){seen.add(key);list.push({label:LABELS[m]+' route',route:r});}});
  options=list.map(x=>x.route);cards(list);}
 route=options[0];$('status').textContent='Route found';
 const aa=graph.attractions.filter(a=>a.id===$('start').value||a.id===$('end').value);$('snap').textContent=aa.map(a=>`${a.name}: ${a.snap_distance_m} m map-derived approach gap, not included in driving totals.`).join(' ');render();}
 catch(e){route=null;options=[];cards([]);$('itinerary').innerHTML='';$('steps').innerHTML='';$('status').textContent=e.message;$('summary').textContent='';draw();}finally{$('plan').disabled=false;}}
$('plan').onclick=plan;window.addEventListener('resize',()=>{fit=null;draw();});
(async()=>{try{const r=await fetch('/api/graph');if(!r.ok)throw Error('Graph unavailable');graph=await r.json();if(!graph.real_data)throw Error('Real-data UI requires a validated OSM dataset');graph.places.forEach(n=>nodes.set(n.id,n));graph.roads.forEach(r=>roads.set(r.id,r));
 for(const id of ['start','end'])graph.attractions.forEach(a=>{const o=document.createElement('option');o.value=a.id;o.textContent=a.name;$(id).append(o);});$('end').value='SB';
 graph.attractions.forEach(a=>{const l=document.createElement('label'),i=document.createElement('input');i.type='checkbox';i.value=a.id;l.append(i,document.createTextNode(a.name));$('stops').append(l);});
 $('credit').textContent=graph.manifest.attribution+' · ODbL';const a=document.createElement('a');a.href=graph.manifest.license_url;a.textContent=' Attribution and license';$('credit').append(a);
 $('provenance').textContent=`OSM snapshot: ${graph.manifest.snapshot}. Profile: ${graph.manifest.profile}. ${graph.places.length.toLocaleString()} street vertices, ${graph.roads.length.toLocaleString()} road segments. Source: ${graph.manifest.source_url}. ${graph.manifest.time_model}`;draw();await plan();}catch(e){$('status').textContent=e.message;}})();
