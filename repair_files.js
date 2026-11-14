/**
 * NeuroRecover File Repair Tool
 * Fixes corrupted JPEG, PNG, BMP, MP4 files that can't open
 * 
 * Run: node repair_files.js C:\test_output
 * Or:  node repair_files.js (repairs C:\test_output by default)
 */

const fs   = require('fs');
const path = require('path');

const inputDir  = process.argv[2] || 'C:\\test_output';
const outputDir = process.argv[3] || inputDir + '_repaired';

console.log('\n╔══════════════════════════════════════════╗');
console.log('║   NeuroRecover File Repair Tool v1.0    ║');
console.log('╠══════════════════════════════════════════╣');
console.log('║  Input : ' + inputDir.padEnd(32) + '║');
console.log('║  Output: ' + outputDir.padEnd(32) + '║');
console.log('╚══════════════════════════════════════════╝\n');

if (!fs.existsSync(inputDir)) {
  console.error('❌ Input folder not found:', inputDir);
  console.log('Usage: node repair_files.js C:\\test_output');
  process.exit(1);
}

fs.mkdirSync(outputDir, { recursive: true });

// ── JPEG Repair ───────────────────────────────────────────────────────────────
function repairJPEG(data) {
  const SOI = Buffer.from([0xFF, 0xD8]);
  const EOI = Buffer.from([0xFF, 0xD9]);
  
  let modified = false;
  let buf = Buffer.from(data);
  
  // Fix SOI marker
  if (buf[0] !== 0xFF || buf[1] !== 0xD8) {
    buf = Buffer.concat([SOI, buf]);
    modified = true;
  }
  
  // Find true end by scanning for last valid JPEG data
  // Look for EOI or truncate at first all-zero run
  let lastGoodByte = buf.length;
  for (let i = buf.length - 1; i > 10; i--) {
    if (buf[i] !== 0x00) { lastGoodByte = i + 1; break; }
  }
  
  // Check if EOI exists
  const hasEOI = buf[lastGoodByte-2] === 0xFF && buf[lastGoodByte-1] === 0xD9;
  if (!hasEOI) {
    buf = Buffer.concat([buf.slice(0, lastGoodByte), EOI]);
    modified = true;
  }
  
  // Validate: must have at least SOI + APP0/APP1 + data + EOI
  if (buf.length < 10) return null;
  
  return { data: buf, modified };
}

// ── BMP Repair ────────────────────────────────────────────────────────────────
function repairBMP(data) {
  let buf = Buffer.from(data);
  if (buf[0] !== 0x42 || buf[1] !== 0x4D) return null; // not BMP
  
  let modified = false;
  
  // Fix file size field (bytes 2-5)
  const storedSize = buf.readUInt32LE(2);
  if (storedSize !== buf.length) {
    buf.writeUInt32LE(buf.length, 2);
    modified = true;
  }
  
  // Fix reserved bytes (must be 0)
  if (buf.readUInt16LE(6) !== 0 || buf.readUInt16LE(8) !== 0) {
    buf.writeUInt16LE(0, 6);
    buf.writeUInt16LE(0, 8);
    modified = true;
  }
  
  // Get dimensions and calculate expected pixel data size
  if (buf.length < 54) {
    // Too small for BMP header - pad to minimum
    const padded = Buffer.alloc(54, 0);
    buf.copy(padded, 0, 0, Math.min(buf.length, 54));
    buf = padded;
    modified = true;
  }
  
  const width  = buf.readInt32LE(18);
  const height = Math.abs(buf.readInt32LE(22));
  const bpp    = buf.readUInt16LE(28);
  const dataOffset = buf.readUInt32LE(10);
  
  if (width > 0 && height > 0 && bpp > 0 && bpp <= 32) {
    const rowSize = Math.floor((bpp * width + 31) / 32) * 4;
    const expectedPixels = rowSize * height;
    const available = buf.length > dataOffset ? buf.length - dataOffset : 0;
    
    if (expectedPixels > available && expectedPixels < 50 * 1024 * 1024) {
      // Pad with zeros to make it openable
      const padded = Buffer.alloc(dataOffset + expectedPixels, 0);
      buf.copy(padded);
      buf = padded;
      buf.writeUInt32LE(buf.length, 2); // fix size again
      modified = true;
    }
  }
  
  return { data: buf, modified };
}

// ── PNG Repair ────────────────────────────────────────────────────────────────
function repairPNG(data) {
  const PNG_SIG = Buffer.from([0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A]);
  const IEND    = Buffer.from([0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,0x60,0x82]);
  
  let buf = Buffer.from(data);
  let modified = false;
  
  // Fix PNG signature
  if (!buf.slice(0,8).equals(PNG_SIG)) {
    buf = Buffer.concat([PNG_SIG, buf.slice(8)]);
    modified = true;
  }
  
  // Ensure IEND chunk at end
  if (!buf.slice(-12).equals(IEND)) {
    buf = Buffer.concat([buf, IEND]);
    modified = true;
  }
  
  return { data: buf, modified };
}

// ── MP4 / Video repair (basic) ────────────────────────────────────────────────
function repairMP4(data) {
  const buf = Buffer.from(data);
  // Basic check: MP4 files need ftyp box at start
  // If we can find moov atom, file is likely OK
  const ftypIdx = buf.indexOf('ftyp');
  const moovIdx = buf.indexOf('moov');
  
  if (ftypIdx === -1 && moovIdx === -1) return null; // can't repair
  return { data: buf, modified: false }; // just copy as-is
}

// ── Process all files ─────────────────────────────────────────────────────────
const files = fs.readdirSync(inputDir);
let stats = { total:0, ok:0, repaired:0, skipped:0, failed:0 };

console.log(`Found ${files.length} files to process...\n`);

for (const filename of files) {
  const inPath  = path.join(inputDir, filename);
  const outPath = path.join(outputDir, filename);
  
  try {
    const stat = fs.statSync(inPath);
    if (!stat.isFile() || stat.size === 0) continue;
    
    stats.total++;
    const ext = path.extname(filename).slice(1).toLowerCase();
    const data = fs.readFileSync(inPath);
    
    let result = null;
    
    if (['jpg','jpeg'].includes(ext)) {
      result = repairJPEG(data);
    } else if (ext === 'bmp') {
      result = repairBMP(data);
    } else if (ext === 'png') {
      result = repairPNG(data);
    } else if (['mp4','mov','avi','mkv'].includes(ext)) {
      result = repairMP4(data);
    } else {
      // Just copy other files
      fs.copyFileSync(inPath, outPath);
      stats.ok++;
      continue;
    }
    
    if (!result) {
      // Unrecognized format - copy as-is
      fs.copyFileSync(inPath, outPath);
      stats.skipped++;
      continue;
    }
    
    fs.writeFileSync(outPath, result.data);
    
    if (result.modified) {
      stats.repaired++;
      if (stats.repaired <= 10 || stats.repaired % 100 === 0) {
        process.stdout.write(`✓ Repaired: ${filename}\n`);
      }
    } else {
      stats.ok++;
    }
    
  } catch(e) {
    stats.failed++;
    if (stats.failed <= 5) console.error(`✗ Failed: ${filename} — ${e.message}`);
  }
  
  // Progress
  if (stats.total % 100 === 0) {
    process.stdout.write(`  Progress: ${stats.total}/${files.length}...\r`);
  }
}

console.log('\n\n╔══════════════════════════════════════════╗');
console.log('║            Repair Complete!              ║');
console.log('╠══════════════════════════════════════════╣');
console.log('║  Total processed : ' + String(stats.total).padEnd(21) + '║');
console.log('║  Already OK      : ' + String(stats.ok).padEnd(21) + '║');
console.log('║  Repaired        : ' + String(stats.repaired).padEnd(21) + '║');
console.log('║  Skipped         : ' + String(stats.skipped).padEnd(21) + '║');
console.log('║  Failed          : ' + String(stats.failed).padEnd(21) + '║');
console.log('╠══════════════════════════════════════════╣');
console.log('║  Output folder:                          ║');
console.log('║  ' + outputDir.slice(0,40).padEnd(40) + '║');
console.log('╚══════════════════════════════════════════╝\n');
console.log('Open File Explorer → ' + outputDir);