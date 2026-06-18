# Kompilierung & Test

## Quick Start (Linux/macOS)

```bash
# 1. Dependencies installieren
sudo apt-get install libsodium-dev  # Ubuntu/Debian
# oder
brew install libsodium              # macOS

# 2. Kompilieren
g++ -std=c++17 -O2 eraser_secure.cpp -o eraser_secure -lsodium

# 3. Testen mit Testdatei
echo "Testinhalt" > test_file.txt
./eraser_secure test_file.txt

# 4. Paranoid-Modus testen
echo "Vertraulich" > secret.txt
./eraser_secure secret.txt --paranoid

# 5. Verzeichnis testen
mkdir test_dir
echo "file1" > test_dir/file1.txt
echo "file2" > test_dir/file2.txt
./eraser_secure test_dir --passes 5
```

## Windows (MSVC)

```bash
# Dependencies mit vcpkg
vcpkg install libsodium

# Kompilieren
cl /std:c++17 /O2 eraser_secure.cpp /link sodium.lib
```

## Probleme & Lösungen

### "fatal error: sodium.h: No such file or directory"
```bash
# libsodium nicht gefunden
sudo apt-get install libsodium-dev

# Oder manueller Pfad:
g++ -I/usr/include -L/usr/lib -std=c++17 -O2 eraser_secure.cpp -o eraser_secure -lsodium
```

### "undefined reference to `randombytes_buf'"
```bash
# -lsodium fehlt am Ende!
g++ -std=c++17 -O2 eraser_secure.cpp -o eraser_secure -lsodium
```

### "Permission denied" beim Löschen
```bash
chmod u+w test_file.txt
./eraser_secure test_file.txt
```

### Zu langsam?
```bash
# Mit Optimierungen
g++ -std=c++17 -O3 -march=native eraser_secure.cpp -o eraser_secure -lsodium
```

## Verifizierung

```bash
# 1. Binär existiert?
ls -lh eraser_secure

# 2. Abhängigkeiten korrekt?
ldd ./eraser_secure
# sollte libsodium.so zeigen

# 3. Hilfe funktioniert?
./eraser_secure --help

# 4. Mit Testdatei testen (NICHT deine echten Daten!)
dd if=/dev/zero of=test_1gb.bin bs=1M count=1024
time ./eraser_secure test_1gb.bin --passes 3
```

## Performance-Erwartungen

| Größe | Passes | Zeit (SSD) | Zeit (HDD) |
|-------|--------|-----------|-----------|
| 100 MB | 3 | ~2 sec | ~10 sec |
| 1 GB | 3 | ~20 sec | ~100 sec |
| 10 GB | 7 | ~3-4 min | ~15-20 min |

**Wenn es länger dauert = normal, HDD ist einfach langsam!**

---

**Status**: Code sollte compilieren. Falls nicht → Fehler hier posten!
