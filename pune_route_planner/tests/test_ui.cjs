const {chromium} = require('playwright');
(async () => {
 const b=await chromium.launch({...(process.env.CHROME_PATH ? {executablePath:process.env.CHROME_PATH} : {}),headless:true,args:['--no-sandbox']});
 const errors=[];
 const page=await b.newPage({viewport:{width:1440,height:1000}});
 page.on('pageerror',e=>errors.push(e.message));
 await page.goto(process.env.PLANNER_URL || 'http://127.0.0.1:8087');
 await page.waitForFunction(()=>document.querySelector('#startSelect').options.length===10);
 await page.selectOption('#startSelect','SW'); await page.selectOption('#endSelect','SB');
 await page.locator('[data-mode="fastest"]').click();
 await page.getByRole('button',{name:'Calculate Best Route'}).click();
 await page.waitForFunction(()=>state.currentRoute?.found);
 console.log('fastest',await page.evaluate(()=>state.currentRoute.total_travel_time_min));
 if(await page.evaluate(()=>state.currentRoute.total_travel_time_min)!==22.8) throw Error('Wrong fastest');
 await page.screenshot({path:(process.env.SCREENSHOT_DIR || '.') + '/planner-desktop-fixed.png',fullPage:true});
 await page.locator('#btnSimulateEvent').click();
 await page.waitForFunction(()=>!document.querySelector('#btnUndoAction').disabled);
 await page.locator('#btnUndoAction').click();
 for(const width of [320,390,768,1440]) {
   await page.setViewportSize({width,height:900});
   await page.waitForTimeout(100);
   const layout=await page.evaluate(()=>({width:innerWidth,scroll:document.documentElement.scrollWidth,
      offenders:[...document.querySelectorAll('body *')].filter(e=>e.getBoundingClientRect().right>innerWidth+1).map(e=>({tag:e.tagName,id:e.id,cls:e.className,right:e.getBoundingClientRect().right})).slice(0,10)}));
   console.log('layout',JSON.stringify(layout));
   if(width===390) await page.screenshot({path:(process.env.SCREENSHOT_DIR || '.') + '/planner-mobile-fixed.png',fullPage:true});
   if(layout.scroll>width) throw Error('Horizontal overflow at '+width);
 }
 if(errors.length) throw Error(errors.join('\n'));
 console.log('Browser smoke passed: calculate, simulate, undo; no page errors');
 await b.close();
})().catch(e=>{console.error(e);process.exit(1)});
