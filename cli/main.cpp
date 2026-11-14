#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <csignal>
#include <cstdio>
#include "utils/config.h"
#include "utils/logger.h"
#include "core/recovery_engine.h"

static volatile bool g_interrupted = false;
void signalHandler(int) { g_interrupted = true; }

static void printBanner() {
    std::cout << R"(
 _   _                 ____                                
| \ | | ___ _   _ _ __|  _ \ ___  ___ _____   _____ _ __ 
|  \| |/ _ \ | | | '__| |_) / _ \/ __/ _ \ \ / / _ \ '__|
| |\  |  __/ |_| | |  |  _ <  __/ (_| (_) \ V /  __/ |   
|_| \_|\___|\__,_|_|  |_| \_\___|\___\___/ \_/ \___|_|   
                                           v3.0  ENHANCED
)" << "\n";
}

static void printHelp(const char* prog) {
    std::cout << "Usage: " << prog << " [options] <device> <output_dir>\n\n";
    std::cout << "File Type Selection:\n";
    std::cout << "  -t, --types TYPES      Comma-separated types (jpg,png,mp4) or 'all'\n\n";
    std::cout << "Core Recovery:\n";
    std::cout << "  -d, --deep             Deep scan (thorough, slower)\n";
    std::cout << "  -f, --repair           Repair corrupted files\n";
    std::cout << "  -r, --report           Generate recovery_report.txt\n";
    std::cout << "  -a, --metadata         Extract EXIF/GPS metadata\n\n";
    std::cout << "v3.0 New Features:\n";
    std::cout << "  -p, --preview          Generate HTML gallery (gallery.html)\n";
    std::cout << "  -c, --checkpoint       Save progress (resume if interrupted)\n";
    std::cout << "  --resume               Resume previous interrupted session\n";
    std::cout << "  --sqlite               Deep recover deleted SQLite DB records\n";
    std::cout << "  --no-encrypt-check     Skip encryption detection\n";
    std::cout << "  --filter               Enable post-recovery smart filter\n";
    std::cout << "  --min-size SIZE        Min file size  (e.g. 10KB, 1MB)\n";
    std::cout << "  --max-size SIZE        Max file size  (e.g. 500MB, 2GB)\n";
    std::cout << "  --min-conf SCORE       Min confidence 0.0-1.0 (e.g. 0.7)\n";
    std::cout << "  --filter-clean         Delete rejected files from disk\n\n";
    std::cout << "Performance:\n";
    std::cout << "  -j, --threads N        Number of threads (default: 4)\n";
    std::cout << "  --fast                 Fast mode (3-4x faster, less thorough)\n";
    std::cout << "  --turbo                Turbo mode (fastest, 8 threads, 32MB buffer)\n";
    std::cout << "  --no-dedup             Disable deduplication\n";
    std::cout << "  --start BYTES          Start scan at byte offset\n";
    std::cout << "  --end BYTES            Stop scan at byte offset\n\n";
    std::cout << "Logging:\n";
    std::cout << "  -v, --verbose          Verbose output\n";
    std::cout << "  --log FILE             Log file path\n";
    std::cout << "  -h, --help             Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " //./PhysicalDrive1 C:/out -t all -d -f -r -p\n";
    std::cout << "  " << prog << " //./PhysicalDrive1 C:/photos -t jpg,png -f -a -p --min-size 50KB\n";
    std::cout << "  " << prog << " //./PhysicalDrive1 C:/out -t all -d -c        (checkpoint)\n";
    std::cout << "  " << prog << " //./PhysicalDrive1 C:/out --resume             (resume)\n";
    std::cout << "  " << prog << " //./PhysicalDrive1 C:/out -t all --sqlite      (SQLite)\n";
    std::cout << "  " << prog << " //./PhysicalDrive1 C:/out --filter --min-size 100KB --min-conf 0.8\n\n";
}

static std::vector<std::string> splitCSV(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) {
            std::transform(tok.begin(), tok.end(), tok.begin(), ::tolower);
            out.push_back(tok);
        }
    }
    return out;
}

int main(int argc, char* argv[]) {
    // CRITICAL: disable stdout buffering so Node.js receives output in real-time
    // Without this, when spawned as child process, output is fully buffered
    // and Node.js receives nothing until the process exits
    std::ios::sync_with_stdio(false);
    std::cout.setf(std::ios::unitbuf);  // unbuffered cout
    setvbuf(stdout, nullptr, _IONBF, 0); // unbuffered C stdout
    setvbuf(stderr, nullptr, _IONBF, 0); // unbuffered C stderr

    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);
    printBanner();

    if (argc < 2) { printHelp(argv[0]); return 1; }

    RecoveryConfig config;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Option " << arg << " requires a value.\n";
                exit(1);
            }
            return argv[++i];
        };

        if      (arg=="-h"||arg=="--help")           { printHelp(argv[0]); return 0; }
        else if (arg=="-d"||arg=="--deep")             config.deepScan         = true;
        else if (arg=="-f"||arg=="--repair")           config.repairFiles      = true;
        else if (arg=="-r"||arg=="--report")           config.generateReport   = true;
        else if (arg=="-a"||arg=="--metadata")         config.extractMetadata  = true;
        else if (arg=="-p"||arg=="--preview")          config.generatePreview  = true;
        else if (arg=="-c"||arg=="--checkpoint")       config.enableCheckpoint = true;
        else if (arg=="--resume")                      config.resumeSession    = true;
        else if (arg=="--sqlite")                      config.deepSQLite       = true;
        else if (arg=="-v"||arg=="--verbose")          config.verbose          = true;
        else if (arg=="--fast")                        config.fastMode         = true;
        else if (arg=="--turbo") {
            // Turbo: 16 threads, 128MB buffer, zero-skip, fast mode
            config.fastMode   = true;
            config.numThreads = 16;
            config.readBuffer = 128 * 1024 * 1024;
        }
        else if (arg=="--no-dedup")                    config.deduplication    = false;
        else if (arg=="--no-encrypt-check")            config.detectEncryption = false;
        else if (arg=="--filter")                      config.smartFilter      = true;
        else if (arg=="--filter-clean")               { config.smartFilter=true; config.filterClean=true; }
        else if (arg=="-t"||arg=="--types") {
            std::string t = next();
            if (t == "all") config.recoverAll = true;
            else            config.fileTypes  = splitCSV(t);
        }
        else if (arg=="-j"||arg=="--threads") {
            config.numThreads = std::stoi(next());
            config.numThreads = std::max(1, std::min(32, config.numThreads));
        }
        else if (arg=="--log")       config.logFile      = next();
        else if (arg=="--start")     config.startOffset  = std::stoull(next());
        else if (arg=="--end")       config.endOffset    = std::stoull(next());
        else if (arg=="--min-size") { config.filterMinSize=next(); config.smartFilter=true; }
        else if (arg=="--max-size") { config.filterMaxSize=next(); config.smartFilter=true; }
        else if (arg=="--min-conf") { config.filterMinConf=std::stod(next()); config.smartFilter=true; }
        else if (arg[0] != '-')      positional.push_back(arg);
        else { std::cerr << "Unknown option: " << arg << "\n"; return 1; }
    }

    if (positional.size() < 2) {
        std::cerr << "Error: <device> and <output_dir> required.\n\n";
        printHelp(argv[0]);
        return 1;
    }
    config.devicePath = positional[0];
    config.outputDir  = positional[1];

    if (config.fileTypes.empty() && !config.recoverAll) {
        config.recoverAll = true;
        std::cout << "[INFO] No types specified — recovering all formats.\n";
    }

    // If resume requested, auto-enable checkpoint too
    if (config.resumeSession) config.enableCheckpoint = true;

    // Setup logging
    Logger::instance().setVerbose(config.verbose);
    Logger::instance().setLogFile(config.outputDir + "/" + config.logFile);

    // Print summary
    std::cout << "┌─────────────────────────────────────────┐\n";
    std::cout << "│         NeuroRecover v3.0 Config         │\n";
    std::cout << "├─────────────────────────────────────────┤\n";
    std::cout << "│ Device      : " << std::setw(26) << std::left << config.devicePath  << "│\n";
    std::cout << "│ Output      : " << std::setw(26) << std::left << config.outputDir   << "│\n";
    std::cout << "│ Threads     : " << std::setw(26) << std::left << config.numThreads  << "│\n";
    std::cout << "│ Deep scan   : " << std::setw(26) << std::left << (config.deepScan?"yes":"no") << "│\n";
    std::cout << "│ Repair      : " << std::setw(26) << std::left << (config.repairFiles?"yes":"no") << "│\n";
    std::cout << "│ Preview     : " << std::setw(26) << std::left << (config.generatePreview?"yes":"no") << "│\n";
    std::cout << "│ Checkpoint  : " << std::setw(26) << std::left << (config.enableCheckpoint?"yes":"no") << "│\n";
    std::cout << "│ Resume      : " << std::setw(26) << std::left << (config.resumeSession?"yes":"no") << "│\n";
    std::cout << "│ SQLite deep : " << std::setw(26) << std::left << (config.deepSQLite?"yes":"no") << "│\n";
    std::cout << "│ Smart filter: " << std::setw(26) << std::left << (config.smartFilter?"yes":"no") << "│\n";
    std::cout << "└─────────────────────────────────────────┘\n\n";

    RecoveryEngine engine(config);

    engine.setProgressCallback([](const std::string& phase, double) {
        static std::string last;
        if (phase != last) {
            std::cout << "\n══ Phase: " << phase << " ══\n";
            last = phase;
        }
    });

    engine.setFileCallback([](const std::string& path, const std::string& ext) {
        std::cout << "  [+] " << path << " (" << ext << ")\n";
    });

    try {
        auto stats = engine.run();

        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════╗\n";
        std::cout << "║   NeuroRecover v3.0  —  Recovery Done!   ║\n";
        std::cout << "╠══════════════════════════════════════════╣\n";
        std::cout << "║  Files recovered  : " << std::setw(20) << std::left << stats.filesFound        << "║\n";
        std::cout << "║  Files repaired   : " << std::setw(20) << std::left << stats.filesRepaired     << "║\n";
        std::cout << "║  Dups skipped     : " << std::setw(20) << std::left << stats.filesDeduplicated << "║\n";
        std::cout << "║  Filtered out     : " << std::setw(20) << std::left << stats.filesFiltered     << "║\n";
        std::cout << "║  SQLite records   : " << std::setw(20) << std::left << stats.sqliteRecords     << "║\n";
        std::cout << "║  Filesystem       : " << std::setw(20) << std::left << stats.fsType            << "║\n";
        if (!stats.encryptionType.empty())
            std::cout << "║  Encryption  ⚠️   : " << std::setw(20) << std::left << stats.encryptionType << "║\n";
        std::cout << "║  Time elapsed     : " << std::setw(17) << std::left
                  << std::to_string((int)stats.elapsedSeconds) + "s" << "   ║\n";
        std::cout << "╚══════════════════════════════════════════╝\n\n";

        std::cout << "📁 Output folder  : " << config.outputDir << "\n";
        if (config.generateReport)
            std::cout << "📄 Report         : " << config.outputDir << "/recovery_report.txt\n";
        if (config.generatePreview)
            std::cout << "🖼  Gallery        : " << config.outputDir << "/gallery.html\n";
        std::cout << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "\nFatal error: " << ex.what() << "\n";
        return 1;
    }
}