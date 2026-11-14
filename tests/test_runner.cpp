#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cassert>
#include <cstring>
#include <sstream>

// ─── Test Framework ──────────────────────────────────────────────────────────
static int g_passed = 0, g_failed = 0;
struct TestResult { std::string name; bool passed; std::string msg; };
static std::vector<TestResult> g_results;

#define TEST(name) void name()
#define ASSERT_TRUE(expr)  do { if(!(expr)){ throw std::runtime_error("ASSERT_TRUE failed: " #expr); } } while(0)
#define ASSERT_FALSE(expr) do { if( (expr)){ throw std::runtime_error("ASSERT_FALSE failed: " #expr); } } while(0)
#define ASSERT_EQ(a,b)     do { if((a)!=(b)){ std::ostringstream _s; _s<<"ASSERT_EQ: "<<(a)<<" != "<<(b); throw std::runtime_error(_s.str()); } } while(0)
#define ASSERT_NE(a,b)     do { if((a)==(b)){ std::ostringstream _s; _s<<"ASSERT_NE: "<<(a)<<" == "<<(b); throw std::runtime_error(_s.str()); } } while(0)
#define ASSERT_GT(a,b)     do { if(!((a)>(b))){ std::ostringstream _s; _s<<"ASSERT_GT: "<<(a)<<" <= "<<(b); throw std::runtime_error(_s.str()); } } while(0)

void runTest(const std::string& name, std::function<void()> fn) {
    try { fn(); g_results.push_back({name, true, ""}); ++g_passed; }
    catch (std::exception& e) { g_results.push_back({name, false, e.what()}); ++g_failed; }
}

// ─── Include modules under test ──────────────────────────────────────────────
#include "utils/hash.h"
#include "utils/dedup.h"
#include "../core/carving/file_signatures.h"
#include "../core/ai/ai_engine.h"
#include "../core/reconstruct/jpeg_repair.h"
#include "../core/reconstruct/png_repair.h"
#include "../core/reconstruct/video_repair.h"
#include "utils/logger.h"

// ═══════════════════════════════════════════════════════════════════════════
//  HASH TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_fnv64_basic() {
    uint8_t d[] = {1,2,3,4};
    uint64_t h1 = HashUtils::fnv64(d, 4);
    uint64_t h2 = HashUtils::fnv64(d, 4);
    ASSERT_EQ(h1, h2); // deterministic
}

void test_fnv64_different_inputs() {
    uint8_t a[] = {1,2,3}; uint8_t b[] = {4,5,6};
    ASSERT_NE(HashUtils::fnv64(a,3), HashUtils::fnv64(b,3));
}

void test_fnv64_empty() {
    uint8_t empty[] = {};
    uint64_t h = HashUtils::fnv64(empty, 0);
    ASSERT_NE(h, (uint64_t)0); // FNV offset basis
}

void test_sha256_known() {
    // SHA-256("abc") = ba7816bf...
    uint8_t abc[] = {'a','b','c'};
    std::string h = HashUtils::sha256(abc, 3);
    ASSERT_EQ(h.substr(0,8), std::string("ba7816bf"));
}

void test_sha256_empty() {
    // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    uint8_t empty[] = {};
    std::string h = HashUtils::sha256(empty, 0);
    ASSERT_EQ(h.substr(0,8), std::string("e3b0c442"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  DEDUP TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_dedup_first_not_dup() {
    DedupEngine d;
    std::vector<uint8_t> data = {1,2,3,4,5};
    ASSERT_FALSE(d.isDuplicate(data));
}

void test_dedup_second_is_dup() {
    DedupEngine d;
    std::vector<uint8_t> data = {10,20,30};
    d.isDuplicate(data);
    ASSERT_TRUE(d.isDuplicate(data));
}

void test_dedup_different_not_dup() {
    DedupEngine d;
    std::vector<uint8_t> a = {1,2,3}, b = {4,5,6};
    d.isDuplicate(a);
    ASSERT_FALSE(d.isDuplicate(b));
}

void test_dedup_counts() {
    DedupEngine d;
    std::vector<uint8_t> x = {9,8,7};
    d.isDuplicate(x);
    d.isDuplicate(x);
    d.isDuplicate(x);
    ASSERT_EQ(d.uniqueCount(),    (size_t)1);
    ASSERT_EQ(d.duplicateCount(), (size_t)2);
}

void test_dedup_clear() {
    DedupEngine d;
    std::vector<uint8_t> x = {1,2,3};
    d.isDuplicate(x);
    d.clear();
    ASSERT_EQ(d.uniqueCount(), (size_t)0);
    ASSERT_FALSE(d.isDuplicate(x)); // fresh start
}

// ═══════════════════════════════════════════════════════════════════════════
//  FILE SIGNATURE TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_sig_db_not_empty() {
    auto& db = FileSignatureDB::instance();
    ASSERT_GT(db.all().size(), (size_t)30);
}

void test_sig_jpeg_match() {
    auto& db = FileSignatureDB::instance();
    uint8_t jpgHdr[] = {0xFF,0xD8,0xFF,0xE0,0x00,0x10};
    auto sigs = db.forTypes({"jpg"});
    bool found = false;
    for (auto& s : sigs)
        if (db.matchHeader(jpgHdr, 6, s)) { found = true; break; }
    ASSERT_TRUE(found);
}

void test_sig_png_match() {
    auto& db = FileSignatureDB::instance();
    uint8_t pngHdr[] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,0x00};
    auto sigs = db.forTypes({"png"});
    bool found = false;
    for (auto& s : sigs)
        if (db.matchHeader(pngHdr, 9, s)) { found = true; break; }
    ASSERT_TRUE(found);
}

void test_sig_mp4_match() {
    auto& db = FileSignatureDB::instance();
    uint8_t mp4Hdr[] = {0x00,0x00,0x00,0x18,0x66,0x74,0x79,0x70,0x6D,0x70};
    auto sigs = db.forTypes({"mp4"});
    bool found = false;
    for (auto& s : sigs)
        if (db.matchHeader(mp4Hdr, 10, s)) { found = true; break; }
    ASSERT_TRUE(found);
}

void test_sig_pdf_match() {
    auto& db = FileSignatureDB::instance();
    uint8_t pdfHdr[] = {0x25,0x50,0x44,0x46,0x2D,0x31}; // %PDF-1
    auto sigs = db.forTypes({"pdf"});
    bool found = false;
    for (auto& s : sigs)
        if (db.matchHeader(pdfHdr, 6, s)) { found = true; break; }
    ASSERT_TRUE(found);
}

void test_sig_zip_match() {
    auto& db = FileSignatureDB::instance();
    uint8_t zipHdr[] = {0x50,0x4B,0x03,0x04,0x00};
    auto sigs = db.forTypes({"zip"});
    bool found = false;
    for (auto& s : sigs)
        if (db.matchHeader(zipHdr, 5, s)) { found = true; break; }
    ASSERT_TRUE(found);
}

void test_sig_no_false_match() {
    auto& db = FileSignatureDB::instance();
    uint8_t zeros[16] = {};
    auto sigs = db.forTypes({"jpg"});
    for (auto& s : sigs)
        ASSERT_FALSE(db.matchHeader(zeros, 16, s));
}

void test_sig_forTypes_all() {
    auto& db = FileSignatureDB::instance();
    auto all  = db.all();
    auto jpgs = db.forTypes({"jpg"});
    ASSERT_GT(all.size(),  jpgs.size());
    ASSERT_GT(jpgs.size(), (size_t)0);
}

void test_sig_mkv_match() {
    auto& db = FileSignatureDB::instance();
    uint8_t mkvHdr[] = {0x1A,0x45,0xDF,0xA3,0x01};
    auto sigs = db.forTypes({"mkv"});
    bool found = false;
    for (auto& s : sigs)
        if (db.matchHeader(mkvHdr, 5, s)) { found = true; break; }
    ASSERT_TRUE(found);
}

void test_sig_flac_match() {
    auto& db = FileSignatureDB::instance();
    uint8_t flacHdr[] = {0x66,0x4C,0x61,0x43,0x00};
    auto sigs = db.forTypes({"flac"});
    bool found = false;
    for (auto& s : sigs)
        if (db.matchHeader(flacHdr, 5, s)) { found = true; break; }
    ASSERT_TRUE(found);
}

void test_sig_sqlite_match() {
    auto& db = FileSignatureDB::instance();
    const char* sqliteMagic = "SQLite format 3\0";
    auto sigs = db.forTypes({"sqlite"});
    bool found = false;
    for (auto& s : sigs)
        if (db.matchHeader(reinterpret_cast<const uint8_t*>(sqliteMagic), 16, s))
            { found = true; break; }
    ASSERT_TRUE(found);
}

// ═══════════════════════════════════════════════════════════════════════════
//  AI ENGINE TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_ai_entropy_zeros() {
    std::vector<uint8_t> zeros(1024, 0);
    double e = AIEngine::entropy(zeros.data(), zeros.size());
    ASSERT_EQ(e, 0.0);
}

void test_ai_entropy_random() {
    // A buffer with all 256 byte values has max entropy = 8.0
    std::vector<uint8_t> buf(256);
    for (int i = 0; i < 256; ++i) buf[i] = (uint8_t)i;
    double e = AIEngine::entropy(buf.data(), buf.size());
    ASSERT_GT(e, 7.9);
}

void test_ai_classify_jpeg() {
    AIEngine ai;
    uint8_t buf[] = {0xFF,0xD8,0xFF,0xE0,0x00,0x10,0x4A,0x46,0x49,0x46};
    double conf = ai.classifyBuffer(buf, 10, "jpg");
    ASSERT_GT(conf, 0.8);
}

void test_ai_classify_png() {
    AIEngine ai;
    uint8_t buf[] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};
    double conf = ai.classifyBuffer(buf, 8, "png");
    ASSERT_GT(conf, 0.9);
}

void test_ai_infer_jpg() {
    AIEngine ai;
    uint8_t buf[] = {0xFF,0xD8,0xFF,0xE0};
    ASSERT_EQ(ai.inferType(buf, 4), std::string("jpg"));
}

void test_ai_infer_png() {
    AIEngine ai;
    uint8_t buf[] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};
    ASSERT_EQ(ai.inferType(buf, 8), std::string("png"));
}

void test_ai_infer_pdf() {
    AIEngine ai;
    uint8_t buf[] = {0x25,0x50,0x44,0x46,0x2D};
    ASSERT_EQ(ai.inferType(buf, 5), std::string("pdf"));
}

void test_ai_infer_unknown() {
    AIEngine ai;
    uint8_t buf[] = {0xAA,0xBB,0xCC,0xDD};
    std::string t = ai.inferType(buf, 4);
    ASSERT_EQ(t, std::string("bin"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  JPEG REPAIR TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_jpeg_valid_detection() {
    JPEGRepair r;
    uint8_t good[] = {0xFF,0xD8,0xFF,0xE0,0xAB,0xCD,0xFF,0xD9};
    ASSERT_TRUE(r.isValid(good, 8));
}

void test_jpeg_invalid_detection() {
    JPEGRepair r;
    uint8_t bad[] = {0xFF,0xD8,0xFF,0xE0,0xAB,0xCD}; // no EOI
    ASSERT_FALSE(r.isValid(bad, 6));
}

void test_jpeg_repair_adds_eoi() {
    JPEGRepair r;
    std::vector<uint8_t> data = {0xFF,0xD8,0xFF,0xE0,0x00,0x10,0xAB,0xCD};
    auto result = r.repairData(data);
    ASSERT_TRUE(result.success);
    // EOI should be at end
    ASSERT_EQ(data.back(),   (uint8_t)0xD9);
    ASSERT_EQ(data[data.size()-2], (uint8_t)0xFF);
}

// ═══════════════════════════════════════════════════════════════════════════
//  PNG REPAIR TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_png_valid_detection() {
    PNGRepair r;
    uint8_t good[20] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
                        0,0,0,0,'I','E','N','D',0xAE,0x42,0x60,0x82};
    // Must have IEND at end
    ASSERT_TRUE(r.isValid(good, 20));
}

void test_png_signature_check() {
    PNGRepair r;
    uint8_t bad[12] = {0x00,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,1,2,3,4};
    ASSERT_FALSE(r.isValid(bad, 12));
}

// ═══════════════════════════════════════════════════════════════════════════
//  VIDEO REPAIR TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_video_mp4_valid() {
    VideoRepair r;
    uint8_t buf[] = {0x00,0x00,0x00,0x18,0x66,0x74,0x79,0x70,0x6D,0x70,0x34,0x32};
    ASSERT_TRUE(r.isValidMP4(buf, 12));
}

void test_video_avi_valid() {
    VideoRepair r;
    uint8_t buf[] = {0x52,0x49,0x46,0x46,0x00,0x00,0x00,0x00,
                     0x41,0x56,0x49,0x20,0x00,0x00,0x00,0x00};
    ASSERT_TRUE(r.isValidAVI(buf, 16));
}

// ═══════════════════════════════════════════════════════════════════════════
//  LOGGER TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_logger_smoke() {
    // Should not throw
    Logger::instance().setVerbose(false);
    Logger::instance().info("Test log message");
    Logger::instance().warning("Test warning");
    ASSERT_TRUE(true);
}

// ═══════════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "═══════════════════════════════════════════\n";
    std::cout << "   NeuroRecover v2.0 - Unit Test Runner\n";
    std::cout << "═══════════════════════════════════════════\n\n";

    // Hash tests
    runTest("fnv64_basic",              test_fnv64_basic);
    runTest("fnv64_different_inputs",   test_fnv64_different_inputs);
    runTest("fnv64_empty",              test_fnv64_empty);
    runTest("sha256_known_abc",         test_sha256_known);
    runTest("sha256_empty",             test_sha256_empty);

    // Dedup tests
    runTest("dedup_first_not_dup",      test_dedup_first_not_dup);
    runTest("dedup_second_is_dup",      test_dedup_second_is_dup);
    runTest("dedup_different_not_dup",  test_dedup_different_not_dup);
    runTest("dedup_counts",             test_dedup_counts);
    runTest("dedup_clear",              test_dedup_clear);

    // Signature tests
    runTest("sig_db_not_empty",         test_sig_db_not_empty);
    runTest("sig_jpeg_match",           test_sig_jpeg_match);
    runTest("sig_png_match",            test_sig_png_match);
    runTest("sig_mp4_match",            test_sig_mp4_match);
    runTest("sig_pdf_match",            test_sig_pdf_match);
    runTest("sig_zip_match",            test_sig_zip_match);
    runTest("sig_no_false_match",       test_sig_no_false_match);
    runTest("sig_forTypes_all",         test_sig_forTypes_all);
    runTest("sig_mkv_match",            test_sig_mkv_match);
    runTest("sig_flac_match",           test_sig_flac_match);
    runTest("sig_sqlite_match",         test_sig_sqlite_match);

    // AI engine tests
    runTest("ai_entropy_zeros",         test_ai_entropy_zeros);
    runTest("ai_entropy_random",        test_ai_entropy_random);
    runTest("ai_classify_jpeg",         test_ai_classify_jpeg);
    runTest("ai_classify_png",          test_ai_classify_png);
    runTest("ai_infer_jpg",             test_ai_infer_jpg);
    runTest("ai_infer_png",             test_ai_infer_png);
    runTest("ai_infer_pdf",             test_ai_infer_pdf);
    runTest("ai_infer_unknown",         test_ai_infer_unknown);

    // JPEG repair tests
    runTest("jpeg_valid_detection",     test_jpeg_valid_detection);
    runTest("jpeg_invalid_detection",   test_jpeg_invalid_detection);
    runTest("jpeg_repair_adds_eoi",     test_jpeg_repair_adds_eoi);

    // PNG repair tests
    runTest("png_valid_detection",      test_png_valid_detection);
    runTest("png_signature_check",      test_png_signature_check);

    // Video repair tests
    runTest("video_mp4_valid",          test_video_mp4_valid);
    runTest("video_avi_valid",          test_video_avi_valid);

    // Logger tests
    runTest("logger_smoke",             test_logger_smoke);

    // ── Results ───────────────────────────────────────────────────────────
    std::cout << "\n─── Results ──────────────────────────────────\n";
    for (auto& r : g_results) {
        std::cout << (r.passed ? "  [PASS] " : "  [FAIL] ") << r.name;
        if (!r.passed) std::cout << "\n         → " << r.msg;
        std::cout << "\n";
    }
    std::cout << "\n═══════════════════════════════════════════\n";
    std::cout << "  Passed: " << g_passed << " / " << (g_passed+g_failed) << "\n";
    std::cout << "  Failed: " << g_failed << "\n";
    std::cout << "═══════════════════════════════════════════\n";

    return g_failed > 0 ? 1 : 0;
}
