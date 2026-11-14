/**
 * NeuroRecover EXE Tester
 * Run: node test_exe.js
 * This tests if neurorecover.exe can be spawned by Node.js
 */
const { spawn, execSync } = require('child_process');
const path = require('path');
const fs   = require('fs');

const EXE = path.join(__dirname, 'build', 'neurorecover.exe');
console.log('\n=== NeuroRecover EXE Test ===');
console.log('EXE path:', EXE);
console.log('EXE exists:', fs.existsSync(EXE));

if (!fs.existsSync(EXE)) {
  console.log('ERROR: EXE not found! Build it first with build_windows.sh');
  process.exit(1);
}

// Check EXE file size
const stat = fs.statSync(EXE);
console.log('EXE size:', (stat.size/1024/1024).toFixed(1), 'MB');

// Try to get file type on Windows
try {
  const out = execSync(`powershell -command "Get-Item '${EXE}' | Select-Object -ExpandProperty VersionInfo" 2>nul`, {encoding:'utf8',timeout:5000});
  console.log('Version info:', out.trim().slice(0,100));
} catch(e) {}

// Try simple run with --help (no disk access needed)
console.log('\n--- Testing: neurorecover.exe --help ---');
const test1 = spawn(EXE, ['--help'], { stdio:['ignore','pipe','pipe'], windowsHide:true });
let out1 = '';
test1.stdout.on('data', d => { out1 += d.toString(); process.stdout.write('.'); });
test1.stderr.on('data', d => { console.log('STDERR:', d.toString().trim()); });
test1.on('close', (code, signal) => {
  console.log('\nExit code:', code, 'Signal:', signal);
  console.log('Got', out1.length, 'bytes of output');
  if (out1.length > 0) {
    console.log('First 200 chars:', out1.slice(0,200));
    console.log('\n✅ EXE works! It can output text to stdout.');
    console.log('The issue is with disk access or path handling.');
    testDisk();
  } else {
    console.log('\n❌ EXE produced NO output — likely missing DLLs!');
    console.log('\nFIX: Rebuild in MSYS2 MINGW64 with static linking:');
    console.log('  ./build_windows.sh');
    console.log('\nOr check for missing DLLs:');
    console.log('  Run the exe directly in MSYS2 MINGW64 terminal');
  }
});
test1.on('error', e => {
  console.log('\n❌ Spawn failed:', e.code, e.message);
  console.log('\nThe exe cannot be launched by Node.js at all.');
  console.log('Fix: Check if it is a valid Windows PE executable');
  console.log('  Run in MSYS2: file build/neurorecover.exe');
});

function testDisk() {
  console.log('\n--- Testing: scan //./PhysicalDrive1 (5 second timeout) ---');
  const test2 = spawn(EXE, ['//./PhysicalDrive1','C:\\test_output','-t','jpg','-j','1'], 
    { stdio:['ignore','pipe','pipe'], windowsHide:true });
  let out2 = '';
  const timer = setTimeout(() => {
    console.log('Timeout reached - killing process');
    test2.kill();
  }, 5000);
  test2.stdout.on('data', d => { out2 += d.toString(); });
  test2.stderr.on('data', d => { out2 += d.toString(); });
  test2.on('close', (code, signal) => {
    clearTimeout(timer);
    console.log('Exit:', code, signal);
    console.log('Output received:', out2.length, 'bytes');
    if (out2.length > 0) console.log('Output:', out2.slice(0,500));
    else console.log('❌ NO OUTPUT from disk scan either!');
  });
}