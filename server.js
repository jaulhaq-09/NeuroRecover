/**
 * NeuroRecover AI — Server v5.1 FINAL
 *
 * ALL FIXES:
 *  ✅ Serve real file thumbnails (/api/thumb)
 *  ✅ /api/recover actually copies + repairs files
 *  ✅ Output path works with both / and \
 *  ✅ Sleep prevention during scan
 *  ✅ Admin detection
 *  ✅ Direct g++.exe build
 *  ✅ Smart output folder search
 *  ✅ Claude AI integration
 *
 * RUN AS ADMINISTRATOR:
 *   Right-click PowerShell → Run as Administrator
 *   cd C:\Users\mrove\OneDrive\Desktop\NeuroRecover
 *   node server.js
 */

const express  = require('express');
const { spawn, execSync, spawnSync } = require('child_process');
const path     = require('path');
const fs       = require('fs');
const crypto   = require('crypto');
const db       = require('./db');

const app  = express();
const PORT = 3000;
const ROOT = __dirname;
const HTML = path.join(ROOT, 'index.html');

// ── Paths ─────────────────────────────────────────────────────────────────────
const EXE_LOCS = [
  path.join(ROOT,'build','neurorecover.exe'),
  path.join(ROOT,'neurorecover.exe'),
];
const GCC_LOCS = [
  'C:\\msys64\\mingw64\\bin\\g++.exe',
  'C:\\msys2\\mingw64\\bin\\g++.exe',
  'C:\\mingw64\\bin\\g++.exe',
];
const BASH_LOCS = [
  'C:\\msys64\\usr\\bin\\bash.exe',
  'C:\\msys2\\usr\\bin\\bash.exe',
];
const DEFAULT_OUT = path.join(process.env.USERPROFILE || 'C:\\', 'RecoveredFiles');
const SEARCH_DIRS = [
  DEFAULT_OUT,
  'C:/test_output','C:/recovered','C:/recovered2',
  'C:/out','C:/neurorecover_output',
];

const findExe  = () => EXE_LOCS.find(fs.existsSync) || EXE_LOCS[0];
const findGcc  = () => GCC_LOCS.find(fs.existsSync);
const findBash = () => BASH_LOCS.find(fs.existsSync);
const exeOk    = () => fs.existsSync(findExe());
const normPath = p  => (p||'').replace(/\\/g,'/').trim();
const winPath  = p  => (p||'').replace(/\//g,'\\').trim();

// ── Admin check ───────────────────────────────────────────────────────────────
function isAdmin() {
  if (process.platform !== 'win32') return true;
  try { execSync('net session >nul 2>&1',{stdio:'ignore'}); return true; }
  catch { return false; }
}
const ADMIN = isAdmin();

// ── Sleep prevention ──────────────────────────────────────────────────────────
let powerProc = null;
function preventSleep() {
  if (process.platform!=='win32'||powerProc) return;
  try {
    const ps = [
      'Add-Type -TypeDefinition "using System;using System.Runtime.InteropServices;',
      'public class Power{[DllImport(\\"kernel32.dll\\")]',
      'public static extern uint SetThreadExecutionState(uint f);}" ;',
      'Write-Host "SLEEP_OK";',
      'while($true){[Power]::SetThreadExecutionState(0x80000001)|Out-Null;Start-Sleep -Seconds 30}',
    ].join('');
    powerProc = spawn('powershell.exe',['-NoProfile','-NonInteractive','-Command',ps],
      {stdio:['ignore','pipe','ignore'],windowsHide:true});
    powerProc.stdout.on('data',d=>{if(d.toString().includes('SLEEP_OK'))console.log('☀️  Sleep prevented');});
    powerProc.on('exit',()=>{powerProc=null;});
  } catch(e){console.log('⚠ Sleep prevention:',e.message);}
}
function allowSleep() {
  if (!powerProc) return;
  try { powerProc.kill(); } catch {}
  powerProc = null;
  console.log('🌙 Sleep allowed');
}

// ── Startup ───────────────────────────────────────────────────────────────────
console.log('\n╔══════════════════════════════════════════════════╗');
console.log('║    NeuroRecover AI  v5.1  —  Starting           ║');
console.log('╠══════════════════════════════════════════════════╣');
console.log(`║  URL   : http://localhost:${PORT}                     ║`);
console.log(`║  Admin : ${(ADMIN?'✅ Yes':'❌ NO — RESTART AS ADMIN!').padEnd(42)}║`);
console.log(`║  HTML  : ${(fs.existsSync(HTML)?'✅ Found':'❌ Missing').padEnd(42)}║`);
console.log(`║  EXE   : ${(exeOk()?'✅ Ready':'⚠  Not built').padEnd(42)}║`);
console.log(`║  g++   : ${(findGcc()?'✅ Found':'⚠  MSYS2 not found').padEnd(42)}║`);
console.log('╚══════════════════════════════════════════════════╝\n');
if(!ADMIN) console.log('⚠️  CRITICAL: Restart PowerShell as Administrator!\n');

// ── Middleware ────────────────────────────────────────────────────────────────
app.use(express.json({limit:'50mb'}));
app.use((req,res,next)=>{
  res.setHeader('Access-Control-Allow-Origin','*');
  res.setHeader('Access-Control-Allow-Methods','GET,POST,OPTIONS');
  res.setHeader('Access-Control-Allow-Headers','Content-Type,Authorization');
  if(req.method==='OPTIONS')return res.sendStatus(200);
  const auth = req.headers['authorization'];
  const token = auth && auth.startsWith('Bearer ') ? auth.slice(7) : (req.query.token || null);
  req.user = db.getUserByToken(token);
  next();
});
app.get('/',(req,res)=>{
  if(fs.existsSync(HTML))return res.sendFile(HTML);
  res.send(`<html><body style="background:#06080C;color:#F0F6FC;font-family:monospace;padding:40px">
    <h2 style="color:#00D9F5">NeuroRecover Running ✅</h2>
    <p style="color:#F14C4C">⚠ index.html missing from: ${ROOT}</p>
    <p style="color:#8B949E">${fs.readdirSync(ROOT).filter(f=>!f.startsWith('.')).join(', ')}</p>
  </body></html>`);
});
app.use(express.static(ROOT));

// ── SSE helper ────────────────────────────────────────────────────────────────
const sseOpen = res => res.writeHead(200,{
  'Content-Type':'text/event-stream','Cache-Control':'no-cache',
  'Connection':'keep-alive','X-Accel-Buffering':'no',
});
const sseSend = res => obj => { try{res.write('data:'+JSON.stringify(obj)+'\n\n');}catch(_){} };

function getSystemDrives() {
  const drives = [];
  if (process.platform === 'win32') {
    try {
      const psScript = `
$out = @()
try {
  $disks = Get-CimInstance Win32_DiskDrive -ErrorAction Stop
  foreach ($d in $disks) {
    $id = $d.DeviceID -replace '^\\\\\\.\\\\',''
    $isUSB = ($d.InterfaceType -like '*USB*' -or $d.MediaType -like '*Removable*')
    $isSys = ($id.ToUpper() -eq 'PHYSICALDRIVE0')
    $cap = if ($d.Caption) { $d.Caption } else { $id }
    $out += [PSCustomObject]@{
      caption = $cap
      deviceId = '//./' + $id
      size = [int64]$d.Size
      isUSB = [bool]$isUSB
      isSystem = [bool]$isSys
      recommended = [bool]($isUSB -and -not $isSys)
    }
  }
} catch {}
try {
  $vols = [System.IO.DriveInfo]::GetDrives()
  foreach ($v in $vols) {
    if ($v.IsReady) {
      $letter = $v.Name.Substring(0,1)
      $isUSB = ($v.DriveType.ToString() -eq 'Removable')
      $isSys = ($letter.ToUpper() -eq 'C')
      $lbl = if ($v.VolumeLabel) { "$($v.VolumeLabel) ($($v.Name))" } else { "Volume ($($v.Name))" }
      $out += [PSCustomObject]@{
        caption = $lbl
        deviceId = '//./' + $letter + ':'
        size = [int64]$v.TotalSize
        isUSB = [bool]$isUSB
        isSystem = [bool]$isSys
        recommended = [bool]($isUSB -and -not $isSys)
      }
    }
  }
} catch {}
$out | ConvertTo-Json -Compress
`;
      const b64 = Buffer.from(psScript, 'utf16le').toString('base64');
      const res = spawnSync('powershell.exe', ['-NoProfile', '-NonInteractive', '-EncodedCommand', b64], {
        timeout: 10000,
        encoding: 'utf8',
        windowsHide: true,
      });

      if (res.stdout && res.stdout.trim()) {
        const parsed = JSON.parse(res.stdout.trim());
        const list = Array.isArray(parsed) ? parsed : (parsed ? [parsed] : []);
        const seen = new Set();
        for (const item of list) {
          if (!item || !item.deviceId || seen.has(item.deviceId)) continue;
          seen.add(item.deviceId);
          drives.push({
            caption: item.caption || item.deviceId,
            deviceId: item.deviceId,
            size: item.size || 0,
            sizeStr: fmtSize(item.size || 0),
            isUSB: !!item.isUSB,
            isSystem: !!item.isSystem,
            recommended: !!item.recommended,
          });
        }
      }
    } catch (_) {}
  }

  // Ensure default physical drives are present as presets if not detected
  if (!drives.some(d => /physicaldrive/i.test(d.deviceId))) {
    drives.unshift(
      { caption: 'PhysicalDrive0 (System Disk)', deviceId: '//./PhysicalDrive0', size: 0, sizeStr: '?', isUSB: false, isSystem: true, recommended: false },
      { caption: 'PhysicalDrive1 (USB/SD Card)', deviceId: '//./PhysicalDrive1', size: 0, sizeStr: '?', isUSB: true, isSystem: false, recommended: true }
    );
  }
  return drives;
}

// ── GET /api/status ───────────────────────────────────────────────────────────
app.get('/api/status',(req,res)=>res.json({
  ok:true,version:'5.1.0',isAdmin:isAdmin(),
  exeFound:exeOk(),exePath:findExe(),
  gccFound:!!findGcc(),msys2Found:!!findBash(),root:ROOT,
  defaultOutputDir:DEFAULT_OUT,
}));

// ── GET /api/drives ───────────────────────────────────────────────────────────
app.get('/api/drives',(req,res)=>{
  const drives = getSystemDrives();
  res.json({drives,exeFound:exeOk(),isAdmin:isAdmin(),defaultOutputDir:DEFAULT_OUT});
});

// ── GET /api/thumb — serve actual recovered file as thumbnail ─────────────────
app.get('/api/thumb',(req,res)=>{
  const {file,dir} = req.query;
  if(!file||!dir){return res.status(400).end();}
  // Security: only serve files within the scan output directories
  const fullPath = path.resolve(dir.replace(/\//g,'\\'), file);
  const baseDir  = path.resolve(dir.replace(/\//g,'\\'));
  if(!fullPath.startsWith(baseDir)){return res.status(403).end();}
  if(!fs.existsSync(fullPath)){return res.status(404).end();}
  const ext = path.extname(fullPath).slice(1).toLowerCase();
  const mimes = {jpg:'image/jpeg',jpeg:'image/jpeg',png:'image/png',gif:'image/gif',bmp:'image/bmp',webp:'image/webp',heic:'image/heic'};
  if(mimes[ext]){
    res.setHeader('Content-Type',mimes[ext]);
    res.setHeader('Cache-Control','public, max-age=3600');
    fs.createReadStream(fullPath).pipe(res);
  } else {
    res.status(415).end();
  }
});

// ── POST /api/build ───────────────────────────────────────────────────────────
const SRC = [
  'cli/main.cpp','utils/logger.cpp','utils/progress.cpp','utils/hash.cpp','utils/dedup.cpp',
  'utils/config.cpp','utils/thread_pool.cpp','core/scanner/scanner.cpp','core/scanner/deep_scanner.cpp',
  'core/carving/file_signatures.cpp','core/carving/carver.cpp','core/carving/fragment_stitcher.cpp',
  'core/ai/ai_engine.cpp','core/filesystem/fs_analyzer.cpp','core/filesystem/ntfs_parser.cpp',
  'core/filesystem/fat_parser.cpp','core/filesystem/ext_parser.cpp',
  'core/reconstruct/jpeg_repair.cpp','core/reconstruct/png_repair.cpp',
  'core/reconstruct/video_repair.cpp','core/reconstruct/bmp_repair.cpp',
  'core/reconstruct/reconstructor.cpp','core/metadata/metadata_extractor.cpp',
  'core/preview/preview_generator.cpp','core/checkpoint/checkpoint_manager.cpp',
  'core/encryption/encryption_detector.cpp','core/filter/smart_filter.cpp',
  'core/sqlite_carver/sqlite_carver.cpp','core/recovery_engine.cpp',
].map(f=>path.join(ROOT,f));

app.post('/api/build',(req,res)=>{
  sseOpen(res); const send=sseSend(res);
  const gcc=findGcc(),bash=findBash();
  if(!gcc&&!bash){
    send({type:'error',message:'❌ g++ not found!\n\nInstall MSYS2 from https://www.msys2.org\nThen in MSYS2 MINGW64:\n  pacman -S mingw-w64-x86_64-gcc\n\nOR build manually:\n  Open MSYS2 MINGW64 as Admin\n  ./build_windows.sh'});
    return res.end();
  }
  const missing=SRC.filter(f=>!fs.existsSync(f));
  if(missing.length>3){send({type:'error',message:`❌ Missing ${missing.length} C++ source files.\nCheck that cli/, core/, utils/ folders exist in:\n${ROOT}`});return res.end();}
  fs.mkdirSync(path.join(ROOT,'build'),{recursive:true});
  const outExe=path.join(ROOT,'build','neurorecover.exe');
  send({type:'log',message:'Compiler: '+(gcc||'bash')});
  send({type:'log',message:'Output: '+outExe});
  send({type:'progress',percent:5});
  let pct=5,errors=0;
  const pt=setInterval(()=>{if(pct<88)pct+=1.2;send({type:'progress',percent:Math.round(pct)});},500);
  const env=Object.assign({},process.env,{PATH:(gcc?path.dirname(gcc)+';':'')+(process.env.PATH||'')});
  let proc;
  if(gcc){
    proc=spawn(gcc,['-std=c++17','-O3','-I'+ROOT,...SRC,'-o',outExe,
      '-static','-static-libgcc','-static-libstdc++','-lpthread','-lstdc++fs','-lws2_32','-lwinpthread'],
      {env,stdio:['ignore','pipe','pipe']});
  } else {
    const wr=ROOT.replace(/\\/g,'/').replace(/^([A-Za-z]):/,(_,d)=>'/'+d.toLowerCase());
    proc=spawn(bash,['--norc','--noprofile','-c',
      `export PATH="/mingw64/bin:/usr/bin:/bin:$PATH"; cd "${wr}"; ./build_windows.sh 2>&1`],
      {env:Object.assign({},env,{MSYSTEM:'MINGW64'}),stdio:['ignore','pipe','pipe']});
  }
  const onData=d=>d.toString().split('\n').filter(Boolean).forEach(msg=>{
    if(msg.includes('cannot set terminal')||msg.includes('no job control'))return;
    const isErr=msg.toLowerCase().includes('error:');if(isErr)errors++;
    send({type:'log',message:msg.trim(),isError:isErr});
  });
  proc.stdout.on('data',onData);proc.stderr.on('data',onData);
  proc.on('close',()=>{
    clearInterval(pt);send({type:'progress',percent:100});
    const ok=fs.existsSync(outExe)&&errors===0;
    send({type:'done',success:ok,message:ok
      ?`✅ Build complete! neurorecover.exe (${fmtSize(fs.statSync(outExe).size)}) is ready.`
      :`❌ Build failed (${errors} errors).\n\nFix: Open MSYS2 MINGW64 as Admin → ./build_windows.sh`});
    res.end();
  });
  proc.on('error',e=>{clearInterval(pt);send({type:'error',message:'Compiler error: '+e.message});res.end();});
});

// ── Active scans ──────────────────────────────────────────────────────────────
const scans = {};

// ── POST /api/scan ────────────────────────────────────────────────────────────
app.post('/api/scan',(req,res)=>{
  sseOpen(res); const send=sseSend(res);
  const {device,outputDir,types,deep,metadata,preview,checkpoint,resume,sqliteDeep} = req.body||{};

  if(!device){send({type:'error',message:'No device selected'});return res.end();}

  const isPhysical = /^((\/\/|\x5c\x5c)\.[\/\\]|physicaldrive|[a-z]:$)/i.test(device.trim());
  const isRegularFile = fs.existsSync(device);

  if(isPhysical && !isRegularFile && !isAdmin()){
    send({type:'need_admin',message:'❌ Not running as Administrator!\n\nRaw disk access ('+device+') requires admin rights.\n\nFIX:\n1. Press Ctrl+C to stop server\n2. Open PowerShell as Administrator\n3. cd "'+ROOT+'"\n4. node server.js\n5. Refresh browser\n\nTip: You can scan disk image files (.img / .raw) without Administrator rights!'});
    return res.end();
  }
  if(!exeOk()){send({type:'need_build',message:'neurorecover.exe not found. Click Build Engine!'});return res.end();}

  // Output path — use backslashes for Windows mkdir, forward slashes for C++ args
  let rawOut = winPath(outputDir || DEFAULT_OUT);
  try{
    fs.mkdirSync(rawOut,{recursive:true});
  }catch(e){
    console.warn(`Cannot create ${rawOut}, falling back to ${DEFAULT_OUT}`);
    rawOut = DEFAULT_OUT;
    try { fs.mkdirSync(rawOut,{recursive:true}); }
    catch(e2){send({type:'error',message:'Cannot create output folder: '+rawOut+'\n'+e2.message});return res.end();}
  }
  const fwdOut = normPath(rawOut);

  // Types
  let typesStr='all';
  if(types){
    if(Array.isArray(types)&&types.length)typesStr=types.join(',');
    else if(typeof types==='string'&&types.trim())typesStr=types.trim();
  }

  // Build args
  const args=[device, fwdOut, '-t', typesStr];
  if(deep)       args.push('-d');
  args.push('-f');              // always repair
  if(metadata)   args.push('-a');
  if(preview)    args.push('-p');
  if(checkpoint) args.push('-c');
  if(resume)     args.push('--resume');
  if(sqliteDeep) args.push('--sqlite');
  args.push('-r','-v');         // report + verbose
  // TEMP DIAGNOSTIC: --turbo forces 16 threads + 128MB buffer, which may be
  // hammering this slow/old USB drive with too much concurrent I/O. Testing
  // with defaults (4 threads) to isolate whether concurrency is a factor.
  // args.push('--turbo');

  const exePath=findExe();
  console.log(`\n▶ SCAN: ${device} -> ${rawOut}`);
  console.log(`  EXE: ${exePath}`);
  console.log(`  Args: ${args.join(' ')}\n`);

  preventSleep();
  send({type:'start',message:'Scanning '+device,device,outputDir:rawOut,fwdOut});

  let proc;
  try{proc=spawn(exePath,args,{stdio:['ignore','pipe','pipe']});}
  catch(e){allowSleep();send({type:'error',message:'Failed to start: '+e.message});return res.end();}

  const id=crypto.randomBytes(4).toString('hex');
  scans[id]=proc;
  send({type:'pid',scanId:id});

  const currentUser = req.user || null;
  try {
    db.recordScanStart({
      id,
      userId: currentUser ? currentUser.id : null,
      devicePath: device,
      outputDir: rawOut,
      options: { types: typesStr, deep: !!deep, repair: true, metadata: !!metadata, preview: !!preview }
    });
  } catch(_) {}

  // If the client disconnects for ANY reason (browser closed, network
  // reset, server restarted via --watch mid-scan), kill the child process.
  // Without this, neurorecover.exe becomes orphaned and keeps scanning the
  // same drive in the background — causing duplicate/competing raw reads.
  //
  // IMPORTANT: req.on('close') fires once the request body has been fully
  // read — NOT only on a real client disconnect. Without the res.writableEnded
  // guard below, this fired on every scan almost immediately after spawning
  // and killed it before it could do anything.
  res.on('close',()=>{
    if(scans[id] && !res.writableEnded){
      console.log('⚠ Client disconnected — killing orphaned scan process id='+id);
      try{scans[id].kill('SIGTERM');}catch(_){}
      delete scans[id];
      allowSleep();
      try {
        db.recordScanEnd(id, {
          status: 'stopped',
          totalFiles: total,
          filesRepaired: 0,
          bytesScanned: 0
        });
      } catch(_) {}
    }
  });

  // SSE keepalive
  const ka=setInterval(()=>{try{res.write(':ka\n\n');}catch(_){}},15000);

  let buf='',total=0,byType={},byCategory={};

  proc.stdout.on('data',chunk=>{
    buf+=chunk.toString();
    // Split on \r AND \n — ProgressTracker::print() uses \r (no \n) for live
    // terminal redraw, and without splitting on it too, a \r-terminated
    // progress update can merge with the next \n-terminated log line into
    // one corrupted "line", producing garbled ETA text with embedded logs.
    const lines=buf.split(/[\r\n]+/);buf=lines.pop();
    lines.forEach(line=>{
      if(!line.trim())return;

      // File carved: [INFO ] Carved: C:/recovered/recovered_000001.jpg (12 KB)
      const cm=line.match(/Carved:\s+(.+?)\s+\((\d+)\s*KB\)/i);
      if(cm){
        total++;
        const fp=cm[1].trim();
        const fname=fp.split(/[/\\]/).pop();
        const ext=path.extname(fp).slice(1).toLowerCase();
        const cat=getCategory(ext);
        const szBytes=(parseInt(cm[2])||0)*1024;
        byType[ext]=(byType[ext]||0)+1;
        byCategory[cat]=(byCategory[cat]||0)+1;
        try {
          db.recordRecoveredFile({
            scanId: id,
            userId: currentUser ? currentUser.id : null,
            fileName: fname,
            filePath: fp,
            extension: ext,
            category: cat,
            sizeBytes: szBytes,
            isRepaired: true
          });
        } catch(_) {}
        const isImg=['jpg','jpeg','png','gif','bmp','webp','heic'].includes(ext);
        send({type:'file',path:fp,name:fname,ext,category:cat,total,byType,byCategory,
          sizeKB:parseInt(cm[2]),
          // Provide thumb URL for images so UI can show real thumbnails
          thumbUrl:isImg?`/api/thumb?file=${encodeURIComponent(fname)}&dir=${encodeURIComponent(fwdOut)}`:null,
        });
        return;
      }

      // Progress: Carving [####---] 5.3% ETA:45m
      const pm=line.match(/(\d+\.?\d*)%\s+ETA:(.+)/);
      if(pm){send({type:'progress',percent:parseFloat(pm[1]),eta:pm[2].trim(),files:total});return;}

      // Phase
      const ph=line.match(/Phase:\s*(\w+)/i);
      if(ph){send({type:'phase',phase:ph[1]});return;}

      // Access denied
      if(line.includes('Cannot open device')||line.toLowerCase().includes('access is denied')){
        send({type:'access_denied',message:'❌ Cannot open '+device+'\n\nRestart PowerShell as Administrator:\n  node server.js'});return;
      }

      // Encryption
      if(line.includes('ENCRYPTED')||line.includes('BitLocker'))
        send({type:'encryption',message:line.trim()});

      // Log lines
      if(line.includes('[INFO ]')) send({type:'log',message:line.replace(/.*\[INFO \]\s*/,'').trim()});
      if(line.includes('[WARN ]')) send({type:'warn',message:line.replace(/.*\[WARN \]\s*/,'').trim()});
      if(line.includes('[ERROR]')) send({type:'error',message:line.replace(/.*\[ERROR\]\s*/,'').trim()});
    });
  });

  proc.stderr.on('data',d=>{const m=d.toString().trim();if(m)send({type:'log',message:m});});

  proc.on('close',(code,signal)=>{
    clearInterval(ka);delete scans[id];allowSleep();
    console.log(`✅ Scan done. EXE code: ${code} signal: ${signal} files: ${total}`);
    try {
      db.recordScanEnd(id, {
        status: code === 0 ? 'completed' : 'interrupted',
        totalFiles: total,
        filesRepaired: 0,
        bytesScanned: 0
      });
    } catch(_) {}
    // Count files actually on disk
    let diskFiles=0;
    try{
      diskFiles=fs.readdirSync(rawOut).filter(f=>{
        try{const st=fs.statSync(path.join(rawOut,f));return st.isFile()&&st.size>0&&!f.endsWith('.txt')&&!f.endsWith('.json')&&f!=='gallery.html';}
        catch{return false;}
      }).length;
    }catch{}
    send({type:'done',exitCode:code,filesFound:total,diskFiles,byType,byCategory,
      outputDir:rawOut,fwdOut,
      galleryPath:fs.existsSync(path.join(rawOut,'gallery.html'))?path.join(rawOut,'gallery.html'):null,
    });
    res.end();
  });

  proc.on('error',e=>{clearInterval(ka);delete scans[id];allowSleep();send({type:'error',message:e.message});res.end();});
});

// ── POST /api/stop ────────────────────────────────────────────────────────────
app.post('/api/stop',(req,res)=>{
  const{scanId}=req.body||{};
  if(scanId&&scans[scanId]){
    scans[scanId].kill('SIGTERM');
    delete scans[scanId];
    try { db.recordScanEnd(scanId, { status: 'stopped' }); } catch(_) {}
  }
  allowSleep();res.json({stopped:true});
});

// ── GET /api/files — list recovered files from disk ───────────────────────────
app.get('/api/files',(req,res)=>{
  const{dir='',category='',ext:fext=''}=req.query;
  const winDir=winPath(dir);
  if(!winDir||!fs.existsSync(winDir))return res.json({files:[],total:0,totalSize:0});
  const SKIP=new Set(['gallery.html','recovery_report.txt','.neurorecover_checkpoint.json','recovery.log']);
  const fwdDir=normPath(winDir);
  try{
    let files=fs.readdirSync(winDir)
      .filter(n=>!SKIP.has(n)&&!n.endsWith('.json')&&!n.endsWith('.log')&&!n.endsWith('.txt'))
      .map(n=>{
        const full=path.join(winDir,n);
        try{
          const st=fs.statSync(full);
          if(!st.isFile()||!st.size)return null;
          const e=path.extname(n).slice(1).toLowerCase();
          const cat=getCategory(e);
          const isImg=['jpg','jpeg','png','gif','bmp','webp','heic'].includes(e);
          return{
            name:n,ext:e,size:st.size,
            path:fwdDir+'/'+n,
            category:cat,
            modified:st.mtimeMs,
            // Real thumbnail URL for images
            thumbUrl:isImg?`/api/thumb?file=${encodeURIComponent(n)}&dir=${encodeURIComponent(fwdDir)}`:null,
          };
        }catch{return null;}
      })
      .filter(Boolean)
      .sort((a,b)=>b.modified-a.modified);
    if(category)files=files.filter(f=>f.category===category);
    if(fext)files=files.filter(f=>f.ext===fext);
    res.json({files,total:files.length,totalSize:files.reduce((a,f)=>a+f.size,0)});
  }catch(e){res.json({files:[],total:0,totalSize:0,error:e.message});}
});

// ── GET /api/check-output ─────────────────────────────────────────────────────
app.get('/api/check-output',(req,res)=>{
  const dir=normPath(req.query.dir||'C:/recovered');
  const countDir=d=>{
    const wd=winPath(d);
    try{
      if(!fs.existsSync(wd))return{count:0,size:0};
      const items=fs.readdirSync(wd).filter(f=>{
        try{const st=fs.statSync(path.join(wd,f));return st.isFile()&&st.size>0&&!f.endsWith('.txt')&&!f.endsWith('.json')&&f!=='gallery.html';}
        catch{return false;}
      });
      return{count:items.length,size:items.reduce((a,f)=>{try{return a+fs.statSync(path.join(wd,f)).size;}catch{return a;}},0),sample:items.slice(0,5)};
    }catch{return{count:0,size:0};}
  };
  const main=countDir(dir);
  const alternates=[dir,...SEARCH_DIRS].map(d=>({path:d,exists:fs.existsSync(winPath(d)),...countDir(d)}));
  res.json({dir,exists:fs.existsSync(winPath(dir)),...main,alternates});
});

// ── POST /api/recover — REAL: copy + repair files ─────────────────────────────
app.post('/api/recover',(req,res)=>{
  sseOpen(res); const send=sseSend(res);
  const{files,outputDir,repair=true}=req.body||{};
  if(!files||!files.length){send({type:'error',message:'No files specified'});return res.end();}

  const destWin=winPath(outputDir||'C:\\MyRecoveredFiles');
  const destFwd=normPath(destWin);
  try{fs.mkdirSync(destWin,{recursive:true});}
  catch(e){send({type:'error',message:'Cannot create destination: '+destWin+'\n'+e.message});return res.end();}

  send({type:'start',total:files.length,outputDir:destWin});
  let done=0,repaired=0,failed=0,skipped=0;

  (async()=>{
    for(const file of files){
      try{
        const srcPath=winPath(file.path||'');
        const fileName=file.name||path.basename(srcPath);
        const ext=(file.ext||path.extname(fileName).slice(1)).toLowerCase();
        const destPath=path.join(destWin,fileName);

        if(!srcPath||!fs.existsSync(srcPath)){
          skipped++;done++;
          send({type:'progress',done,total:files.length,file:fileName,status:'skipped'});
          continue;
        }

        const data=fs.readFileSync(srcPath);
        let out=data,wasRepaired=false;

        if(repair){
          if(['jpg','jpeg'].includes(ext)){const f=repairJPEG(data);if(f){out=f;wasRepaired=true;}}
          else if(ext==='bmp'){const f=repairBMP(data);if(f){out=f;wasRepaired=true;}}
          else if(ext==='png'){const f=repairPNG(data);if(f){out=f;wasRepaired=true;}}
        }

        fs.writeFileSync(destPath,out);
        if(wasRepaired)repaired++;
        done++;
        send({type:'progress',done,total:files.length,file:fileName,status:wasRepaired?'repaired':'ok',repaired:wasRepaired});

      }catch(e){
        failed++;done++;
        send({type:'progress',done,total:files.length,file:file.name||'?',status:'failed',error:e.message});
      }
    }
    console.log(`✅ Recover done: ${done} files, ${repaired} repaired, ${failed} failed`);
    send({type:'done',done,repaired,failed,skipped,outputDir:destWin});
    res.end();
  })();
});

// ── File repair helpers ───────────────────────────────────────────────────────
function repairJPEG(data){
  try{
    let buf=Buffer.from(data),mod=false;
    if(buf[0]!==0xFF||buf[1]!==0xD8){buf=Buffer.concat([Buffer.from([0xFF,0xD8]),buf]);mod=true;}
    let end=buf.length;while(end>4&&buf[end-1]===0)end--;
    if(end<buf.length){buf=buf.slice(0,end);mod=true;}
    let hasEOI=false;
    for(let i=Math.max(0,buf.length-10);i<buf.length-1;i++)if(buf[i]===0xFF&&buf[i+1]===0xD9){hasEOI=true;break;}
    if(!hasEOI){buf=Buffer.concat([buf,Buffer.from([0xFF,0xD9])]);mod=true;}
    return mod?buf:null;
  }catch{return null;}
}
function repairBMP(data){
  try{
    const buf=Buffer.from(data);if(buf[0]!==0x42||buf[1]!==0x4D)return null;
    let mod=false;
    if(buf.readUInt32LE(2)!==buf.length){buf.writeUInt32LE(buf.length,2);mod=true;}
    if(buf.readUInt16LE(6)!==0){buf.writeUInt16LE(0,6);mod=true;}
    if(buf.readUInt16LE(8)!==0){buf.writeUInt16LE(0,8);mod=true;}
    return mod?buf:null;
  }catch{return null;}
}
function repairPNG(data){
  try{
    const SIG=Buffer.from([0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A]);
    const IEND=Buffer.from([0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,0x60,0x82]);
    let buf=Buffer.from(data),mod=false;
    if(!buf.slice(0,8).equals(SIG)){buf=Buffer.concat([SIG,buf.slice(8)]);mod=true;}
    if(!buf.slice(-12).equals(IEND)){buf=Buffer.concat([buf,IEND]);mod=true;}
    return mod?buf:null;
  }catch{return null;}
}

// ── POST /api/ai/chat ─────────────────────────────────────────────────────────
function localAIChat(message,context){
  const m=(message||'').toLowerCase();
  const filesFound=context?.filesFound||0;
  const byCategory=context?.byCategory||{};
  const device=context?.device||'the selected drive';

  const rules=[
    {test:/how long|eta|faster|speed|take/,reply:()=>
      `Scan speed depends mainly on drive size and USB version:\n\n`+
      `• USB 2.0: ~60-90 min for a 128GB drive (~25-35 MB/s)\n`+
      `• USB 3.0 (blue port): ~15-25 min for the same size (~85-110 MB/s)\n\n`+
      `To speed things up:\n`+
      `→ Use a USB 3.0 (blue) port if available\n`+
      `→ Try a Quick Scan (Photos/Videos/Audio) instead of Full Scan\n`+
      `→ Close other disk-heavy programs while scanning`},
    {test:/corrupt|won.?t open|broken|damaged|repair/,reply:()=>
      `NeuroRecover auto-repairs common corruption on JPEG, PNG, BMP and MP4/AVI files during recovery (check "Auto-repair" in the Recover dialog).\n\n`+
      `If a file still won't open after repair:\n`+
      `→ It was likely fragmented across non-contiguous disk sectors\n`+
      `→ Or part of the data was already overwritten by newer files\n\n`+
      `Files tagged "FIXED" in the grid were successfully repaired. Files without that tag but still broken are likely partially overwritten.`},
    {test:/0 files|no files|exit code 1|nothing found|found nothing/,reply:()=>
      `A scan returning 0 files usually means:\n\n`+
      `→ Wrong drive selected — check the Target Device dropdown\n`+
      `→ Not running as Administrator — raw disk access needs admin rights\n`+
      `→ neurorecover.exe isn't built yet — check the sidebar for "Build Engine"\n`+
      `→ The drive is genuinely empty of recoverable data (heavily overwritten)\n\n`+
      `Try: confirm admin mode, reselect the drive, and re-run.`},
    {test:/format|formatted/,reply:()=>
      `A quick format only clears the filesystem's file table — it doesn't erase actual file data. A Deep Scan with raw carving should still recover most files, though original filenames may be lost (shown as recovered_NNNNNN.ext instead).\n\n`+
      `A full/slow format overwrites the whole drive and is much less recoverable.`},
    {test:/best setting|recommend|settings|optimal/,reply:()=>
      `Recommended settings for most jobs:\n\n`+
      `→ Deep Scan: ON\n→ Auto-repair: ON\n→ EXIF/GPS: ON if you care about photo metadata\n→ Checkpoint/Resume: ON for drives over 64GB\n\n`+
      `For specific file types only, use the Quick Scan buttons — 2-3x faster than full scan.`},
    {test:/encrypt|bitlocker|locked/,reply:()=>
      `If BitLocker or another encrypted volume is detected, raw carving can't read the encrypted region without the decryption key.\n\n`+
      `Unlock the drive first via Windows' "Manage BitLocker" tool using your recovery key or password, then re-run the scan.`},
  ];

  for(const rule of rules){if(rule.test.test(m))return rule.reply();}

  if(filesFound>0){
    const top=Object.entries(byCategory).sort((a,b)=>b[1]-a[1])[0];
    return `You currently have ${filesFound} files recovered${top?`, mostly ${top[0]} (${top[1]} files)`:''} from ${device}.\n\n`+
      `Ask me about scan speed, fixing corrupted files, best settings, or what "0 files found" means.`;
  }
  return `I can help with:\n→ Best scan settings\n→ Why files show as corrupted\n→ What "0 files found" means\n→ Recovering from a formatted drive\n→ Scan speed / making it faster\n\nAsk about any of these, or run a scan and I'll analyze your results.`;
}

function localAISuggest(device,driveType,goal){
  const g=(goal||'').toLowerCase();
  const isSystemDrive=/physicaldrive0/i.test(device||'');
  const wantsPhotos=/photo|image|pic/.test(g);
  const wantsVideos=/video|movie/.test(g);
  const wantsAudio=/audio|music|song/.test(g);

  const settings={deep:true,repair:true,metadata:wantsPhotos};
  const tips=[];
  let recommendation,expectedTime;

  if(isSystemDrive){
    recommendation='This is your system drive — scanning it live is riskier since it changes constantly. If possible, recover from a different physical drive, or boot from external media first.';
    expectedTime='Varies — system drives are usually much larger';
    tips.push('Avoid scanning C:\\ while Windows is actively writing to it');
    tips.push('Consider imaging the drive first if the files are critical');
  }else{
    recommendation='Use Deep Scan with auto-repair enabled for the most thorough recovery of '+
      (wantsPhotos?'photos':wantsVideos?'videos':wantsAudio?'audio files':'your files')+'.';
    expectedTime='30–90 min (USB 2.0) · 15–25 min (USB 3.0)';
    tips.push('Use the blue USB 3.0 port for up to 4x faster scanning');
    tips.push('Run the server as Administrator — raw disk access requires it');
    tips.push('Avoid writing new files to the drive before scanning');
    if(wantsPhotos)tips.push('Enable EXIF/GPS extraction to recover photo metadata');
  }
  return{recommendation,settings,expectedTime,tips};
}

app.post('/api/ai/chat',(req,res)=>{
  const{message,context}=req.body||{};
  if(!message)return res.json({success:false,reply:'No message'});
  res.json({success:true,reply:localAIChat(message,context)});
});

app.post('/api/ai/suggest',(req,res)=>{
  const{device,driveType,goal}=req.body||{};
  res.json({success:true,...localAISuggest(device,driveType,goal)});
});

// ── Auth Endpoints ────────────────────────────────────────────────────────────
app.post('/api/auth/signup', (req, res) => {
  const { username, email, password } = req.body || {};
  try {
    const result = db.registerUser(username, email, password);
    res.json({ success: true, user: result.user, token: result.token });
  } catch (err) {
    res.status(400).json({ success: false, error: err.message });
  }
});

app.post('/api/auth/login', (req, res) => {
  const { identifier, password } = req.body || {};
  try {
    const result = db.loginUser(identifier, password);
    res.json({ success: true, user: result.user, token: result.token });
  } catch (err) {
    res.status(401).json({ success: false, error: err.message });
  }
});

app.get('/api/auth/me', (req, res) => {
  res.json({ success: true, user: req.user || null });
});

app.post('/api/auth/logout', (req, res) => {
  const auth = req.headers['authorization'];
  const token = auth && auth.startsWith('Bearer ') ? auth.slice(7) : (req.query.token || null);
  if (token) db.revokeSession(token);
  res.json({ success: true });
});

// ── Scan History & Database Query Endpoints ───────────────────────────────────
app.get('/api/history/scans', (req, res) => {
  const userId = req.user ? req.user.id : null;
  const scans = db.getScanHistory(userId);
  res.json({ success: true, scans });
});

app.get('/api/history/scans/:id', (req, res) => {
  const details = db.getScanDetails(req.params.id);
  if (!details) return res.status(404).json({ success: false, error: 'Scan not found' });
  res.json({ success: true, ...details });
});

app.delete('/api/history/scans/:id', (req, res) => {
  db.deleteScanRecord(req.params.id);
  res.json({ success: true });
});

// ── Helpers ───────────────────────────────────────────────────────────────────
function fmtSize(b){if(!b)return'0 B';if(b<1024)return b+' B';if(b<1048576)return(b/1024).toFixed(1)+' KB';if(b<1073741824)return(b/1048576).toFixed(1)+' MB';return(b/1073741824).toFixed(2)+' GB';}
const EC={jpg:'image',jpeg:'image',png:'image',gif:'image',bmp:'image',tiff:'image',tif:'image',webp:'image',heic:'image',psd:'image',ico:'image',mp4:'video',mov:'video',avi:'video',mkv:'video',flv:'video',wmv:'video',webm:'video','3gp':'video',m4v:'video',mpeg:'video',mp3:'audio',wav:'audio',flac:'audio',m4a:'audio',ogg:'audio',aac:'audio',wma:'audio',pdf:'document',docx:'document',xlsx:'document',pptx:'document',odt:'document',rtf:'document',epub:'document',txt:'document',zip:'archive',rar:'archive','7z':'archive',gz:'archive',cr2:'raw',cr3:'raw',nef:'raw',arw:'raw',raf:'raw',dng:'raw',orf:'raw',rw2:'raw',sqlite:'database',db:'database'};
function getCategory(e){return EC[e]||'other';}

// ── Start ─────────────────────────────────────────────────────────────────────
const server=app.listen(PORT,'0.0.0.0',()=>console.log('✅ http://localhost:'+PORT));
server.on('error',e=>{if(e.code==='EADDRINUSE'){console.error('Port '+PORT+' busy → run: npx kill-port '+PORT);process.exit(1);}});
process.on('SIGINT',()=>{allowSleep();process.exit(0);});
process.on('SIGTERM',()=>{allowSleep();process.exit(0);});