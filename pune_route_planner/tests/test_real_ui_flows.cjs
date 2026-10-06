const {chromium}=require('playwright');
(async()=>{const b=await chromium.launch({headless:true,args:['--no-sandbox']});const page=await b.newPage({viewport:{width:1280,height:900}});const errors=[];page.on('pageerror',e=>errors.push(e.message));
await page.goto(process.env.PLANNER_URL||'http://127.0.0.1:8088');
await page.waitForFunction(()=>document.querySelector('#status').textContent==='Route found',{},{timeout:90000});
// alternatives + cards
await page.selectOption('#start','SW');await page.selectOption('#end','AK');await page.selectOption('#k','3');await page.click('#plan');
await page.waitForFunction(()=>!document.querySelector('#plan').disabled,{},{timeout:90000});
if(await page.locator('.card').count()<2)throw Error('expected route option cards');
await page.locator('.card').nth(1).click();if(!(await page.locator('.card.on').count()))throw Error('card selection');
// tour with schedule
await page.selectOption('#k','1');await page.check('#stops input[value=KM]');await page.fill('#time','09:00');await page.click('#plan');
await page.waitForFunction(()=>!document.querySelector('#plan').disabled,{},{timeout:90000});
if(await page.locator('#itinerary li').count()<3)throw Error('itinerary missing: '+await page.locator('#status').innerText());
// closed-stop error is shown, not a crash
await page.uncheck('#stops input[value=KM]');await page.selectOption('#end','SB');await page.check('#stops input[value=AK]');await page.fill('#time','17:00');await page.click('#plan');
await page.waitForFunction(()=>!document.querySelector('#plan').disabled,{},{timeout:90000});
if(!/clos/i.test(await page.locator('#status').innerText()))throw Error('closed stop message missing');
// block + undo
await page.uncheck('#stops input[value=AK]');await page.fill('#time','10:00');await page.selectOption('#mode','fastest');await page.click('#plan');
await page.waitForFunction(()=>document.querySelector('#status').textContent==='Route found',{},{timeout:90000});
const before=await page.locator('#summary').innerText();
await page.check('#blockmode');
const pt=await page.evaluate(()=>{const r=route.roads[Math.floor(route.roads.length/2)],rd=roads.get(r),a=toScreen(nodes.get(rd.u)),c=toScreen(nodes.get(rd.v)),box=canvas.getBoundingClientRect();return{x:box.left+(a[0]+c[0])/2,y:box.top+(a[1]+c[1])/2}});
await page.mouse.click(pt.x,pt.y);await page.waitForFunction(()=>!document.querySelector('#undo').disabled,{},{timeout:60000});
if(!/Blocked/.test(await page.locator('#edit').innerText()))throw Error('block message missing');
await page.click('#undo');await page.waitForFunction(()=>document.querySelector('#undo').disabled,{},{timeout:90000});
if(await page.locator('#summary').innerText()!==before)throw Error('undo did not restore the route summary');
if(errors.length)throw Error(errors.join('\n'));await b.close();console.log('UI flows passed');})().catch(e=>{console.error(e);process.exit(1)});
