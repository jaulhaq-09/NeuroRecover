// ═══════════════════════════════════════════════════════
// CONFIG & AUTH STATE
// ═══════════════════════════════════════════════════════
const SRV = (location.hostname==='localhost'||location.hostname==='127.0.0.1')
          ? `${location.protocol}//${location.host}` : null;
let serverOn=false, exeReady=false, isAdmin=false, aiOpen=false;
let currentUser = null;
let authToken = localStorage.getItem('nr_token') || null;

function getAuthHeaders() {
  const h = { 'Content-Type': 'application/json' };
  if (authToken) h['Authorization'] = 'Bearer ' + authToken;
  return h;
}

// ═══════════════════════════════════════════════════════
// INIT
// ═══════════════════════════════════════════════════════
async function init(){
  // default drives
  document.getElementById('drv').innerHTML =
    '<option value="//./PhysicalDrive0">💾 PhysicalDrive0 (System)</option>'+
    '<option value="//./PhysicalDrive1" selected>🔌 PhysicalDrive1 (USB) ← recommended</option>';
  if(!SRV){setMode('demo');renderAuthBar();return;}
  try{
    const r=await fetch(SRV+'/api/status',{signal:AbortSignal.timeout(3000)});
    if(!r.ok)throw 0;
    const s=await r.json();
    serverOn=true;exeReady=s.exeFound;isAdmin=s.isAdmin;
    if(s.defaultOutputDir && (!document.getElementById('outd').value || document.getElementById('outd').value==='C:\\recovered')){
      document.getElementById('outd').value=s.defaultOutputDir;
    }
    setMode(!isAdmin?'no-admin':!exeReady?'build':'live');
    loadDrives();
    checkAuth();
  }catch{serverOn=false;setMode('demo');toast('info','💡','Run "node server.js" as Administrator for real recovery');renderAuthBar();}
}

function setMode(m){
  const badge=document.getElementById('modeBadge');
  const admBan=document.getElementById('admBan');
  const admWarn=document.getElementById('adminWarn');
  const buildCard=document.getElementById('buildCard');
  const exeOk=document.getElementById('exeOk');
  const analBtn=document.getElementById('aiAnalBtn');
  admBan.classList.remove('show');admWarn.style.display='none';buildCard.style.display='none';exeOk.style.display='none';if(analBtn)analBtn.style.display='none';
  if(m==='live'){
    badge.className='badge b-live';badge.textContent='● Live Mode';
    exeOk.style.display='flex';if(analBtn)analBtn.style.display='inline-flex';
    document.getElementById('idleH').textContent='Real Recovery Ready';
    document.getElementById('idleP').textContent='Select your USB/SD drive and click Start Scan. 100% read-only.';
    toast('ok','🟢','Live mode — real recovery enabled!');
  }else if(m==='no-admin'){
    badge.className='badge b-err';badge.textContent='● No Admin Rights';
    admBan.classList.add('show');admWarn.style.display='block';
    document.getElementById('idleH').textContent='⚠️ Administrator Required';
    document.getElementById('idleP').textContent='Restart PowerShell as Administrator, then run: node server.js';
    toast('err','🔴','Not Administrator! Scanning will fail.');
  }else if(m==='build'){
    badge.className='badge b-warn';badge.textContent='● Engine Not Built';
    buildCard.style.display='block';
    toast('warn','🔨','Build the engine first — click "Build Engine"');
  }else{
    badge.className='badge b-off';badge.textContent='● Demo Mode';
  }
}

async function loadDrives(){
  if(!serverOn)return;
  try{
    const data=await(await fetch(SRV+'/api/drives')).json();
    const drives=data.drives;
    if(data.defaultOutputDir && (!document.getElementById('outd').value || document.getElementById('outd').value==='C:\\recovered')){
      document.getElementById('outd').value=data.defaultOutputDir;
    }
    if(drives?.length){
      ST.driveSizes={};
      drives.forEach(d=>{ST.driveSizes[d.deviceId]=d.size||0;});
      document.getElementById('drv').innerHTML=drives.map(d=>
        `<option value="${d.deviceId}" ${d.recommended?'selected':''}>`+
        `${d.isUSB?'🔌':'💾'} ${d.caption}${d.sizeStr&&d.sizeStr!=='?'?' ('+d.sizeStr+')':''}${d.recommended?' ← recommended':''}${d.isSystem?' ⚠ SYSTEM':''}</option>`
      ).join('');
    }
  }catch{}
}

function toggleCustomPath(){
  const wrap=document.getElementById('customPathWrap');
  const btn=document.getElementById('btnTogCustom');
  if(!wrap)return;
  if(wrap.style.display==='none'){
    wrap.style.display='block';
    if(btn)btn.textContent='💾 Presets';
    const inp=document.getElementById('customDrv');
    if(inp)inp.focus();
  }else{
    wrap.style.display='none';
    if(btn)btn.textContent='✏️ Custom';
    const inp=document.getElementById('customDrv');
    if(inp)inp.value='';
  }
}

function onDriveSelect(){
  const wrap=document.getElementById('customPathWrap');
  if(wrap)wrap.style.display='none';
  const btn=document.getElementById('btnTogCustom');
  if(btn)btn.textContent='✏️ Custom';
  const inp=document.getElementById('customDrv');
  if(inp)inp.value='';
}

// ═══════════════════════════════════════════════════════
// BUILD
// ═══════════════════════════════════════════════════════
function doBuild(){
  if(!serverOn){toast('err','❌','Server not running');return;}
  om('buildModal');
  const log=document.getElementById('buildLog');log.innerHTML='';
  document.getElementById('buildSub').textContent='Compiling...';
  document.getElementById('buildPb').style.width='0%';
  document.getElementById('buildBtn').disabled=true;
  const add=(msg,cls='')=>{const d=document.createElement('div');d.className=cls;d.textContent=msg;log.appendChild(d);log.scrollTop=log.scrollHeight;};
  fetch(SRV+'/api/build',{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'})
  .then(res=>{
    const reader=res.body.getReader(),dec=new TextDecoder();let buf='';
    const rd=()=>reader.read().then(({done,value})=>{
      if(done)return;buf+=dec.decode(value,{stream:true});
      const lines=buf.split('\n');buf=lines.pop();
      lines.filter(l=>l.startsWith('data:')).forEach(l=>{try{
        const e=JSON.parse(l.slice(5));
        if(e.type==='log')add(e.message,e.isError?'e':'');
        if(e.type==='progress')document.getElementById('buildPb').style.width=e.percent+'%';
        if(e.type==='error'){add(e.message,'e');toast('err','❌','Build error');}
        if(e.type==='done'){document.getElementById('buildBtn').disabled=false;
          if(e.success){exeReady=true;setMode('live');cm('buildModal');toast('ok','✅','Build complete!');}
          else{document.getElementById('buildSub').textContent='Failed';toast('err','❌','Build failed. Run ./build_windows.sh in MSYS2 MINGW64 as Admin.');}
        }
      }catch{}});rd();
    }).catch(()=>{document.getElementById('buildBtn').disabled=false;});rd();
  }).catch(e=>{add('Error: '+e.message,'e');document.getElementById('buildBtn').disabled=false;});
}

// ═══════════════════════════════════════════════════════
// STATE
// ═══════════════════════════════════════════════════════
const ST={
  files:[],filtered:[],sel:new Set(),
  cat:'all',ext:'all',view:'grid',
  scanning:false,scanId:null,xhr:null,
  counts:{all:0,image:0,video:0,audio:0,document:0,archive:0,raw:0},
  ec:{},lastStats:null,quickTypes:null,
  currentDir:'C:/recovered',
  prevIdx:0,
};

const EC={jpg:'image',jpeg:'image',png:'image',gif:'image',bmp:'image',tiff:'image',tif:'image',webp:'image',heic:'image',psd:'image',ico:'image',mp4:'video',mov:'video',avi:'video',mkv:'video',flv:'video',wmv:'video',webm:'video','3gp':'video',m4v:'video',mpeg:'video',mp3:'audio',wav:'audio',flac:'audio',m4a:'audio',ogg:'audio',aac:'audio',wma:'audio',pdf:'document',docx:'document',xlsx:'document',pptx:'document',odt:'document',rtf:'document',epub:'document',txt:'document',zip:'archive',rar:'archive','7z':'archive',gz:'archive',cr2:'raw',cr3:'raw',nef:'raw',arw:'raw',raf:'raw',dng:'raw',orf:'raw',rw2:'raw',sqlite:'database',db:'database'};
const CI={image:'🖼️',video:'🎬',audio:'🎵',document:'📄',archive:'📦',raw:'📷',database:'🗄️',other:'📎'};
const IMGEXTS=new Set(['jpg','jpeg','png','gif','bmp','webp','heic']);
const VIDEXTS=new Set(['mp4','mov','avi','mkv','flv','wmv','webm']);
const gc=e=>EC[e?.toLowerCase()]||'other';
const gi=e=>CI[gc(e)]||'📎';
const fz=b=>{if(!b)return'0 B';if(b<1024)return b+' B';if(b<1048576)return(b/1024).toFixed(1)+' KB';if(b<1073741824)return(b/1048576).toFixed(1)+' MB';return(b/1073741824).toFixed(2)+' GB';};
const fn=n=>Number(n).toLocaleString();

// ═══════════════════════════════════════════════════════
// SCAN
// ═══════════════════════════════════════════════════════
function quickScan(type){
  const m={photos:'jpg,jpeg,png,bmp,gif,heic,tiff,webp,cr2,nef,arw,dng,orf,rw2,psd',videos:'mp4,mov,avi,mkv,flv,wmv,webm,3gp,m4v',audio:'mp3,wav,flac,m4a,ogg,aac,wma'};
  ST.quickTypes=m[type]||null;
  toast('info',CI[type]||'📁','Quick scan: '+type+' only — 2-3x faster!');
  doScan();
}

const PHOTO_VIDEO_TYPES='jpg,jpeg,png,bmp,gif,heic,tiff,webp,cr2,nef,arw,dng,orf,rw2,psd,mp4,mov,avi,mkv,flv,wmv,webm,3gp,m4v';

function doScan(){
  if(ST.scanning)return;
  if(!ST.quickTypes)ST.quickTypes=PHOTO_VIDEO_TYPES; // scope to photos+videos only by default
  const custom=document.getElementById('customDrv')?document.getElementById('customDrv').value.trim():'';
  const dev=custom||document.getElementById('drv').value||'//./PhysicalDrive1';
  const isPhysical=/^((\/\/|\x5c\x5c)\.[\/\\]|physicaldrive|[a-z]:$)/i.test(dev.trim());

  if(serverOn&&!isAdmin&&isPhysical&&!custom){
    showError('Administrator Required','❌ Not running as Administrator!\n\nRaw physical disk access requires administrator rights.\n\nFIX:\n1. Close server / press Ctrl+C\n2. Right-click START_HERE.bat → "Run as Administrator"\n3. Refresh browser\n\nTip: You can scan disk image files (.img, .raw, .dd) without Administrator rights by clicking "✏️ Custom" above.');
    sh('st-idle',0);sh('st-error',1);return;
  }
  ST.scanning=true;ST.files=[];ST.sel.clear();ST.ec={};
  ST.counts={all:0,image:0,video:0,audio:0,document:0,archive:0,raw:0};
  const out=document.getElementById('outd').value||'RecoveredFiles';
  ST.currentDir=out.replace(/\\/g,'/');
  ST.deviceSizeBytes=(ST.driveSizes&&ST.driveSizes[dev])||0;
  // EaseUS-style: show the (initially empty) results grid right away instead
  // of a full-screen spinner blocking it. Files appear live as they're found;
  // scan status lives in the slim footer bar so the grid stays clickable
  // and previewable the entire time.
  sh('st-idle',0);sh('st-error',0);sh('st-scan',0);
  const res=document.getElementById('st-res');res.style.display='flex';res.style.flexDirection='column';
  document.getElementById('chipbar').style.display='flex';
  document.getElementById('fg').innerHTML='';document.getElementById('emp').style.display='none';
  document.getElementById('scanFooter').style.display='flex';
  document.getElementById('abar').style.display='none';
  document.getElementById('stopBtn').style.display='inline-flex';
  document.getElementById('scanBtn').disabled=true;
  document.getElementById('ss').textContent=serverOn&&exeReady?'Scanning':'Demo';
  document.getElementById('sleepPill').style.display='none';
  if(serverOn&&exeReady)realScan(dev,out);else demoScan();
}

function realScan(device,outputDir){
  const body={device,outputDir,types:ST.quickTypes||null,
    deep:document.getElementById('od').checked,
    metadata:document.getElementById('om').checked,
    preview:document.getElementById('op').checked,
    checkpoint:document.getElementById('oc').checked,
    sqliteDeep:document.getElementById('oq').checked,
    resume:document.getElementById('ors').checked,
  };
  ST.quickTypes=null;
  const xhr=new XMLHttpRequest();xhr.open('POST',SRV+'/api/scan',true);
  xhr.setRequestHeader('Content-Type','application/json');
  if(authToken) xhr.setRequestHeader('Authorization','Bearer '+authToken);
  xhr.timeout=0;ST.xhr=xhr;
  let pos=0,buf='';
  xhr.onprogress=()=>{
    const nd=xhr.responseText.slice(pos);pos=xhr.responseText.length;buf+=nd;
    const lines=buf.split('\n');buf=lines.pop();
    lines.filter(l=>l.startsWith('data:')).forEach(l=>{try{handleEv(JSON.parse(l.slice(5)));}catch{}});
  };
  xhr.onload=()=>{if(ST.scanning)doneScan();};
  xhr.onerror=()=>{ST.scanning=false;showError('Connection Error','Lost connection to server.\nMake sure node server.js is still running.');sh('st-scan',0);sh('st-error',1);document.getElementById('stopBtn').style.display='none';document.getElementById('scanBtn').disabled=false;};
  xhr.send(JSON.stringify(body));
}

function handleEv(e){
  switch(e.type){
    case 'pid':ST.scanId=e.scanId;break;
    case 'need_build':toast('warn','🔨','Build engine first!');stopScan();setMode('build');break;
    case 'need_admin':case 'access_denied':stopScan();showError('Administrator Required',e.message);sh('st-scan',0);sh('st-error',1);break;
    case 'start':document.getElementById('sleepPill').style.display='block';if(e.fwdOut)ST.currentDir=e.fwdOut;break;
    case 'file':addRF(e);break;
    case 'progress':upProg(e.percent,e.eta,e.files);break;
    case 'phase':addLog('── Phase: '+e.phase);break;
    case 'log':addLog(e.message);break;
    case 'warn':addLog('⚠ '+e.message,'w');break;
    case 'error':addLog('❌ '+e.message,'e');toast('err','❌',e.message.slice(0,60));break;
    case 'encryption':addLog(e.message,'w');toast('warn','🔒','Encrypted volume detected!');break;
    case 'done':
      ST.lastStats={filesFound:e.filesFound,diskFiles:e.diskFiles,byType:e.byType,byCategory:e.byCategory,outputDir:e.outputDir};
      toast('ok','✅','Scan complete! '+fn(e.filesFound||0)+' files found.');
      if(serverOn&&e.fwdOut){fetchFilesFromDisk(e.fwdOut).then(()=>doneScan());}
      else doneScan();
      break;
  }
}

function addRF(e){
  const ext=(e.ext||'').toLowerCase(),cat=gc(ext);
  const file={id:ST.files.length,name:e.name||e.path?.split(/[/\\]/).pop()||'file',
    ext,path:e.path,category:cat,size:(e.sizeKB||0)*1024,
    confidence:.88,repaired:false,real:true,thumbUrl:e.thumbUrl||null};
  ST.files.push(file);
  ST.counts.all=(e.total||0);ST.counts[cat]=(ST.counts[cat]||0)+1;
  ST.ec[ext]=(e.byType?.[ext])||((ST.ec[ext]||0)+1);
  upCounts();upLive(e.total||ST.counts.all,ST.counts);

  if(ST.cat==='all'||ST.cat===cat){
    appendFileCard(file);
  }
}

function upProg(pct,eta,files){
  const totalMB=(ST.deviceSizeBytes||126668*1024*1024)/1024/1024;
  const mb=((pct/100)*totalMB);
  const ft=document.getElementById('ftFound'),fs=document.getElementById('ftSize'),fp=document.getElementById('ftPct');
  if(ft)ft.textContent=fn(files||0);
  if(fs)fs.textContent=mb>1024?(mb/1024).toFixed(2)+' GB':fn(mb.toFixed(0))+' MB';
  if(fp)fp.textContent=pct.toFixed(1)+'%';
}

function addLog(msg,cls=''){
  const b=document.getElementById('logb');if(!b)return;
  const d=document.createElement('div');d.className='ll'+(cls?' '+cls:'');d.textContent=msg;
  b.appendChild(d);b.scrollTop=b.scrollHeight;
}

function upLive(t,c){
  document.getElementById('lv0').textContent=fn(t);
  document.getElementById('lv1').textContent=fn(c.image||0);
  document.getElementById('lv2').textContent=fn(c.video||0);
  document.getElementById('lv3').textContent=fn(c.audio||0);
}

function upCounts(){
  Object.keys(ST.counts).forEach(k=>{const e=document.getElementById('c-'+k);if(e)e.textContent=fn(ST.counts[k]);});
  ['jpg','png','bmp','mp4','mov','avi','mkv','mp3','pdf','cr2'].forEach(e=>{const el=document.getElementById('ch-'+e);if(el)el.textContent=fn(ST.ec[e]||0);});
  const a=document.getElementById('ch-all');if(a)a.textContent=fn(ST.counts.all);
}

function demoScan(){
  toast('info','🎭','Demo mode — run node server.js as Administrator for real scanning');
  const EXTS=['jpg','jpg','jpg','jpg','png','png','mp4','mp4','mov','avi','mkv','mp3','wav','pdf','docx','bmp','cr2','nef','heic'];
  const TOTAL=Math.floor(Math.random()*200)+80;
  let step=0,done=0;
  const iv=setInterval(()=>{
    step++;const batch=Math.ceil(TOTAL/50);
    for(let i=0;i<batch&&done<TOTAL;i++,done++){
      const ext=EXTS[Math.floor(Math.random()*EXTS.length)],cat=gc(ext);
      const sz={image:4e6,video:80e6,audio:6e6,document:600e3,raw:22e6};
      ST.files.push({id:done,name:`recovered_${String(done+1).padStart(6,'0')}.${ext}`,ext,size:Math.floor((sz[cat]||300e3)*(0.2+Math.random()*1.5)),category:cat,confidence:.6+Math.random()*.4,repaired:Math.random()<.1,real:false,thumbUrl:null,path:`C:/recovered/recovered_${String(done+1).padStart(6,'0')}.${ext}`});
      ST.counts.all++;if(ST.counts[cat]!==undefined)ST.counts[cat]++;ST.ec[ext]=(ST.ec[ext]||0)+1;
    }
    upProg(Math.min((step/50)*100,100),Math.ceil((100-step/50*100)*1.2)+'s',done);
    upCounts();upLive(done,ST.counts);
    if(step>=50){clearInterval(iv);ST.lastStats={filesFound:done,byType:ST.ec,byCategory:ST.counts};doneScan();}
  },90);
}

async function fetchFilesFromDisk(dir){
  if(!serverOn)return;
  try{
    const r=await fetch(SRV+'/api/files?dir='+encodeURIComponent(dir));
    const d=await r.json();
    if(d.files&&d.files.length>0){
      const ex=new Set(ST.files.map(f=>f.path));
      d.files.forEach(f=>{
        if(!ex.has(f.path)){
          ST.files.push({id:ST.files.length,name:f.name,ext:f.ext,size:f.size,path:f.path,category:f.category,confidence:.92,repaired:false,real:true,thumbUrl:f.thumbUrl||null});
          ST.counts.all++;if(ST.counts[f.category]!==undefined)ST.counts[f.category]++;
          ST.ec[f.ext]=(ST.ec[f.ext]||0)+1;
        }
      });
      upCounts();
    }
  }catch(e){console.error('fetchFilesFromDisk:',e);}
}

async function loadFromDisk(){
  const dir=(document.getElementById('outd').value||'C:\\recovered').trim();
  if(!serverOn){toast('info','📁','Your files are in C:\\test_output — open File Explorer!');return;}
  try{
    const r=await fetch(SRV+'/api/check-output?dir='+encodeURIComponent(dir));
    const d=await r.json();
    let bestDir=dir,bestCount=d.files||0;
    if(d.alternates)for(const a of d.alternates){if(a.count>bestCount){bestCount=a.count;bestDir=a.path;}}
    if(bestCount===0){toast('warn','📭','No recovered files found. Run a scan first!');return;}
    if(bestDir!==dir){toast('info','📁','Files found in '+bestDir+' ('+fn(bestCount)+')');document.getElementById('outd').value=bestDir.replace(/\//g,'\\');}
    ST.files=[];ST.counts={all:0,image:0,video:0,audio:0,document:0,archive:0,raw:0};ST.ec={};
    ST.currentDir=bestDir.replace(/\\/g,'/');
    await fetchFilesFromDisk(bestDir);
    sh('st-idle',0);sh('st-scan',0);sh('st-error',0);
    const res=document.getElementById('st-res');res.style.display='flex';res.style.flexDirection='column';
    document.getElementById('chipbar').style.display='flex';sh('abar',2);
    ST.cat='all';ST.ext='all';
    document.querySelectorAll('.si').forEach(b=>b.classList.remove('on'));document.getElementById('f-all').classList.add('on');
    document.querySelectorAll('.chip').forEach(c=>c.classList.remove('on'));document.querySelector('.chip').classList.add('on');
    upCounts();applyF();
    toast('ok','✅','Loaded '+fn(ST.counts.all)+' files!');
  }catch(e){toast('err','❌','Error: '+e.message);}
}

function doneScan(){
  ST.scanning=false;ST.xhr=null;ST.scanId=null;
  sh('st-scan',0);
  document.getElementById('scanFooter').style.display='none';
  const r=document.getElementById('st-res');r.style.display='flex';r.style.flexDirection='column';
  sh('abar',2);document.getElementById('chipbar').style.display='flex';
  document.getElementById('stopBtn').style.display='none';document.getElementById('scanBtn').disabled=false;
  document.getElementById('ss').textContent='Done';document.getElementById('se').textContent='—';
  ST.cat='all';ST.ext='all';document.getElementById('srch').value='';
  document.querySelectorAll('.si').forEach(b=>b.classList.remove('on'));document.getElementById('f-all').classList.add('on');
  document.querySelectorAll('.chip').forEach(c=>c.classList.remove('on'));document.querySelector('.chip').classList.add('on');
  upCounts();applyF();
  if(ST.counts.all>0&&serverOn)setTimeout(()=>aiAnalyze(true),3000);
}

async function stopScan(){
  if(ST.xhr){try{ST.xhr.abort();}catch{}ST.xhr=null;}
  if(serverOn&&ST.scanId){try{await fetch(SRV+'/api/stop',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({scanId:ST.scanId})});}catch{}}
  ST.scanning=false;ST.scanId=null;
  sh('st-scan',0);document.getElementById('scanFooter').style.display='none';
  document.getElementById('stopBtn').style.display='none';document.getElementById('scanBtn').disabled=false;
  ST.files.length>0?doneScan():sh('st-idle',1);
  toast('info','⏹','Scan stopped');
}

function toggleLiveView(){
  const scanEl=document.getElementById('st-scan');
  const hidden=scanEl.style.display==='none';
  scanEl.style.display=hidden?'flex':'none';
  if(hidden)scanEl.style.flexDirection='column',scanEl.style.alignItems='center',scanEl.style.justifyContent='center';
}

function showError(title,body){
  document.getElementById('errTitle').textContent=title;
  document.getElementById('errHead').textContent=title;
  document.getElementById('errBody').textContent=body;
}

// ═══════════════════════════════════════════════════════
// RENDER WITH REAL THUMBNAILS
// ═══════════════════════════════════════════════════════
function makeThumb(f){
  const isImg=IMGEXTS.has(f.ext);
  const isVid=VIDEXTS.has(f.ext);
  if(isImg&&f.thumbUrl){
    return`<div class="ftmb">
      <span class="icon-fallback">${gi(f.ext)}</span>
      <div class="loading-thumb" id="lt_${f.id}">⟳</div>
      <img class="real-thumb" src="${f.thumbUrl}" alt="" loading="lazy"
        onload="document.getElementById('lt_${f.id}')?.classList.add('hidden')"
        onerror="this.style.display='none';document.getElementById('lt_${f.id}')?.classList.add('hidden')">
      <div class="prev-hint"><div class="prev-hint-ic">⌕</div></div>
      <span class="fbdg">${f.ext.toUpperCase()}</span>
    </div>`;
  }
  if(isVid&&f.thumbUrl){
    return`<div class="ftmb">
      <span class="icon-fallback">${gi(f.ext)}</span>
      <div class="play-ov"><div class="play-ov-ic">▶</div></div>
      <span class="fbdg">${f.ext.toUpperCase()}</span>
    </div>`;
  }
  return`<div class="ftmb">
    <span class="icon-fallback">${gi(f.ext)}</span>
    ${isVid?'<div class="play-ov"><div class="play-ov-ic">▶</div></div>':''}
    <span class="fbdg">${f.ext.toUpperCase()}</span>
  </div>`;
}

function makeRowThumb(f){
  const isImg=IMGEXTS.has(f.ext);
  if(isImg&&f.thumbUrl){
    return`<div class="frow-thumb"><img src="${f.thumbUrl}" alt="" loading="lazy" onerror="this.style.display='none'"></div>`;
  }
  return`<div class="frow-thumb">${gi(f.ext)}</div>`;
}

// Append single card to grid (for live scan streaming)
function appendFileCard(f){
  const grid=document.getElementById('fg');
  if(!grid||ST.view!=='grid')return;
  grid.style.display='grid';
  document.getElementById('emp').style.display='none';
  const div=document.createElement('div');
  div.className='fcard';div.id='fc_'+f.id;
  div.innerHTML=`<div class="fchk"></div>${makeThumb(f)}<div class="finfo"><div class="fnm" title="${f.name}">${f.name}</div><div class="fmeta">${fz(f.size)}${f.repaired?' <span class="tag tag-fix">FIXED</span>':''}${f.real?' <span class="tag tag-real">REAL</span>':''}</div></div>`;
  div.onclick=()=>{tsel(f.id);};
  div.ondblclick=(e)=>{e.stopPropagation();openPrev(f.id);};
  grid.appendChild(div);
}

function applyF(){
  const q=document.getElementById('srch').value.toLowerCase();
  ST.filtered=ST.files.filter(f=>{
    if(ST.cat!=='all'&&f.category!==ST.cat)return false;
    if(ST.ext!=='all'&&f.ext!==ST.ext)return false;
    if(q&&!f.name.toLowerCase().includes(q))return false;
    return true;
  });
  renderF();upSelBar();
}

function renderF(){
  const grid=document.getElementById('fg'),emp=document.getElementById('emp');
  if(!ST.filtered.length){grid.innerHTML='';grid.style.display='none';emp.style.display='flex';return;}
  emp.style.display='none';
  const SHOW=Math.min(ST.filtered.length,300);
  if(ST.view==='grid'){
    grid.className='fg2';grid.style.display='grid';
    grid.innerHTML=ST.filtered.slice(0,SHOW).map(f=>{
      const sel=ST.sel.has(f.id);
      return`<div class="fcard${sel?' sel':''}" id="fc_${f.id}" onclick="tsel(${f.id})" ondblclick="openPrev(${f.id})">
        <div class="fchk">${sel?'✓':''}</div>
        ${makeThumb(f)}
        <div class="finfo"><div class="fnm" title="${f.name}">${f.name}</div>
        <div class="fmeta">${fz(f.size)}${f.repaired?' <span class="tag tag-fix">FIXED</span>':''}${f.real?' <span class="tag tag-real">REAL</span>':''}</div></div>
      </div>`;
    }).join('');
    if(ST.filtered.length>SHOW){
      const m=document.createElement('div');
      m.style.cssText='grid-column:1/-1;padding:12px;text-align:center;font-size:12px;color:var(--t2)';
      m.textContent='Showing '+SHOW.toLocaleString()+' of '+fn(ST.filtered.length)+' files. Use filters/search to narrow down.';
      grid.appendChild(m);
    }
  } else {
    grid.className='fls';grid.style.display='flex';
    grid.innerHTML=ST.filtered.slice(0,SHOW).map(f=>{
      const sel=ST.sel.has(f.id);
      return`<div class="frow${sel?' sel':''}" onclick="tsel(${f.id})" ondblclick="openPrev(${f.id})">
        <input type="checkbox" ${sel?'checked':''} style="accent-color:var(--cy);flex-shrink:0" onclick="event.stopPropagation();tsel(${f.id})">
        ${makeRowThumb(f)}
        <div class="fr-nm" title="${f.path}">${f.name}${f.repaired?' <span class="tag tag-fix">FIXED</span>':''}</div>
        <div class="fr-ext">.${f.ext.toUpperCase()}</div>
        <div class="fr-sz">${fz(f.size)}</div>
      </div>`;
    }).join('');
  }
  upActBar();
}

// ═══════════════════════════════════════════════════════
// PREVIEW LIGHTBOX
// ═══════════════════════════════════════════════════════
function openPrev(id){
  const idx=ST.filtered.findIndex(f=>f.id===id);
  if(idx<0)return;
  ST.prevIdx=idx;showPrev();
  document.getElementById('prevOvl').classList.add('open');
}
function showPrev(){
  const f=ST.filtered[ST.prevIdx];if(!f)return;
  const wrap=document.getElementById('prevWrap');
  const isImg=IMGEXTS.has(f.ext);
  const isVid=VIDEXTS.has(f.ext);
  if(isImg&&f.thumbUrl){
    wrap.innerHTML=`<img src="${f.thumbUrl}" alt="${f.name}">`;
  } else if(isVid&&f.thumbUrl){
    wrap.innerHTML=`<video src="${f.thumbUrl}" controls autoplay></video>`;
  } else {
    wrap.innerHTML=`<div style="font-size:80px;padding:60px;text-align:center;color:var(--t2)">${gi(f.ext)}<br><div style="font-size:14px;margin-top:16px;font-family:var(--fm)">${f.name}</div></div>`;
  }
  document.getElementById('prevName').textContent=f.name;
  document.getElementById('prevMeta').textContent=fz(f.size)+' · '+f.ext.toUpperCase()+(f.repaired?' · REPAIRED':'')+(f.real?' · REAL':'');
  document.getElementById('prevSelChk').checked=ST.sel.has(f.id);
}

function togglePrevSel(checked){
  const f=ST.filtered[ST.prevIdx];if(!f)return;
  checked?ST.sel.add(f.id):ST.sel.delete(f.id);
  const card=document.getElementById('fc_'+f.id);
  if(card){card.classList.toggle('sel',checked);const chk=card.querySelector('.fchk');if(chk)chk.textContent=checked?'✓':'';}
  upActBar();upSelBar();
}
function prevFile(){if(ST.prevIdx>0){ST.prevIdx--;showPrev();}}
function nextFile(){if(ST.prevIdx<ST.filtered.length-1){ST.prevIdx++;showPrev();}}
function closePrev(){document.getElementById('prevOvl').classList.remove('open');}

// ═══════════════════════════════════════════════════════
// SELECTION
// ═══════════════════════════════════════════════════════
function tsel(id){
  ST.sel.has(id)?ST.sel.delete(id):ST.sel.add(id);
  const el=document.getElementById('fc_'+id);
  if(el){el.classList.toggle('sel',ST.sel.has(id));el.querySelector('.fchk').textContent=ST.sel.has(id)?'✓':'';}
  upActBar();upSelBar();
}
function selAll(c){ST.filtered.forEach(f=>c?ST.sel.add(f.id):ST.sel.delete(f.id));renderF();upActBar();}
function clrSel(){ST.sel.clear();document.getElementById('saChk').checked=false;renderF();upActBar();}
function selByType(cat){ST.files.filter(f=>f.category===cat).forEach(f=>ST.sel.add(f.id));applyF();toast('info',CI[cat]||'📁','All '+cat+' selected');}
function upActBar(){
  const n=ST.sel.size,sz=[...ST.sel].reduce((a,id)=>{const f=ST.files.find(x=>x.id===id);return a+(f?f.size:0);},0);
  document.getElementById('selN').textContent=fn(n);document.getElementById('selSz').textContent=fz(sz);document.getElementById('recBtn').disabled=n===0;
}
function upSelBar(){
  const bar=document.getElementById('selbar');
  if(ST.filtered.length>0){bar.style.display='flex';document.getElementById('saN').textContent=fn(ST.filtered.length);document.getElementById('saChk').checked=ST.filtered.length>0&&ST.filtered.every(f=>ST.sel.has(f.id));}
  else bar.style.display='none';
}
function fcat(cat){ST.cat=cat;document.querySelectorAll('.si').forEach(b=>b.classList.remove('on'));document.getElementById('f-'+cat).classList.add('on');applyF();}
function fext(ext){ST.ext=ext;document.querySelectorAll('.chip').forEach(c=>c.classList.remove('on'));event.target.closest('.chip').classList.add('on');applyF();}
function sv(v){ST.view=v;document.getElementById('vg').classList.toggle('on',v==='grid');document.getElementById('vl').classList.toggle('on',v==='list');renderF();}

// ═══════════════════════════════════════════════════════
// REAL RECOVERY — copies + repairs files
// ═══════════════════════════════════════════════════════
function om(id){
  if(id==='recModal'){
    const sz=[...ST.sel].reduce((a,i)=>{const f=ST.files.find(x=>x.id===i);return a+(f?f.size:0);},0);
    document.getElementById('recSub').textContent=fn(ST.sel.size)+' files ('+fz(sz)+')';
  }
  document.getElementById(id).classList.add('open');
}
function cm(id){document.getElementById(id).classList.remove('open');}

function doRecover(){
  const total=ST.sel.size;
  const destDir=(document.getElementById('recPath').value||'C:\\MyRecoveredFiles').trim();
  const doRepair=document.getElementById('rr')?document.getElementById('rr').checked:true;
  if(total===0){toast('warn','⚠️','Select files first!');return;}
  cm('recModal');
  document.getElementById('recTotal').textContent=fn(total);
  document.getElementById('recDone').textContent='0';
  document.getElementById('progSub').textContent='Recovering '+fn(total)+' files...';
  document.getElementById('repCount').style.display='none';
  document.getElementById('curFile').textContent='';
  om('progModal');
  ['pd1','pd2','pd3','pd4'].forEach(d=>document.getElementById(d).className='pd');
  pd('pd1','act');
  const selFiles=[...ST.sel].map(id=>ST.files.find(f=>f.id===id)).filter(Boolean);
  if(serverOn&&selFiles.some(f=>f.real)){realRecover(selFiles,destDir,doRepair,total);}
  else{demoRecover(total,destDir);}
}

function realRecover(files,destDir,doRepair,total){
  const body={files:files.map(f=>({name:f.name,path:f.path,ext:f.ext})),outputDir:destDir,repair:doRepair};
  let done=0,repaired=0;
  fetch(SRV+'/api/recover',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
  .then(res=>{
    const reader=res.body.getReader(),dec=new TextDecoder();let buf='';
    const rd=()=>reader.read().then(({done:sd,value})=>{
      if(sd)return;buf+=dec.decode(value,{stream:true});
      const lines=buf.split('\n');buf=lines.pop();
      lines.filter(l=>l.startsWith('data:')).forEach(l=>{
        try{
          const e=JSON.parse(l.slice(5));
          if(e.type==='progress'){
            done=e.done;const pct=done/total*100;
            document.getElementById('ringFl').style.strokeDashoffset=190-(190*pct/100);
            document.getElementById('ringPct').textContent=Math.round(pct)+'%';
            document.getElementById('recDone').textContent=fn(done);
            document.getElementById('curFile').textContent=e.file||'';
            if(e.repaired)repaired++;
            if(done>=total*.25){pd('pd1','done');pd('pd2','act');}
            if(done>=total*.6) {pd('pd2','done');pd('pd3','act');}
            if(done>=total*.9) {pd('pd3','done');pd('pd4','act');}
          }
          if(e.type==='done'){
            pd('pd4','done');
            if(e.repaired>0){const rc=document.getElementById('repCount');rc.textContent='✅ '+fn(e.repaired)+' files auto-repaired';rc.style.display='block';}
            setTimeout(()=>{
              cm('progModal');
              document.getElementById('doneN').textContent=fn(e.done||total);
              document.getElementById('donePath').textContent=destDir;
              const dr=document.getElementById('doneRep');
              dr.textContent=e.repaired>0?'✅ '+fn(e.repaired)+' corrupted files were auto-repaired':'';
              om('doneModal');
              toast('ok','🎉',fn(e.done||total)+' files recovered to '+destDir);
            },500);
          }
          if(e.type==='error')toast('err','❌',e.message);
        }catch{}
      });rd();
    }).catch(e=>{toast('err','❌','Recovery error: '+e.message);cm('progModal');});
    rd();
  }).catch(e=>{toast('err','❌','Server error: '+e.message);cm('progModal');});
}

function demoRecover(total,destDir){
  const DUR=Math.min(total*50,4000);let done=0;
  setTimeout(()=>pd('pd1','act'),100);
  setTimeout(()=>{pd('pd1','done');pd('pd2','act');},DUR*.3);
  setTimeout(()=>{pd('pd2','done');pd('pd3','act');},DUR*.6);
  setTimeout(()=>{pd('pd3','done');pd('pd4','act');},DUR*.85);
  const iv=setInterval(()=>{
    done+=Math.ceil(total/25);done=Math.min(done,total);
    const pct=done/total*100;
    document.getElementById('ringFl').style.strokeDashoffset=190-(190*pct/100);
    document.getElementById('ringPct').textContent=Math.round(pct)+'%';
    document.getElementById('recDone').textContent=fn(done);
    if(done>=total){clearInterval(iv);pd('pd4','done');
      setTimeout(()=>{cm('progModal');document.getElementById('doneN').textContent=fn(total);document.getElementById('donePath').textContent=destDir;om('doneModal');toast('ok','🎉',fn(total)+' files recovered!');},400);}
  },DUR/25);
}
function pd(n,s){document.getElementById(n).className='pd'+(s==='act'?' act':s==='done'?' done':s==='err'?' err':'');}

// ═══════════════════════════════════════════════════════
// AI
// ═══════════════════════════════════════════════════════
function toggleAI(){aiOpen=!aiOpen;document.getElementById('aiPanel').classList.toggle('open',aiOpen);}
async function ask(msg){
  if(!msg)return;if(!aiOpen)toggleAI();
  const box=document.getElementById('aiMsgs');
  const um=document.createElement('div');um.className='am user';um.textContent=msg;box.appendChild(um);
  const lm=document.createElement('div');lm.className='am bot loading';lm.textContent='Thinking...';box.appendChild(lm);
  box.scrollTop=box.scrollHeight;document.getElementById('aiInp').value='';
  if(!serverOn){lm.textContent='AI requires node server.js to be running.\n\nKey tip: Use USB 3.0 (blue) port for 4x faster scanning. Always run as Administrator.';lm.classList.remove('loading');return;}
  try{
    const r=await fetch(SRV+'/api/ai/chat',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({message:msg,context:{filesFound:ST.counts.all,device:document.getElementById('drv')?.value,byCategory:ST.counts}})});
    const d=await r.json();lm.textContent=d.reply||'No response.';
  }catch{lm.textContent='Connection error. Is the server running?';}
  lm.classList.remove('loading');box.scrollTop=box.scrollHeight;
}
function sendAI(){const v=document.getElementById('aiInp').value.trim();if(v)ask(v);}
async function getAISuggest(){
  const card=document.getElementById('aiCard');
  card.innerHTML='<div style="padding:9px;font-size:11px;color:var(--t2)">✦ Getting recommendations...</div>';
  card.style.display='block';
  if(!serverOn){card.innerHTML=`<div class="card" style="background:rgba(167,139,250,.06);border-color:rgba(167,139,250,.22)"><div style="font-size:11px;color:var(--pur);font-weight:600;margin-bottom:5px">✦ Recommended Settings</div><div style="font-size:11px;color:var(--t2);line-height:1.7">Deep scan + auto-repair (always on).<br>⏱ 60-90 min USB 2.0 · 15-25 min USB 3.0<br>→ Use blue USB 3.0 port for 4x speed<br>→ Run as Administrator (required)<br>→ Don't write to drive before scanning</div><button class="btn b-cy b-sm" style="width:100%;justify-content:center;margin-top:8px" onclick="doScan()">🔍 Start Scan</button></div>`;return;}
  try{
    const r=await fetch(SRV+'/api/ai/suggest',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({device:document.getElementById('drv')?.value,driveType:'USB',goal:'Recover all deleted photos and videos'})});
    const d=await r.json();
    if(d.settings){document.getElementById('od').checked=d.settings.deep??true;document.getElementById('om').checked=d.settings.metadata??false;}
    card.innerHTML=`<div class="card" style="background:rgba(167,139,250,.06);border-color:rgba(167,139,250,.22)"><div style="font-size:11px;color:var(--pur);font-weight:600;margin-bottom:5px">✦ AI Recommendation</div><div style="font-size:11px;color:var(--t2);line-height:1.6;margin-bottom:5px">${d.recommendation||''}</div><div style="font-size:10px;color:var(--t3);margin-bottom:5px">Est: ${d.expectedTime||'varies'}</div>${(d.tips||[]).map(t=>`<div style="font-size:10px;color:var(--t2);padding:2px 0">→ ${t}</div>`).join('')}<button class="btn b-cy b-sm" style="width:100%;justify-content:center;margin-top:8px" onclick="doScan()">🔍 Start Scan</button></div>`;
    toast('ok','✦','AI recommendations ready!');
  }catch{card.innerHTML='<div style="font-size:11px;color:var(--t2);padding:8px">Could not load recommendations.</div>';}
}
async function aiAnalyze(silent=false){
  if(!ST.lastStats&&!silent){toast('info','💡','Run a scan first');return;}
  await ask('Analyze my scan results:\n'+JSON.stringify(ST.lastStats,null,2)+'\n\nGive me:\n1. Overall assessment\n2. Which files are most likely intact\n3. Exact next steps');
}
function aiAfter(){cm('doneModal');if(!aiOpen)toggleAI();aiAnalyze(false);}

// ═══════════════════════════════════════════════════════
// NAV & UTILS
// ═══════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════
// NAV & UTILS
// ═══════════════════════════════════════════════════════
function gp(name){
  document.querySelectorAll('.pg').forEach(p=>p.classList.remove('on'));
  document.querySelectorAll('.nbtn').forEach(b=>b.classList.remove('on'));
  const pg=document.getElementById('pg-'+name);if(pg)pg.classList.add('on');
  const nb=document.getElementById('n-'+name);if(nb)nb.classList.add('on');
  if(name==='history')loadHistory();
}

// ═══════════════════════════════════════════════════════
// AUTHENTICATION & DATABASE HISTORY
// ═══════════════════════════════════════════════════════
async function checkAuth(){
  if(!authToken){renderAuthBar();return;}
  try{
    const res=await fetch(SRV+'/api/auth/me',{headers:getAuthHeaders()});
    if(res.ok){
      const data=await res.json();
      if(data.user){currentUser=data.user;}
      else{currentUser=null;authToken=null;localStorage.removeItem('nr_token');}
    }
  }catch(_){}
  renderAuthBar();
}

function renderAuthBar(){
  const wrap=document.getElementById('authNavWrap');
  if(!wrap)return;
  if(currentUser){
    wrap.innerHTML=`
      <div class="user-pill" title="${currentUser.email}">
        <div class="user-avatar">${currentUser.username.slice(0,1).toUpperCase()}</div>
        <span>${currentUser.username}</span>
      </div>
      <button class="btn b-ghost b-sm" onclick="logoutUser()" title="Log Out">Log Out</button>
    `;
  }else{
    wrap.innerHTML=`
      <button class="btn b-sec b-sm" onclick="openAuthModal('login')">🔑 Log In</button>
    `;
  }
}

function openAuthModal(mode='login'){
  switchAuthTab(mode);
  const errBox=document.getElementById('authErr');
  if(errBox)errBox.style.display='none';
  om('authModal');
}

function switchAuthTab(tab){
  const tL=document.getElementById('tabLogin');
  const tS=document.getElementById('tabSignup');
  const fL=document.getElementById('formLogin');
  const fS=document.getElementById('formSignup');
  const err=document.getElementById('authErr');
  if(err)err.style.display='none';
  if(tab==='login'){
    tL.classList.add('on');tS.classList.remove('on');
    fL.style.display='block';fS.style.display='none';
  }else{
    tS.classList.add('on');tL.classList.remove('on');
    fS.style.display='block';fL.style.display='none';
  }
}

async function submitLogin(){
  const identifier=document.getElementById('loginId').value.trim();
  const password=document.getElementById('loginPw').value;
  const errBox=document.getElementById('authErr');
  const btn=document.getElementById('btnLoginSub');
  btn.disabled=true;
  try{
    const res=await fetch(SRV+'/api/auth/login',{
      method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({identifier,password})
    });
    const data=await res.json();
    if(!res.ok||!data.success)throw new Error(data.error||'Login failed');
    currentUser=data.user;
    authToken=data.token;
    localStorage.setItem('nr_token',authToken);
    renderAuthBar();
    cm('authModal');
    toast('ok','👋',`Welcome back, ${currentUser.username}!`);
  }catch(e){
    errBox.textContent=e.message;
    errBox.style.display='block';
  }finally{
    btn.disabled=false;
  }
}

async function submitSignup(){
  const username=document.getElementById('regUser').value.trim();
  const email=document.getElementById('regEmail').value.trim();
  const password=document.getElementById('regPw').value;
  const errBox=document.getElementById('authErr');
  const btn=document.getElementById('btnSignupSub');
  btn.disabled=true;
  try{
    const res=await fetch(SRV+'/api/auth/signup',{
      method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({username,email,password})
    });
    const data=await res.json();
    if(!res.ok||!data.success)throw new Error(data.error||'Registration failed');
    currentUser=data.user;
    authToken=data.token;
    localStorage.setItem('nr_token',authToken);
    renderAuthBar();
    cm('authModal');
    toast('ok','🎉',`Account created! Welcome, ${currentUser.username}`);
  }catch(e){
    errBox.textContent=e.message;
    errBox.style.display='block';
  }finally{
    btn.disabled=false;
  }
}

async function logoutUser(){
  if(authToken){
    try{
      await fetch(SRV+'/api/auth/logout',{
        method:'POST',
        headers:getAuthHeaders()
      });
    }catch(_){}
  }
  currentUser=null;
  authToken=null;
  localStorage.removeItem('nr_token');
  renderAuthBar();
  toast('info','🔒','Logged out successfully');
}

async function loadHistory(){
  if(!serverOn)return;
  const tbody=document.getElementById('histTbody');
  const table=document.getElementById('histTable');
  const empty=document.getElementById('histEmpty');
  const scCount=document.getElementById('histTotalScans');
  const flCount=document.getElementById('histTotalFiles');
  try{
    const res=await fetch(SRV+'/api/history/scans',{headers:getAuthHeaders()});
    const data=await res.json();
    if(!data.success||!data.scans||!data.scans.length){
      table.style.display='none';
      empty.style.display='flex';
      if(scCount)scCount.textContent='0';
      if(flCount)flCount.textContent='0';
      return;
    }
    empty.style.display='none';
    table.style.display='table';
    let totalFiles=0;
    data.scans.forEach(s=>totalFiles+=(s.total_files||0));
    if(scCount)scCount.textContent=data.scans.length;
    if(flCount)flCount.textContent=totalFiles.toLocaleString();

    tbody.innerHTML=data.scans.map(s=>{
      const stClass=s.status==='completed'?'st-comp':(s.status==='running'?'st-run':'st-stop');
      const dateStr=s.started_at?new Date(s.started_at).toLocaleString():'—';
      return `
        <tr>
          <td><span class="badge-status ${stClass}">${(s.status||'DONE').toUpperCase()}</span></td>
          <td><strong>${s.device_path}</strong></td>
          <td style="color:var(--t2);font-family:var(--fm);font-size:11px">${dateStr}</td>
          <td><strong style="color:var(--cy)">${s.total_files||0}</strong> files</td>
          <td style="color:var(--t2);font-family:var(--fm);font-size:11px">${s.output_dir}</td>
          <td>
            <div style="display:flex;gap:6px">
              <button class="btn b-sec b-sm" onclick="viewScanDetails('${s.id}')">👁 Files</button>
              <button class="btn b-ghost b-sm" onclick="deleteScan('${s.id}')" title="Delete record">🗑</button>
            </div>
          </td>
        </tr>
      `;
    }).join('');
  }catch(e){
    console.error('Failed to load history:',e);
  }
}

async function viewScanDetails(scanId){
  try{
    const res=await fetch(SRV+`/api/history/scans/${scanId}`,{headers:getAuthHeaders()});
    const data=await res.json();
    if(!data.success||!data.scan)return;
    document.getElementById('smTitle').textContent=`Scan: ${data.scan.device_path}`;
    document.getElementById('smSub').textContent=`Started: ${new Date(data.scan.started_at).toLocaleString()} · ${data.files.length} files saved in DB`;
    const list=document.getElementById('smFileList');
    if(!data.files.length){
      list.innerHTML='<div style="padding:20px;text-align:center;color:var(--t2)">No individual file records recorded for this session.</div>';
    }else{
      list.innerHTML=`
        <div style="font-size:12px;margin-bottom:8px;color:var(--t2)">Files carved during this session:</div>
        <div style="display:flex;flex-direction:column;gap:4px">
          ${data.files.map(f=>`
            <div style="display:flex;align-items:center;gap:10px;padding:6px 10px;background:var(--bg3);border-radius:var(--r);font-size:12px">
              <span style="font-size:14px">${gi(f.extension)}</span>
              <span style="flex:1;font-weight:500;overflow:hidden;text-overflow:ellipsis;white-space:nowrap" title="${f.file_path}">${f.file_name}</span>
              <span style="font-family:var(--fm);color:var(--t2);font-size:10px">${fz(f.size_bytes)}</span>
              ${f.is_repaired?'<span class="tag tag-fix" style="font-size:9px">FIXED</span>':''}
            </div>
          `).join('')}
        </div>
      `;
    }
    om('scanModal');
  }catch(e){
    toast('err','❌','Could not load scan details');
  }
}

async function deleteScan(scanId){
  if(!confirm('Are you sure you want to remove this scan session from the database? (Files on disk will remain untouched)'))return;
  try{
    await fetch(SRV+`/api/history/scans/${scanId}`,{
      method:'DELETE',
      headers:getAuthHeaders()
    });
    toast('info','🗑','Scan record deleted');
    loadHistory();
  }catch(_){}
}

function sh(id,v){
  const e=document.getElementById(id);if(!e)return;
  if(!v){e.style.display='none';return;}
  if(id==='st-scan'){e.style.display='flex';e.style.flexDirection='column';e.style.alignItems='center';e.style.justifyContent='center';}
  else if(id==='st-res'){e.style.display='flex';e.style.flexDirection='column';}
  else if(v===2){e.style.display='flex';}
  else e.style.display='block';
}
function toast(type,icon,text){
  const t=document.createElement('div');
  t.className='toast '+(type==='ok'?'ok':type==='err'?'err':type==='warn'?'warn':'info');
  t.innerHTML=`<span class="t-ic">${icon}</span><span style="flex:1">${text}</span>`;
  document.getElementById('tbox').appendChild(t);
  setTimeout(()=>{t.style.opacity='0';t.style.transform='translateX(110%)';t.style.transition='all .28s';setTimeout(()=>t.remove(),280);},5000);
}
document.addEventListener('keydown',e=>{
  if(e.key==='Escape'){['recModal','progModal','doneModal','buildModal','authModal','scanModal'].forEach(cm);closePrev();if(aiOpen){aiOpen=false;document.getElementById('aiPanel').classList.remove('open');}}
  if(e.key==='ArrowLeft'&&document.getElementById('prevOvl').classList.contains('open'))prevFile();
  if(e.key==='ArrowRight'&&document.getElementById('prevOvl').classList.contains('open'))nextFile();
  if((e.ctrlKey||e.metaKey)&&e.key==='a'&&ST.filtered.length){e.preventDefault();selAll(true);}
  if((e.ctrlKey||e.metaKey)&&e.key==='f'){e.preventDefault();document.getElementById('srch')?.focus();}
});
document.addEventListener('DOMContentLoaded',()=>{gp('home');init();});