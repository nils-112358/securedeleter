# Makefile für Secure Deleter
# Kompiliert auf Linux/macOS/Windows (MSVC)

# Compiler-Einstellungen
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
LDFLAGS := -lsodium

# Target
TARGET := eraser_secure
SOURCE := eraser_secure.cpp

# Detect OS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CXXFLAGS += -fPIC
endif
ifeq ($(UNAME_S),Darwin)
    # macOS
    CXXFLAGS += -fPIC
endif

# Default target
all: $(TARGET)

# Build
$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)
	@echo "[+] Build erfolgreich: ./$(TARGET)"

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: clean $(TARGET)
	@echo "[+] Debug-Build erstellt"

# Optimized build
release: CXXFLAGS = -std=c++17 -O3 -march=native -DNDEBUG
release: clean $(TARGET)
	@echo "[+] Release-Build erstellt (optimiert)"

# Test with sample file
test: $(TARGET)
	@echo "[*] Erstelle Test-Datei..."
	@echo "Test Content" > test_sample.txt
	@echo "[*] Führe eraser_secure aus..."
	@./$(TARGET) test_sample.txt
	@if [ ! -f test_sample.txt ]; then \
		echo "[+] Test erfolgreich - Datei wurde gelöscht"; \
	else \
		echo "[-] Test fehlgeschlagen"; \
	fi

# Test paranoid mode
test-paranoid: $(TARGET)
	@echo "[*] Erstelle Test-Datei für Paranoid-Modus..."
	@echo "Sensitive Data" > test_paranoid.txt
	@echo "[*] Führe mit --paranoid aus..."
	@./$(TARGET) test_paranoid.txt --paranoid

# Test directory deletion
test-dir: $(TARGET)
	@echo "[*] Erstelle Test-Verzeichnis..."
	@mkdir -p test_directory
	@echo "File 1" > test_directory/file1.txt
	@echo "File 2" > test_directory/file2.txt
	@echo "[*] Führe eraser_secure auf Verzeichnis aus..."
	@./$(TARGET) test_directory --passes 3

# Show help
help: $(TARGET)
	@./$(TARGET) --help

# Syntax check (no linking)
syntax-check:
	$(CXX) $(CXXFLAGS) -fsyntax-only $(SOURCE)
	@echo "[+] Syntax-Check erfolgreich"

# Check dependencies
check-deps:
	@echo "[*] Überprüfe libsodium..."
	@pkg-config --exists libsodium && echo "[+] libsodium gefunden" || echo "[-] libsodium nicht gefunden"
	@ldconfig -p | grep sodium && echo "[+] libsodium Bibliothek gefunden" || echo "[-] libsodium Bibliothek nicht gefunden"

# Verbose build
verbose: CXXFLAGS += -v
verbose: clean $(TARGET)

# Install (Linux/macOS)
install: $(TARGET)
	@echo "[*] Installiere zu /usr/local/bin/..."
	@sudo cp $(TARGET) /usr/local/bin/
	@echo "[+] Installation erfolgreich: /usr/local/bin/$(TARGET)"

# Uninstall
uninstall:
	@echo "[*] Deinstalliere..."
	@sudo rm -f /usr/local/bin/$(TARGET)
	@echo "[+] Deinstallation erfolgreich"

# Clean build artifacts
clean:
	@echo "[*] Räume auf..."
	@rm -f $(TARGET)
	@rm -f test_sample.txt test_paranoid.txt
	@rm -rf test_directory
	@rm -f *.o *.a
	@echo "[+] Aufräumen fertig"

# Deep clean (including tests)
distclean: clean
	@echo "[*] Vollständiger Clean..."
	@rm -f *.o *.a
	@echo "[+] Vollständiger Clean fertig"

# Show info
info:
	@echo "=== Secure Deleter Build Info ==="
	@echo "Compiler: $(CXX)"
	@echo "Flags: $(CXXFLAGS)"
	@echo "Target: $(TARGET)"
	@echo "OS: $(UNAME_S)"
	@echo ""
	@echo "Verfügbare Targets:"
	@echo "  make              - Normaler Build"
	@echo "  make debug        - Debug-Build mit Symbolen"
	@echo "  make release      - Optimierter Build"
	@echo "  make test         - Test mit Beispieldatei"
	@echo "  make test-paranoid - Test mit Paranoid-Modus"
	@echo "  make test-dir     - Test mit Verzeichnis-Löschung"
	@echo "  make install      - Installiere zu /usr/local/bin"
	@echo "  make uninstall    - Deinstalliere"
	@echo "  make clean        - Aufräumen"
	@echo "  make check-deps   - Überprüfe Abhängigkeiten"
	@echo "  make syntax-check - Nur Syntax-Check"
	@echo "  make help         - Zeige Hilfe des Programms"
	@echo "  make info         - Diese Info"

# Phony targets
.PHONY: all clean distclean install uninstall test test-paranoid test-dir help check-deps syntax-check verbose debug release info

# Default action when 'make' is called
.DEFAULT_GOAL := all
