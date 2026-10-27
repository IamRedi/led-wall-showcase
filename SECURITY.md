# Sicherheit / Security

## Einsatzmodell

LED Wall ist ein Gerät für das eigene WLAN. Kein Konto, keine Cloud, keine Telemetrie.

| Schnittstelle | Zweck |
|---|---|
| HTTP (Port 80) | Web-Oberfläche und semantische API ([docs/api/CONTRACT.md](docs/api/CONTRACT.md)) |
| WebSocket (Port 81) | Zustands-Ereignisse für Oberfläche und Adapter |
| Bluetooth Classic | nur in der DualShock-Variante (`esp32_eq_ds4`) |

## Bekannte Grenzen

- **Keine Anmeldung (Vertrauen ins Heimnetz).** Wer im selben WLAN ist, kann die Wand steuern. Die API darf nie ins Internet weitergeleitet werden. Das ist im API-Vertrag als bewusste Grenze von Version 1 festgehalten.
- **Elektrik nicht abgenommen.** Die Firmware begrenzt die Helligkeit und schätzt den Strom, eine Messung unter Last fehlt. Die LEDs brauchen ein eigenes, passend dimensioniertes Netzteil mit gemeinsamer Masse; nicht über den USB-Anschluss des ESP32 versorgen. Der Brownout-Detektor ist im Build ausgeschaltet (`CONFIG_ESP32_BROWNOUT_DET=0`), wie in der Vorgänger-Firmware.

## Was im Code abgesichert ist

- **Eine Hoheit:** Nur die Firmware rendert. Clients schicken Absichten (Text, Szene, Ausdruck …), nie LED-Indizes oder Pins.
- **Helligkeits-Obergrenze** lokal erzwungen, unabhängig von dem, was ein Client verlangt.
- **Eingaben begrenzt:** Textlänge, Wertebereiche, unbekannte Felder werden abgelehnt (Firmware und Adapter, mit Tests).
- **Zugangsdaten:** WLAN-Passwort nur im NVS-Speicher, nie im Quellcode und nie in API-Antworten. Die Konfiguration der Adapter (`config.json`) ist nicht im Repository; der Home-Assistant-Adapter zeigt das MQTT-Passwort in Logs nur maskiert.
- **Zwei-Spieler-Sitzungen** mit Sitzungsnummer, Folgenummer und Heartbeat; verlorene Sitzungen pausieren das Spiel.
- Secret-Scan (gitleaks) in GitHub Actions bei jedem Push.

## Schwachstelle melden

Bitte nicht als öffentliches Issue. Kontakt über das GitHub-Profil [@IamRedi](https://github.com/IamRedi) oder über „Report a vulnerability“ im Security-Tab dieses Repositorys, falls aktiviert.
