// main.cpp
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <random>
#include <thread>
#include <cstdlib>
#include <new>     // Für std::bad_alloc

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/sysinfo.h> // Für Linux RAM-Auslesung, falls dort genutzt
#endif

namespace fs = std::filesystem;

// --- SHA-256 Implementierung (unverändert) ---
class SHA256 {
protected:
    typedef unsigned int uint32;
    const static uint32 sha256_k[];
    static const unsigned int SHA224_256_BLOCK_SIZE = 64;
    void transform(const unsigned char *message, unsigned int block_nb);
    unsigned int m_tot_len;
    unsigned int m_len;
    unsigned char m_block[128];
    uint32 m_h[8];
public:
    void init();
    void update(const unsigned char *message, unsigned int len);
    void final(unsigned char *digest);
    static const unsigned int DIGEST_SIZE = 32;
};

const unsigned int SHA256::sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19666f4e, 0x283ba228, 0x4a2860b7, 0x523357bd, 0x5f064402, 0x71fc2416, 0x74e2ee28, 0x982d5b3c,
    0xb0c26432, 0x5be0cd19, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d
};

#define SHA2_SHFR(x, n)    (x >> n)
#define SHA2_ROTR(x, n)   ((x >> n) | (x << ((sizeof(x) << 3) - n)))
#define SHA2_CH(x, y, z)  ((x & y) ^ (~x & z))
#define SHA2_MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define SHA256_F1(x) (SHA2_ROTR(x,  2) ^ SHA2_ROTR(x, 13) ^ SHA2_ROTR(x, 22))
#define SHA256_F2(x) (SHA2_ROTR(x,  6) ^ SHA2_ROTR(x, 11) ^ SHA2_ROTR(x, 25))
#define SHA256_F3(x) (SHA2_ROTR(x,  7) ^ SHA2_ROTR(x, 18) ^ SHA2_SHFR(x,  3))
#define SHA256_F4(x) (SHA2_ROTR(x, 17) ^ SHA2_ROTR(x, 19) ^ SHA2_SHFR(x, 10))

void SHA256::transform(const unsigned char *message, unsigned int block_nb) {
    uint32 w[64]; uint32 wv[8]; uint32 t1, t2; const unsigned char *sub_block;
    for (unsigned int i = 0; i < block_nb; i++) {
        sub_block = message + (i << 6);
        for (unsigned int j = 0; j < 16; j++) {
            w[j] =  (sub_block[j << 2] << 24) | (sub_block[(j << 2) + 1] << 16) | (sub_block[(j << 2) + 2] << 8) | (sub_block[(j << 2) + 3]);
        }
        for (unsigned int j = 16; j < 64; j++) w[j] = SHA256_F4(w[j - 2]) + w[j - 7] + SHA256_F3(w[j - 15]) + w[j - 16];
        for (unsigned int j = 0; j < 8; j++) wv[j] = m_h[j];
        for (unsigned int j = 0; j < 64; j++) {
            t1 = wv[7] + SHA256_F2(wv[4]) + SHA2_CH(wv[4], wv[5], wv[6]) + sha256_k[j] + w[j];
            t2 = SHA256_F1(wv[0]) + SHA2_MAJ(wv[0], wv[1], wv[2]);
            wv[7] = wv[6]; wv[6] = wv[5]; wv[5] = wv[4]; wv[4] = wv[3] + t1;
            wv[3] = wv[2]; wv[2] = wv[1]; wv[1] = wv[0]; wv[0] = t1 + t2;
        }
        for (unsigned int j = 0; j < 8; j++) m_h[j] += wv[j];
    }
}
void SHA256::init() {
    m_h[0] = 0x6a09e667; m_h[1] = 0xbb67ae85; m_h[2] = 0x3c6ef372; m_h[3] = 0xa54ff53a;
    m_h[4] = 0x510e527f; m_h[5] = 0x9b05688c; m_h[6] = 0x1f83d9ab; m_h[7] = 0x5be0cd19;
    m_len = 0; m_tot_len = 0;
}
void SHA256::update(const unsigned char *message, unsigned int len) {
    unsigned int block_nb; unsigned int rem_len = SHA224_256_BLOCK_SIZE - m_len; unsigned int fill_len = (len < rem_len) ? len : rem_len;
    for(unsigned int i=0; i<fill_len; i++) m_block[m_len + i] = message[i];
    if (m_len + len < SHA224_256_BLOCK_SIZE) { m_len += len; return; }
    transform(m_block, 1); m_tot_len += SHA224_256_BLOCK_SIZE;
    block_nb = (len - fill_len) / SHA224_256_BLOCK_SIZE; transform(message + fill_len, block_nb);
    m_tot_len += block_nb * SHA224_256_BLOCK_SIZE; m_len = (len - fill_len) % SHA224_256_BLOCK_SIZE;
    for(unsigned int i=0; i<m_len; i++) m_block[i] = message[fill_len + (block_nb * SHA224_256_BLOCK_SIZE) + i];
}
void SHA256::final(unsigned char *digest) {
    unsigned int block_nb = (1 + ((SHA224_256_BLOCK_SIZE - 9) < m_len)); unsigned int len_b = (m_tot_len + m_len) << 3;
    m_block[m_len] = 0x80; unsigned int pm_len = block_nb * SHA224_256_BLOCK_SIZE;
    for (unsigned int i = m_len + 1; i < pm_len; i++) m_block[i] = 0;
    m_block[pm_len - 4] = (len_b >> 24) & 0xFF; m_block[pm_len - 3] = (len_b >> 16) & 0xFF;
    m_block[pm_len - 2] = (len_b >> 8) & 0xFF; m_block[pm_len - 1] = len_b & 0xFF;
    transform(m_block, block_nb);
    for (int i = 0; i < 8; i++) {
        digest[i << 2] = (m_h[i] >> 24) & 0xFF; digest[(i << 2) + 1] = (m_h[i] >> 16) & 0xFF;
        digest[(i << 2) + 2] = (m_h[i] >> 8) & 0xFF; digest[(i << 2) + 3] = m_h[i] & 0xFF;
    }
}
std::string get_sha256(const std::string& input) {
    unsigned char digest[SHA256::DIGEST_SIZE]; SHA256 ctx; ctx.init();
    ctx.update((const unsigned char*)input.c_str(), input.length()); ctx.final(digest);
    std::stringstream ss;
    for(int i = 0; i < SHA256::DIGEST_SIZE; ++i) ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return ss.str();
}

struct YouTubeEntropy {
    std::string videoTitle = "DefaultTitle";
    int videoLength = 360; 
    std::vector<unsigned char> iconBytes;
};

// Funktion zur Auslastung und Zerstörung von RAM-Fragmenten
void nuclearRamPurge() {
    std::cout << "\n[!] Starte nukleare RAM-Bereinigung (RAM Remanence Protection)...\n";
    std::cout << "[*] Allokiere und ueberschreibe verbleibende Fragmente im Arbeitsspeicher...\n";

    size_t blockSize = 100 * 1024 * 1024; // 100 MB Blöcke
    std::vector<std::vector<char>*> memoryHogs;
    
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 255);

    // Schleife läuft, bis der RAM voll ist und std::bad_alloc wirft
    try {
        while (true) {
            std::vector<char>* block = new std::vector<char>(blockSize);
            
            // Block sofort mit mathematischem Rauschen füllen und CPU belasten
            char noise = static_cast<char>(dist(rng));
            std::fill(block->begin(), block->end(), noise);
            
            // Ein Alibi-Hash berechnen, um massive CPU-Zyklen auf dem RAM-Block zu erzwingen
            std::string sample(block->begin(), block->begin() + 64);
            get_sha256(sample);

            memoryHogs.push_back(block);
            std::cout << "\r[+] " << (memoryHogs.size() * 100) << " MB RAM gesaeubert und ueberschrieben..." << std::flush;
            
            // Kurze Atempause, damit Windows nicht abstürzt
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    } 
    catch (const std::bad_alloc&) {
        // RAM-Limit erreicht! Das Betriebssystem verweigert weiteren Speicher.
        std::cout << "\n[+] Maximal verfuegbarer RAM wurde komplett ueberschrieben.\n";
    }

    std::cout << "[*] Gebe gesaeuberten Arbeitsspeicher wieder frei... " << std::flush;
    // Speicher sauber freigeben, damit das System wieder normal läuft
    for (auto block : memoryHogs) {
        delete block;
    }
    std::cout << "[OK]\n";
}

YouTubeEntropy gatherYouTubeEntropy() {
    std::cout << "[*] Kontaktiere globale API für Echtzeit-Entropie...\n";
    YouTubeEntropy yt;
    std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
    std::string randomQuery = "";
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, chars.length() - 1);
    for(int i=0; i<3; ++i) randomQuery += chars[dist(rng)];

#ifdef _WIN32
    std::string cmd = "powershell -Command \"try { "
                      "$html = Invoke-WebRequest -Uri 'https://www.youtube.com/results?search_query=" + randomQuery + "' -UserAgent 'Mozilla/5.0' -TimeoutSec 5; "
                      "if ($html.Content -match '\\\"title\\\":{\\\"runs\\\":\\[{\\\"text\\\":\\\"([^\\\"]+)\\\"') { $title = $Matches[1]; Out-File -FilePath 'yt_t.txt' -InputObject $title -Encoding utf8; } "
                      "if ($html.Content -match '\\\"lengthText\\\":{\\\"accessibility\\\":{\\\"accessibilityData\\\":{\\\"label\\\":\\\"([^\\\"]+)\\\"') { $len = $Matches[1]; Out-File -FilePath 'yt_l.txt' -InputObject $len; } "
                      "if ($html.Content -match '(https://i\\.ytimg\\.com/vi/[^\\\"]/hqdefault\\.jpg)') { Invoke-WebRequest -Uri $Matches[1] -OutFile 'yt_i.jpg' -TimeoutSec 3; } "
                      "} catch {} \"";
    std::system(cmd.c_str());
#else
    std::string cmd = "curl -s -A 'Mozilla/5.0' 'https://www.youtube.com/results?search_query=" + randomQuery + "' | grep -o '\"title\":{\"runs\":[{\"text\":\"[^\"]*\"' | head -n 1 | cut -d'\"' -f8 > yt_t.txt";
    std::system(cmd.c_str());
#endif

    std::ifstream tFile("yt_t.txt");
    if (tFile.is_open()) { std::getline(tFile, yt.videoTitle); tFile.close(); fs::remove("yt_t.txt"); }
    std::ifstream lFile("yt_l.txt");
    if (lFile.is_open()) { std::string lenStr; std::getline(lFile, lenStr); lFile.close(); fs::remove("yt_l.txt"); yt.videoLength = std::max(10, static_cast<int>(lenStr.length() * 42)); }
    std::ifstream iFile("yt_i.jpg", std::ios::binary);
    if (iFile.is_open()) { yt.iconBytes = std::vector<unsigned char>((std::istreambuf_iterator<char>(iFile)), std::istreambuf_iterator<char>()); iFile.close(); fs::remove("yt_i.jpg"); }
    if (yt.iconBytes.empty()) { for(int i=0; i<1024; ++i) yt.iconBytes.push_back(dist(rng) * 3); }
    std::cout << "[+] Globale Entropie geladen: \"" << yt.videoTitle.substr(0, 20) << "...\" (" << yt.videoLength << "s Video)\n";
    return yt;
}

inline const std::vector<unsigned char> STINKEFINGER_WEBP = {
    0x52, 0x49, 0x46, 0x46, 0x44, 0x24, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50, 0x56, 0x50, 0x38, 0x4C,
    0x37, 0x24, 0x00, 0x00, 0x2F, 0x57, 0x02, 0x00, 0x10, 0x8D, 0x25, 0x88, 0x21, 0xDE, 0x93, 0xC3,
    0x7F, 0x12, 0x42, 0x20, 0x2C, 0xD3, 0x2F, 0x6E, 0xEE, 0x51, 0x76, 0x3F, 0xBC, 0x61, 0x05, 0x00,
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x02, 0x58, 0x00, 0x00, 0x02, 0x58, 0x08, 0x06, 0x00, 0x00, 0x00, 0xE4, 0x1F, 0x2C,
    0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};

const size_t CHUNK_SIZE = 64 * 1024;

bool secureShred(const fs::path& filePath, const std::vector<unsigned char>& imageData, int passes, int ytLength) {
    if (imageData.empty()) return false; uintmax_t fileSize = fs::file_size(filePath);
    std::vector<char> blockBuffer(CHUNK_SIZE); size_t imageCursor = 0;
    for (int pass = 1; pass <= passes; ++pass) {
        std::ofstream file(filePath, std::ios::binary | std::ios::in | std::ios::out); if (!file.is_open()) return false;
        uintmax_t bytesWritten = 0;
        while (bytesWritten < fileSize) {
            uintmax_t remaining = fileSize - bytesWritten; size_t currentChunkSize = std::min(static_cast<uintmax_t>(CHUNK_SIZE), remaining);
            for (size_t i = 0; i < currentChunkSize; ++i) {
                blockBuffer[i] = static_cast<char>(imageData[imageCursor] ^ (pass * 47) ^ (ytLength % 255));
                imageCursor = (imageCursor + 1) % imageData.size();
            }
            file.write(blockBuffer.data(), currentChunkSize); bytesWritten += currentChunkSize;
        }
        file.flush(); file.close();
    }
    return true;
}

bool scrambleFileBlocks(const fs::path& filePath, unsigned int combinedSeed, const std::vector<unsigned char>& ytIcon) {
    uintmax_t fileSize = fs::file_size(filePath); if (fileSize <= CHUNK_SIZE) return true;
    std::ifstream inFile(filePath, std::ios::binary); if (!inFile.is_open()) return false;
    std::vector<std::vector<char>> allChunks; uintmax_t bytesRead = 0; size_t iconCursor = 0;
    while (bytesRead < fileSize) {
        uintmax_t remaining = fileSize - bytesRead; size_t currentChunkSize = std::min(static_cast<uintmax_t>(CHUNK_SIZE), remaining);
        std::vector<char> chunk(currentChunkSize); inFile.read(chunk.data(), currentChunkSize);
        for(size_t i = 0; i < currentChunkSize; ++i) {
            if(!ytIcon.empty()) { chunk[i] ^= ytIcon[iconCursor]; iconCursor = (iconCursor + 1) % ytIcon.size(); }
        }
        allChunks.push_back(chunk); bytesRead += currentChunkSize;
    }
    inFile.close();
    std::mt19937 g(combinedSeed); std::shuffle(allChunks.begin(), allChunks.end(), g);
    std::ofstream outFile(filePath, std::ios::binary | std::ios::trunc); if (!outFile.is_open()) return false;
    for (const auto& chunk : allChunks) { outFile.write(chunk.data(), chunk.size()); }
    outFile.flush(); outFile.close();
    return true;
}

unsigned int gatherMouseEntropy() {
    std::cout << "[*] Bitte bewege die Maus fuer 1 Sekunde wild im Kreis...\n";
    std::string entropyString = ""; auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() < 1000) {
#ifdef _WIN32
        POINT p; if (GetCursorPos(&p)) { entropyString += std::to_string(p.x) + std::to_string(p.y) + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()); }
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::string hash = get_sha256(entropyString); unsigned int seed = 0;
    for (size_t i = 0; i < 4 && i < hash.length(); ++i) { seed = (seed << 8) | static_cast<unsigned char>(hash[i]); }
    return seed;
}

void processDirectory(const fs::path& dirPath) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
        if (fs::is_regular_file(entry.path())) files.push_back(entry.path());
    }
    if (files.empty()) { std::cout << "[*] Der 'shredder'-Ordner ist leer.\n"; return; }

    unsigned int mouseSeed = gatherMouseEntropy();
    YouTubeEntropy ytData = gatherYouTubeEntropy();

    std::string superSeedStr = std::to_string(mouseSeed) + ytData.videoTitle;
    std::string superHash = get_sha256(superSeedStr);
    unsigned int finalScrambleSeed = 0;
    for (size_t i = 0; i < 4 && i < superHash.length(); ++i) { finalScrambleSeed = (finalScrambleSeed << 8) | static_cast<unsigned char>(superHash[i]); }

    std::cout << "[+] " << files.size() << " Dateien werden nuklear bereinigt.\n";

    for (const auto& filePath : files) {
        std::cout << "[*] Eliminierung: " << filePath.filename() << " ... " << std::flush;
        if (secureShred(filePath, STINKEFINGER_WEBP, 3, ytData.videoLength)) {
            try {
                auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                std::string nameSeed = filePath.filename().string() + std::to_string(now);
                std::path lockedPath = filePath.parent_path() / (get_sha256(nameSeed) + ".locked");
                fs::rename(filePath, lockedPath);

                std::cout << "[Scrambling...] " << std::flush;
                scrambleFileBlocks(lockedPath, finalScrambleSeed, ytData.iconBytes);
                fs::remove(lockedPath);
                std::cout << "[TERMINIERT]\n";
            } catch (const fs::filesystem_error& e) { std::cerr << "[Fehler: " << e.what() << "]\n"; }
        } else { std::cerr << "[Schreibfehler]\n"; }
    }

    // JETZT STARTET DIE NEUE RECHENINTENSIVE RAM-BEREINIGUNG
    nuclearRamPurge();
}

int main() {
    std::cout << "=== CypherShred Drop-Zone v8.0 (Anti-Forensic RAM Purge) ===\n";
    fs::path currentDir = fs::current_path();
    fs::path shredderDir = currentDir / "shredder";

    if (!fs::exists(shredderDir)) {
        try { fs::create_directory(shredderDir); std::cout << "[+] Ordner 'shredder' wurde angelegt.\n"; return 0; }
        catch (const fs::filesystem_error& e) { std::cerr << "[-] Fehler: " << e.what() << "\n"; return 1; }
    }

    std::cout << "[!] Bereit zum Schreddern mit Anti-RAM-Remanence Modus.\n";
    std::cout << "Operation starten? (y/N): ";
    char confirm; std::cin >> confirm;

    if (confirm == 'y' || confirm == 'Y') {
        processDirectory(shredderDir);
        std::cout << "\n[+] Operation erfolgreich abgeschlossen. System sicher.\n";
    }

    std::cout << "\nTaste druecken...";
    std::cin.get(); std::cin.get();
    return 0;
}
