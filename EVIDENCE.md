# Prüfprotokoll

Was für diese öffentliche Fassung am 27.10.2026 tatsächlich ausgeführt wurde. Umgebung: Linux-Container, g++ 13.3, Python 3.13, Node 22. Kein ESP32, keine LEDs.

| Prüfung | Ergebnis |
|---|---|
| `bash tests/run_tests.sh`: C++-Host-Tests (`-Wall -Wextra -Werror`) | **44.552 Prüfungen**, 0 Fehler |
| dasselbe: JavaScript-Kern der Oberfläche, Start der Oberfläche V5 | 38 und 41 Prüfungen, 0 Fehler |
| dasselbe: Python (Home-Assistant-Adapter 107, HUGO-Adapter 158, NVS-Werkzeug 4) | **269 Tests**, 0 Fehler |
| `tests/capture_v41_frames.sh`: Frames aus den echten Renderern | Pong, Tail-Tennis, Feuerwerk, Pixel erzeugt; Bilder in `docs/assets/frames` |
| Secret-Scan, gitleaks 8.28 über alle Dateien | keine Funde |
| Suche nach privaten Daten (Netzadressen, MAC, Pfade, E-Mail) | nur noch Platzhalter |
| Firmware-Build mit PlatformIO (GitHub Actions, `espressif32@6.10.0`) | beide Varianten bauen: `esp32_eq` und `esp32_eq_ds4` (DS4: 1.303.577 B Flash, 99,5 % der App-Partition; RAM 35,5 %) |

## Aus den privaten Entwicklungsunterlagen (hier nicht nachgeprüft)

- L001 bis L007 wurden nach unabhängiger Prüfung vom Besitzer abgenommen (September 2026).
- Am echten Gerät: Firmware mehrfach geschrieben und per Prüfsumme bestätigt, Oberfläche V5 vom Gerät ausgeliefert und am Handy fotografiert (Bild in `docs/assets`), Tail-Länge 150 und Grundbild von Matrix und Tail vom Besitzer bestätigt, DualShock 4 als Spieler 1 mit Snake und Pong.
- Letzter Firmware-Build L015 (DualShock-Variante): 1.310.608 Bytes, passt in die App-Partition.

## Was nicht geprüft ist

- **Echte Hardware in dieser Fassung.** Hier lief nichts gegen einen ESP32 oder LEDs.
- **Elektrik:** Strom, Spannung, Temperatur, Dauerlauf.
- **WLAN-Stabilität:** laut Besitzer (27.10.2026) im Alltag ohne Probleme; hier nicht gemessen.
- **Live-Betrieb von Home Assistant und HUGO.**
