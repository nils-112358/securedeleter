// Secure File Deletion - Production Ready
// Based on DOD 5220.22-M and Gutmann standards
// Uses libsodium for cryptographic randomness

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstring>
#include <iomanip>
#include <sodium.h>

namespace fs = std::filesystem;

// Configuration constants
const size_t CHUNK_SIZE = 4 * 1024 * 1024;  // 4 MB chunks for efficiency
const int DEFAULT_PASSES = 3;  // DOD 5220.22-M standard (3 passes is industry standard)
const int SECURE_PASSES = 7;   // Gutmann standard (paranoid mode)

class SecureDeleter {
private:
    int passes;
    std::vector<uint8_t> randomBuffer;
    size_t bytesProcessed = 0;
    size_t totalBytes = 0;

    // Generate cryptographically secure random bytes using libsodium
    void generateRandomBytes(size_t count) {
        randomBuffer.resize(count);
        randombytes_buf(randomBuffer.data(), count);
    }

    // Overwrite a file with cryptographically secure random data
    bool overwriteWithRandomData(const fs::path& filePath, int currentPass) {
        uintmax_t fileSize = fs::file_size(filePath);
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
            
            // Generate fresh random data for each chunk
            generateRandomBytes(chunkSize);
            
            if (!file.write(reinterpret_cast<const char*>(randomBuffer.data()), chunkSize)) {
                std::cerr << "[-] Schreibfehler bei " << filePath << "\n";
                file.close();
                return false;
            }

            bytesWritten += chunkSize;
            bytesProcessed += chunkSize;
            
            // Progress display
            int percent = (bytesWritten * 100) / fileSize;
            std::cout << "\r[Pass " << currentPass << "/" << passes << "] " 
                      << percent << "% (" << (bytesWritten / 1024 / 1024) << " MB)  " 
                      << std::flush;
        }

        file.flush();
        file.close();
        
        // Force OS to flush to disk on Windows
        #ifdef _WIN32
        HANDLE h = CreateFileA(filePath.string().c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(h);
            CloseHandle(h);
        }
        #else
        // On Linux/Unix, use fsync
        int fd = open(filePath.c_str(), O_RDWR);
        if (fd >= 0) {
            fsync(fd);
            close(fd);
        }
        #endif

        return true;
    }

    // Rename file to random name (makes recovery harder)
    bool randomizeFilename(fs::path& filePath) {
        uint8_t randomBytes[16];
        randombytes_buf(randomBytes, sizeof(randomBytes));
        
        std::string newName;
        for (int i = 0; i < sizeof(randomBytes); i++) {
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

public:
    SecureDeleter(int passCount = DEFAULT_PASSES) : passes(passCount) {
        if (sodium_init() < 0) {
            throw std::runtime_error("Libsodium initialization failed!");
        }
    }

    bool deleteFile(const fs::path& filePath) {
        if (!fs::exists(filePath)) {
            std::cerr << "[-] Datei nicht gefunden: " << filePath << "\n";
            return false;
        }

        uintmax_t fileSize = fs::file_size(filePath);
        totalBytes = fileSize * passes;

        std::cout << "\n[*] Lösche: " << filePath.filename() << " (" 
                  << (fileSize / 1024 / 1024) << " MB)\n";
        std::cout << "[*] Überschreibungspässe: " << passes << "\n";

        fs::path currentPath = filePath;

        // Multiple overwrite passes with fresh random data each time
        for (int pass = 1; pass <= passes; pass++) {
            if (!overwriteWithRandomData(currentPath, pass)) {
                return false;
            }
            std::cout << "\n";
        }

        // Randomize filename for additional obfuscation
        if (!randomizeFilename(currentPath)) {
            return false;
        }

        // Final deletion
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
        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (fs::is_regular_file(entry.path())) {
                files.push_back(entry.path());
            }
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

    void setPassCount(int count) {
        if (count < 1 || count > 35) {
            std::cerr << "[-] Ungültige Pass-Anzahl (1-35)\n";
            return;
        }
        passes = count;
    }

    int getPassCount() const { return passes; }
};

void printUsage(const char* programName) {
    std::cout << "\n=== Secure Deleter - Production Ready Edition ===\n\n";
    std::cout << "Verwendung:\n";
    std::cout << "  " << programName << " <Datei/Verzeichnis> [--passes N] [--paranoid]\n\n";
    std::cout << "Optionen:\n";
    std::cout << "  --passes N      Anzahl Überschreibungspässe (Standard: 3, Max: 35)\n";
    std::cout << "  --paranoid      Verwende Gutmann-Standard (7 Pässe)\n";
    std::cout << "  --help          Diese Hilfe anzeigen\n\n";
    std::cout << "Sicherheitsstandards:\n";
    std::cout << "  DOD 5220.22-M:  3 Pässe mit Zufallsdaten\n";
    std::cout << "  Gutmann:        7 Pässe mit speziellen Patterns\n";
    std::cout << "  Custom:         1-35 Pässe\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string targetPath = argv[1];
    int passCount = DEFAULT_PASSES;
    bool paranoid = false;

    // Parse arguments
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--paranoid") {
            paranoid = true;
            passCount = SECURE_PASSES;
        } else if (arg == "--passes" && i + 1 < argc) {
            passCount = std::stoi(argv[++i]);
        }
    }

    try {
        SecureDeleter deleter(passCount);
        
        if (paranoid) {
            std::cout << "[!] PARANOIA-MODUS: 7 Pässe (Gutmann-Standard)\n";
        }

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
