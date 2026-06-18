// Secure File Deletion - Production Ready with Multi-Layer Entropy
// Based on DOD 5220.22-M and Gutmann standards
// Uses libsodium + multiple entropy sources for maximum security

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstring>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cmath>
#include <sodium.h>

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

// Configuration constants
const size_t CHUNK_SIZE = 4 * 1024 * 1024;  // 4 MB chunks for efficiency
const int DEFAULT_PASSES = 3;  // DOD 5220.22-M standard
const int SECURE_PASSES = 7;   // Gutmann standard (paranoid mode)

// Entropy layer flags
enum EntropyLayers {
    ENTROPY_LIBSODIUM = 1,      // Layer 1: Cryptographic RNG (always on)
    ENTROPY_TIMING = 2,          // Layer 2: System timing (CPU cycles, memory access)
    ENTROPY_MOUSE = 4,           // Layer 3: Mouse movements (optional)
    ENTROPY_DISK_IO = 8,         // Layer 4: Disk I/O timing jitter
    ENTROPY_SYSTEM = 16          // Layer 5: System state (load, memory, etc)
};

class EntropyCollector {
private:
    int layerFlags;
    std::vector<uint8_t> entropyPool;

public:
    EntropyCollector(int flags = ENTROPY_LIBSODIUM) : layerFlags(flags) {}

    // Layer 1: libsodium (cryptographically secure, always active)
    void collectLibsodiumEntropy(size_t count) {
        std::vector<uint8_t> buffer(count);
        randombytes_buf(buffer.data(), count);
        for (size_t i = 0; i < count; i++) {
            entropyPool[i] ^= buffer[i];
        }
    }

    // Layer 2: System timing (CPU cycles, memory latency)
    void collectTimingEntropy(size_t count) {
        if (!(layerFlags & ENTROPY_TIMING)) return;

        std::cout << "[*] Sammle Timing-Entropie (CPU-Zyklen)...\n";
        
        std::vector<uint64_t> timings;
        volatile uint64_t dummy = 0;

        for (size_t i = 0; i < count / 8; i++) {
            auto start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            
            // Dummy computation to add timing variance
            for (int j = 0; j < 1000; j++) {
                dummy += (dummy * 33 + 17) ^ (start >> j);
            }
            
            auto end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            uint64_t delta = (end - start) ^ dummy;
            timings.push_back(delta);
        }

        // XOR timings into entropy pool
        for (size_t i = 0; i < timings.size() && i * 8 < count; i++) {
            uint64_t timing = timings[i];
            for (int j = 0; j < 8 && i * 8 + j < count; j++) {
                entropyPool[i * 8 + j] ^= static_cast<uint8_t>((timing >> (j * 8)) & 0xFF);
            }
        }
        std::cout << "[+] Timing-Entropie gesammelt\n";
    }

    // Layer 3: Mouse movement entropy (Windows only)
    void collectMouseEntropy(size_t count) {
        if (!(layerFlags & ENTROPY_MOUSE)) return;

#ifdef _WIN32
        std::cout << "[*] Sammle Maus-Entropie - bewege die Maus für 2 Sekunden wild...\n";
        
        std::vector<uint32_t> mouseData;
        auto startTime = std::chrono::steady_clock::now();
        int moveCount = 0;

        while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - startTime).count() < 2000) {
            POINT p;
            if (GetCursorPos(&p)) {
                uint32_t data = static_cast<uint32_t>(p.x) ^ 
                               (static_cast<uint32_t>(p.y) << 16) ^
                               static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
                mouseData.push_back(data);
                moveCount++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        // XOR mouse data into entropy pool
        for (size_t i = 0; i < mouseData.size() && i * 4 < count; i++) {
            uint32_t data = mouseData[i];
            for (int j = 0; j < 4 && i * 4 + j < count; j++) {
                entropyPool[i * 4 + j] ^= static_cast<uint8_t>((data >> (j * 8)) & 0xFF);
            }
        }
        std::cout << "[+] Maus-Entropie gesammelt (" << moveCount << " Bewegungen)\n";
#else
        (void)count;  // Suppress unused parameter warning on Linux
        std::cout << "[!] Maus-Entropie nur auf Windows verfügbar\n";
#endif
    }

    // Layer 4: Disk I/O timing jitter
    void collectDiskIOEntropy(size_t count) {
        if (!(layerFlags & ENTROPY_DISK_IO)) return;

        std::cout << "[*] Sammle Disk-I/O Entropie...\n";
        
        // Create temp file for timing measurements
        std::string tempFile = "/tmp/entropy_io_test_" + std::to_string(getpid());
        std::vector<uint64_t> ioTimings;

        for (size_t i = 0; i < count / 16; i++) {
            std::ofstream test(tempFile, std::ios::binary);
            if (!test.is_open()) break;

            auto start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            
            // Write variable-sized chunks
            uint8_t buffer[256];
            randombytes_buf(buffer, sizeof(buffer));
            test.write(reinterpret_cast<char*>(buffer), sizeof(buffer));
            test.flush();

            auto end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            
            test.close();
            ioTimings.push_back(static_cast<uint64_t>(end - start));

            // Small delay
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        // Clean up
        try { fs::remove(tempFile); } catch (...) {}

        // XOR I/O timings into entropy pool
        for (size_t i = 0; i < ioTimings.size() && i * 8 < count; i++) {
            uint64_t timing = ioTimings[i];
            for (int j = 0; j < 8 && i * 8 + j < count; j++) {
                entropyPool[i * 8 + j] ^= static_cast<uint8_t>((timing >> (j * 8)) & 0xFF);
            }
        }
        std::cout << "[+] Disk-I/O Entropie gesammelt\n";
    }

    // Layer 5: System state entropy (load average, memory, etc)
    void collectSystemEntropy(size_t count) {
        if (!(layerFlags & ENTROPY_SYSTEM)) return;

        std::cout << "[*] Sammle System-Entropie...\n";

#ifdef _WIN32
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            uint64_t memData = (memStatus.ullAvailPhys ^ memStatus.ullTotalPhys);
            for (size_t i = 0; i < 8 && i < count; i++) {
                entropyPool[i] ^= static_cast<uint8_t>((memData >> (i * 8)) & 0xFF);
            }
        }
#else
        // Linux: read from /proc/stat for CPU times
        std::ifstream stat("/proc/stat");
        if (stat.is_open()) {
            std::string line;
            uint64_t cpuData = 0;
            for (int i = 0; i < 3 && std::getline(stat, line); i++) {
                for (char c : line) {
                    cpuData = (cpuData << 5) ^ (cpuData >> 27) ^ static_cast<uint8_t>(c);
                }
            }
            stat.close();
            
            for (size_t i = 0; i < 8 && i < count; i++) {
                entropyPool[i] ^= static_cast<uint8_t>((cpuData >> (i * 8)) & 0xFF);
            }
        }
#endif
        std::cout << "[+] System-Entropie gesammelt\n";
    }

    // Initialize and collect all enabled entropy layers
    std::vector<uint8_t> generateEntropy(size_t count) {
        entropyPool.resize(count);
        
        // Layer 1: Always collect libsodium entropy
        std::cout << "[*] Layer 1: Libsodium-Entropie (Hauptquelle)...\n";
        collectLibsodiumEntropy(count);
        
        // Layer 2: Timing entropy
        if (layerFlags & ENTROPY_TIMING) {
            std::cout << "[*] Layer 2: System-Timing-Entropie...\n";
            collectTimingEntropy(count);
        }
        
        // Layer 3: Mouse entropy
        if (layerFlags & ENTROPY_MOUSE) {
            std::cout << "[*] Layer 3: Maus-Entropie...\n";
            collectMouseEntropy(count);
        }
        
        // Layer 4: Disk I/O entropy
        if (layerFlags & ENTROPY_DISK_IO) {
            std::cout << "[*] Layer 4: Disk-I/O Entropie...\n";
            collectDiskIOEntropy(count);
        }
        
        // Layer 5: System state entropy
        if (layerFlags & ENTROPY_SYSTEM) {
            std::cout << "[*] Layer 5: System-Status-Entropie...\n";
            collectSystemEntropy(count);
        }
        
        return entropyPool;
    }
};

class SecureDeleter {
private:
    int passes;
    std::vector<uint8_t> randomBuffer;
    size_t bytesProcessed = 0;
    size_t totalBytes = 0;
    EntropyCollector entropyCollector;

public:
    SecureDeleter(int passCount = DEFAULT_PASSES, int entropyLayers = ENTROPY_LIBSODIUM) 
        : passes(passCount), entropyCollector(entropyLayers) {
        if (sodium_init() < 0) {
            throw std::runtime_error("Libsodium initialization failed!");
        }
    }

    void generateRandomBytes(size_t count) {
        std::vector<uint8_t> entropy = entropyCollector.generateEntropy(count);
        randomBuffer = entropy;
    }

    bool overwriteWithRandomData(const fs::path& filePath, int currentPass) {
        uintmax_t fileSize = fs::file_size(filePath);
        
        if (fileSize == 0) {
            std::cout << "[*] Datei ist leer, überspringe Überschreibung\n";
            return true;
        }

        std::ofstream file(filePath, std::ios::binary);
        
        if (!file.is_open()) {
            std::cerr << "[-] Fehler: Kann Datei nicht öffnen: " << filePath << "\n";
            return false;
        }

        bytesProcessed = 0;
        uintmax_t bytesWritten = 0;

        while (bytesWritten < fileSize) {
            uintmax_t remaining = fileSize - bytesWritten;
            size_t chunkSize = std::min(static_cast<uintmax_t>(CHUNK_SIZE), remaining);
            
            generateRandomBytes(chunkSize);
            
            if (!file.write(reinterpret_cast<const char*>(randomBuffer.data()), chunkSize)) {
                std::cerr << "[-] Schreibfehler bei " << filePath << "\n";
                file.close();
                return false;
            }

            bytesWritten += chunkSize;
            bytesProcessed += chunkSize;
            
            int percent = (bytesWritten * 100) / fileSize;
            std::cout << "\r[Pass " << currentPass << "/" << passes << "] " 
                      << percent << "% (" << (bytesWritten / 1024 / 1024) << " MB)  " 
                      << std::flush;
        }

        file.flush();
        file.close();
        
        // Force OS to flush to disk
        #ifdef _WIN32
        HANDLE h = CreateFileA(filePath.string().c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(h);
            CloseHandle(h);
        }
        #else
        int fd = open(filePath.c_str(), O_RDWR);
        if (fd >= 0) {
            fsync(fd);
            close(fd);
        }
        #endif

        return true;
    }

    bool randomizeFilename(fs::path& filePath) {
        uint8_t randomBytes[16];
        randombytes_buf(randomBytes, sizeof(randomBytes));
        
        std::string newName;
        for (size_t i = 0; i < sizeof(randomBytes); i++) {
            char hex[3];
            snprintf(hex, sizeof(hex), "%02x", randomBytes[i]);
            newName += hex;
        }

        fs::path newPath = filePath.parent_path() / newName;
        try {
            fs::rename(filePath, newPath);
            filePath = newPath;
            return true;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[-] Rename-Fehler: " << e.what() << "\n";
            return false;
        }
    }

    bool deleteFile(const fs::path& filePath) {
        if (!fs::exists(filePath)) {
            std::cerr << "[-] Datei nicht gefunden: " << filePath << "\n";
            return false;
        }

        if (!fs::is_regular_file(filePath)) {
            std::cerr << "[-] Ist keine reguläre Datei: " << filePath << "\n";
            return false;
        }

        uintmax_t fileSize = fs::file_size(filePath);
        totalBytes = fileSize * passes;

        std::cout << "\n[*] Lösche: " << filePath.filename() << " (" 
                  << (fileSize / 1024 / 1024) << " MB)\n";
        std::cout << "[*] Überschreibungspässe: " << passes << "\n";

        fs::path currentPath = filePath;

        for (int pass = 1; pass <= passes; pass++) {
            if (!overwriteWithRandomData(currentPath, pass)) {
                return false;
            }
            std::cout << "\n";
        }

        if (!randomizeFilename(currentPath)) {
            return false;
        }

        try {
            fs::remove(currentPath);
            std::cout << "[+] Datei erfolgreich gelöscht!\n";
            return true;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[-] Fehler beim Löschen: " << e.what() << "\n";
            return false;
        }
    }

    bool deleteDirectory(const fs::path& dirPath) {
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            std::cerr << "[-] Verzeichnis nicht gefunden: " << dirPath << "\n";
            return false;
        }

        std::vector<fs::path> files;
        try {
            for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
                if (fs::is_regular_file(entry.path())) {
                    files.push_back(entry.path());
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[-] Fehler beim Durchsuchen: " << e.what() << "\n";
            return false;
        }

        if (files.empty()) {
            std::cout << "[*] Keine Dateien zum Löschen gefunden.\n";
            return true;
        }

        std::cout << "[!] " << files.size() << " Dateien gefunden.\n";
        std::cout << "[!] Warnung: Diese Operation ist NICHT rückgängig zu machen!\n";
        std::cout << "Fortfahren? (y/N): ";
        
        char confirm;
        std::cin >> confirm;
        if (confirm != 'y' && confirm != 'Y') {
            std::cout << "[-] Abgebrochen.\n";
            return false;
        }

        bool allSuccess = true;
        for (size_t i = 0; i < files.size(); i++) {
            std::cout << "\n[" << (i + 1) << "/" << files.size() << "]\n";
            if (!deleteFile(files[i])) {
                allSuccess = false;
            }
        }

        return allSuccess;
    }
};

void printUsage(const char* programName) {
    std::cout << "\n=== Secure Deleter - Production Ready Edition (Multi-Layer Entropy) ===\n\n";
    std::cout << "Verwendung:\n";
    std::cout << "  " << programName << " <Datei/Verzeichnis> [Optionen]\n\n";
    std::cout << "Optionen:\n";
    std::cout << "  --passes N         Anzahl Überschreibungspässe (Standard: 3, Max: 35)\n";
    std::cout << "  --paranoid         Verwende Gutmann-Standard (7 Pässe)\n";
    std::cout << "  --entropy-mouse    Aktiviere Maus-Entropie\n";
    std::cout << "  --entropy-sys      Aktiviere System-Entropie\n";
    std::cout << "  --entropy-all      Aktiviere ALLE Entropie-Layer\n";
    std::cout << "  --help             Diese Hilfe anzeigen\n\n";
    std::cout << "Entropie-Layer:\n";
    std::cout << "  Layer 1: libsodium (Hauptquelle, immer aktiv)\n";
    std::cout << "  Layer 2: System-Timing (CPU-Zyklen, Memory-Latenz)\n";
    std::cout << "  Layer 3: Maus-Bewegungen (optional, Windows)\n";
    std::cout << "  Layer 4: Disk-I/O Jitter (optional)\n";
    std::cout << "  Layer 5: System-Status (optional, Load/Memory)\n\n";
    std::cout << "Beispiele:\n";
    std::cout << "  " << programName << " secret.txt\n";
    std::cout << "  " << programName << " secret.txt --entropy-all\n";
    std::cout << "  " << programName << " sensitive/ --paranoid --entropy-mouse\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string targetPath = argv[1];
    int passCount = DEFAULT_PASSES;
    int entropyLayers = ENTROPY_LIBSODIUM;
    bool paranoid = false;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--paranoid") {
            paranoid = true;
            passCount = SECURE_PASSES;
        } else if (arg == "--entropy-mouse") {
            entropyLayers |= ENTROPY_MOUSE;
        } else if (arg == "--entropy-sys") {
            entropyLayers |= ENTROPY_SYSTEM;
        } else if (arg == "--entropy-all") {
            entropyLayers |= ENTROPY_TIMING | ENTROPY_MOUSE | ENTROPY_DISK_IO | ENTROPY_SYSTEM;
        } else if (arg == "--passes" && i + 1 < argc) {
            try {
                passCount = std::stoi(argv[++i]);
            } catch (const std::exception& e) {
                std::cerr << "[-] Fehler: Ungültige Pass-Anzahl: " << e.what() << "\n";
                return 1;
            }
        }
    }

    try {
        SecureDeleter deleter(passCount, entropyLayers);
        
        if (paranoid) {
            std::cout << "[!] PARANOIA-MODUS: 7 Pässe (Gutmann-Standard)\n";
        }
        
        if (entropyLayers & ENTROPY_TIMING) std::cout << "[+] Layer 2: System-Timing aktiviert\n";
        if (entropyLayers & ENTROPY_MOUSE) std::cout << "[+] Layer 3: Maus-Entropie aktiviert\n";
        if (entropyLayers & ENTROPY_DISK_IO) std::cout << "[+] Layer 4: Disk-I/O aktiviert\n";
        if (entropyLayers & ENTROPY_SYSTEM) std::cout << "[+] Layer 5: System-Status aktiviert\n";

        if (fs::is_regular_file(targetPath)) {
            return deleter.deleteFile(targetPath) ? 0 : 1;
        } else if (fs::is_directory(targetPath)) {
            return deleter.deleteDirectory(targetPath) ? 0 : 1;
        } else {
            std::cerr << "[-] Ungültiger Pfad: " << targetPath << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[-] Fehler: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
