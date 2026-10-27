# Projektstand LED Wall

Stand: 27.10.2026. Dieses öffentliche Repository ist eine Momentaufnahme des privaten Entwicklungs-Repositorys mit dem neuesten Quellstand (L015/L016, Oktober 2026).

Die Arbeit lief in nummerierten Aufgaben mit Übergabe, Nachweisen, unabhängiger Prüfung und Abnahme durch den Besitzer. Die Tabelle fasst die privaten Unterlagen zusammen.

## Ausbaustufen

| Aufgabe | Inhalt | Stand |
|---|---|---|
| L001 | Repository, Regeln, eingefrorene V1-Spezifikation | abgenommen (11.09.2026) |
| L002 | Kern-Laufzeit: Modusmaschine, Einblendungen, Bildrate, Helligkeit mit Obergrenze | abgenommen (13.09.2026) |
| L003 | Semantische lokale API und Gerätezustand | abgenommen (13.09.2026) |
| L004 | Web-Oberfläche, vom Gerät ausgeliefert | abgenommen (13.09.2026) |
| L005 | Home-Assistant-Adapter (MQTT ↔ HTTP) | abgenommen (13.09.2026), nicht live betrieben |
| L006 | HUGO-Adapter (JSON-RPC über stdio) | abgenommen (13.09.2026), nicht live betrieben |
| L007 | Renderer und Text-Pipeline | abgenommen (15.09.2026) |
| L008 | Inbetriebnahme am echten Gerät, fünf Firmware-Korrekturen aus der Hardware | Gerät lief; Dauerlauf und elektrische Prüfung offen, nie abgenommen |
| L009 | Neue Oberfläche, Text-Studio, Timer, Menü, 5 Spiele, Benachrichtigungen, Standby | umgesetzt und lokal geprüft, nicht abgenommen |
| L010 | Zweite Fläche (Tail), DualShock 4, 1 und 2 Spieler | umgesetzt; Tail-Länge 150 vom Besitzer am Gerät bestätigt; nicht abgenommen |
| L011 | 10 Effekte über beide Flächen, weitere Spiele, Pixel-Studio, Strom-Schätzung | auf dem Gerät installiert, Grundbild vom Besitzer bestätigt; nicht abgenommen |
| L013 | Oberfläche V5 (sechs Bereiche, Handy) | auf dem Gerät installiert und ausgeliefert; Abnahme am Handy offen |
| L014 | DualShock 4 am Gerät | Controller gekoppelt, Snake und Pong am Gerät gespielt |
| L015 | Spieltempo, Duelle bis 5 Punkte, Feuerwerk ohne Rücklauf | Quellstand dieses Repositorys; per Prüfsumme bestätigt auf das Gerät geschrieben |
| L016 | Erneute Prüfung am Gerät | Firmware L015 geschrieben und per Prüfsumme bestätigt; ein damaliger WLAN-Aussetzer ist laut Besitzer inzwischen kein Thema mehr |

## Offen

**Besitzer-Rückmeldung 27.10.2026:** Die Wand läuft im Alltag sehr gut, ohne Probleme. Die WLAN-Aussetzer unter Spiellast, die in den Unterlagen vom 17./18.10. stehen, sind ein altes Problem und treten nicht mehr auf.

- **Elektrische Messung:** Strom, Spannung und Temperatur unter Last sind nicht formal gemessen. Die Helligkeit ist auf 40 von 100 begrenzt.
- **Formale Abnahmen:** Ab L008 sind die Aufgaben in den Unterlagen nicht formal abgenommen, obwohl das Gerät im Alltag läuft.
- **Speicher:** Die SPIFFS-Partition wird bewusst nicht formatiert; Projekte werden daher im Browser gespeichert, nicht auf dem Gerät.
- **HUGO:** Adapter getestet, aber nie gegen ein laufendes HUGO Core oder die echte Wand.

## Was gegenüber dem privaten Repository entfernt oder geändert wurde

- Kompletter Git-Verlauf; dieses Repository beginnt mit einem einzigen Commit.
- `docs/work/` (Status, Aufgabenliste, Übergaben, Nachweise mit Gerätedaten wie MAC-Adresse und Prüfsummen von Flash-Sicherungen), Agenten-Anleitung `AGENTS.md`, der interne Arbeitsstandard, eine Prozess-ADR, eine leere Release-Vorlage, die Veröffentlichungs-Protokolldatei.
- Private Netzadressen durch Platzhalter (`192.168.1.x`) ersetzt, lokale Pfade durch `~/`.
- `tests/test_render_animations.cpp`: `std::sinf` durch `std::sin` ersetzt (g++ 13 kennt `std::sinf` nicht; das Ergebnis ist gleich).
- `tests/capture_v41_frames.sh`: fehlende Quelldatei `body_topology.cpp` ergänzt, Ziel nach `docs/assets/frames`.
- Neu: `tools/render_frames.py`, Bilder in `docs/assets`, diese Dokumente, LICENSE, SECURITY.md, CI mit gitleaks.
