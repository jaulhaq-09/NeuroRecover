/**
 * NeuroRecover AI — Embedded Database Layer (SQLite)
 * Handles persistent storage for:
 *   - User Authentication (passwords hashed with scrypt + salt)
 *   - Sessions (secure tokens)
 *   - Scan Sessions (historical logs, parameters, timestamps)
 *   - Recovered Files Catalog (file details, sizes, categories, repair status)
 */

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

// Ensure data folder exists
const DATA_DIR = path.join(__dirname, 'data');
if (!fs.existsSync(DATA_DIR)) {
  fs.mkdirSync(DATA_DIR, { recursive: true });
}
const DB_PATH = path.join(DATA_DIR, 'neurorecover.db');

// Initialize SQLite Database
let db;
try {
  const { DatabaseSync } = require('node:sqlite');
  db = new DatabaseSync(DB_PATH);
} catch (e) {
  console.error('Fatal: Failed to initialize SQLite DatabaseSync:', e.message);
  throw e;
}

// ── Schema Initialization ─────────────────────────────────────────────────────
db.exec(`
  PRAGMA foreign_keys = ON;

  CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    salt TEXT NOT NULL,
    role TEXT DEFAULT 'user',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
  );

  CREATE TABLE IF NOT EXISTS sessions (
    token TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    expires_at DATETIME NOT NULL
  );

  CREATE TABLE IF NOT EXISTS scans (
    id TEXT PRIMARY KEY,
    user_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    device_path TEXT NOT NULL,
    output_dir TEXT NOT NULL,
    status TEXT DEFAULT 'running',
    total_files INTEGER DEFAULT 0,
    files_repaired INTEGER DEFAULT 0,
    bytes_scanned INTEGER DEFAULT 0,
    started_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    finished_at DATETIME,
    options_json TEXT
  );

  CREATE TABLE IF NOT EXISTS recovered_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    scan_id TEXT REFERENCES scans(id) ON DELETE CASCADE,
    user_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    file_name TEXT NOT NULL,
    file_path TEXT NOT NULL,
    extension TEXT,
    category TEXT,
    size_bytes INTEGER DEFAULT 0,
    is_repaired INTEGER DEFAULT 0,
    discovered_at DATETIME DEFAULT CURRENT_TIMESTAMP
  );

  CREATE INDEX IF NOT EXISTS idx_files_scan ON recovered_files(scan_id);
  CREATE INDEX IF NOT EXISTS idx_files_user ON recovered_files(user_id);
  CREATE INDEX IF NOT EXISTS idx_scans_user ON scans(user_id);
`);

// ── Password Security Helpers ────────────────────────────────────────────────
function hashPassword(password, salt) {
  return crypto.scryptSync(password, salt, 64).toString('hex');
}

function verifyPassword(password, salt, expectedHash) {
  const actualHash = hashPassword(password, salt);
  const a = Buffer.from(actualHash, 'hex');
  const b = Buffer.from(expectedHash, 'hex');
  if (a.length !== b.length) return false;
  return crypto.timingSafeEqual(a, b);
}

// ── Ensure Default Guest User ────────────────────────────────────────────────
function ensureGuestUser() {
  const check = db.prepare('SELECT id FROM users WHERE username = ?').get('guest');
  if (!check) {
    const salt = crypto.randomBytes(16).toString('hex');
    const hash = hashPassword('guest_neurorecover_local', salt);
    db.prepare('INSERT INTO users (username, email, password_hash, salt, role) VALUES (?, ?, ?, ?, ?)')
      .run('guest', 'guest@local.neurorecover', hash, salt, 'guest');
  }
}
ensureGuestUser();

// ── User Authentication API ──────────────────────────────────────────────────
function registerUser(username, email, password) {
  const trimmedUser = (username || '').trim();
  const trimmedEmail = (email || '').trim().toLowerCase();
  if (!trimmedUser || trimmedUser.length < 3) {
    throw new Error('Username must be at least 3 characters.');
  }
  if (!trimmedEmail || !trimmedEmail.includes('@')) {
    throw new Error('Please provide a valid email address.');
  }
  if (!password || password.length < 6) {
    throw new Error('Password must be at least 6 characters.');
  }

  const existing = db.prepare('SELECT id FROM users WHERE username = ? OR email = ?').get(trimmedUser, trimmedEmail);
  if (existing) {
    throw new Error('Username or email already exists.');
  }

  const salt = crypto.randomBytes(16).toString('hex');
  const hash = hashPassword(password, salt);
  const res = db.prepare('INSERT INTO users (username, email, password_hash, salt, role) VALUES (?, ?, ?, ?, ?)')
    .run(trimmedUser, trimmedEmail, hash, salt, 'user');
  
  const userId = Number(res.lastInsertRowid);
  const token = createSession(userId);
  return {
    user: { id: userId, username: trimmedUser, email: trimmedEmail, role: 'user' },
    token
  };
}

function loginUser(emailOrUsername, password) {
  const identifier = (emailOrUsername || '').trim();
  if (!identifier || !password) {
    throw new Error('Please enter username/email and password.');
  }

  const user = db.prepare('SELECT * FROM users WHERE username = ? OR email = ?')
    .get(identifier, identifier.toLowerCase());
  
  if (!user || !verifyPassword(password, user.salt, user.password_hash)) {
    throw new Error('Invalid username/email or password.');
  }

  const token = createSession(user.id);
  return {
    user: { id: user.id, username: user.username, email: user.email, role: user.role },
    token
  };
}

function createSession(userId, durationDays = 7) {
  const token = crypto.randomBytes(32).toString('hex');
  const expiresAt = new Date(Date.now() + durationDays * 24 * 60 * 60 * 1000).toISOString();
  db.prepare('INSERT INTO sessions (token, user_id, expires_at) VALUES (?, ?, ?)').run(token, userId, expiresAt);
  return token;
}

function getUserByToken(token) {
  if (!token) return null;
  const now = new Date().toISOString();
  const row = db.prepare(`
    SELECT u.id, u.username, u.email, u.role, u.created_at
    FROM sessions s
    JOIN users u ON s.user_id = u.id
    WHERE s.token = ? AND s.expires_at > ?
  `).get(token, now);
  return row || null;
}

function revokeSession(token) {
  if (!token) return;
  db.prepare('DELETE FROM sessions WHERE token = ?').run(token);
}

// ── Scan & Recovery History API ──────────────────────────────────────────────
function recordScanStart({ id, userId, devicePath, outputDir, options }) {
  const opts = options ? JSON.stringify(options) : '{}';
  db.prepare(`
    INSERT INTO scans (id, user_id, device_path, output_dir, status, started_at, options_json)
    VALUES (?, ?, ?, ?, 'running', CURRENT_TIMESTAMP, ?)
  `).run(id, userId || null, devicePath, outputDir, opts);
}

function recordScanProgress(id, { totalFiles, filesRepaired, bytesScanned }) {
  db.prepare(`
    UPDATE scans
    SET total_files = ?, files_repaired = ?, bytes_scanned = ?
    WHERE id = ?
  `).run(totalFiles || 0, filesRepaired || 0, bytesScanned || 0, id);
}

function recordScanEnd(id, { status = 'completed', totalFiles = 0, filesRepaired = 0, bytesScanned = 0 }) {
  db.prepare(`
    UPDATE scans
    SET status = ?, total_files = ?, files_repaired = ?, bytes_scanned = ?, finished_at = CURRENT_TIMESTAMP
    WHERE id = ?
  `).run(status, totalFiles, filesRepaired, bytesScanned, id);
}

function recordRecoveredFile({ scanId, userId, fileName, filePath, extension, category, sizeBytes, isRepaired }) {
  db.prepare(`
    INSERT INTO recovered_files (scan_id, user_id, file_name, file_path, extension, category, size_bytes, is_repaired)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
  `).run(
    scanId || null,
    userId || null,
    fileName,
    filePath,
    (extension || '').toLowerCase(),
    category || 'other',
    sizeBytes || 0,
    isRepaired ? 1 : 0
  );
}

function getScanHistory(userId = null, limit = 50) {
  if (userId) {
    return db.prepare(`
      SELECT * FROM scans
      WHERE user_id = ? OR user_id IS NULL
      ORDER BY started_at DESC
      LIMIT ?
    `).all(userId, limit);
  }
  return db.prepare(`
    SELECT * FROM scans
    ORDER BY started_at DESC
    LIMIT ?
  `).all(limit);
}

function getScanDetails(scanId) {
  const scan = db.prepare('SELECT * FROM scans WHERE id = ?').get(scanId);
  if (!scan) return null;
  const files = db.prepare('SELECT * FROM recovered_files WHERE scan_id = ? ORDER BY id DESC').all(scanId);
  return { scan, files };
}

function deleteScanRecord(scanId) {
  db.prepare('DELETE FROM scans WHERE id = ?').run(scanId);
}

module.exports = {
  db,
  registerUser,
  loginUser,
  getUserByToken,
  revokeSession,
  recordScanStart,
  recordScanProgress,
  recordScanEnd,
  recordRecoveredFile,
  getScanHistory,
  getScanDetails,
  deleteScanRecord
};
