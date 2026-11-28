# Gameboy · OSTWALD EDITION

Vier selbst programmierte Spiele für einen Quellenvortrag in Geschichte (Klasse 5).

## Starten

- **Im Browser** `index.html` öffnen und **Einschalten** wählen. Bei GitHub Pages führt der Link in der Projektbeschreibung zur selben Seite. Für eine lokale Vorführung den Ordner `vendor` neben der HTML-Datei lassen. Die Seite lädt den Emulator und die Spieldaten lokal und braucht kein WLAN.
- **Am Originalgerät** `ZEITREISE53.gb` auf eine kompatible Flashkarte kopieren. Browser und Flashkarte verwenden dieselbe ROM. Gerät, Batterien und Flashkarte vor der Vorführung zusammen ausprobieren.

**Großbild** vergrößert den Spielbildschirm in unterstützten Browsern. **Spielton** schaltet den Ton ein oder aus.

## Titel und Menü

Das Titelbild bleibt stehen, bis jemand **Start** drückt. Im Menü steht immer ein Spiel als Steckmodul auf dem Bildschirm. Links und rechts blättern, A startet.

## Die Spiele

- **Zeitreise-Quiz** stellt zehn A/B-Fragen zu Game Boy, Quellenarten, Wilhelm Ostwald und Schule, mit einer Rechenfrage und dem Kohl-Zitat vom Arbeitsblatt. Antwort A mit Taste A, Antwort B mit Taste B. Wahlweise mit 15-Sekunden-Countdown pro Frage.
- **Quellen-Snake** sammelt Münze, Brief, Kassette und Foto. Das Sammelalbum am oberen Rand zeigt jeden neuen Fund. Fünf Level von Steinzeit bis Zukunft.
- **Batterie-Regen** lässt den Game Boy Batterien fangen und Heften ausweichen. Drei Schwierigkeitsstufen.
- **Quellen-Sortierer** lässt Quellenpaare ins Regal fallen. Drei gleiche Quellenarten nebeneinander oder übereinander verschwinden. Vorschau auf das nächste Paar, drei Startgeschwindigkeiten.

## Steuerung

| Game Boy | Browser-Tastatur |
| --- | --- |
| Steuerkreuz | Pfeiltasten |
| A | X oder Leertaste |
| B | Z oder Y |
| Start | Enter |
| Select | Umschalttaste |

Auf den Bildschirmtasten lässt sich auch mit Maus oder Touch spielen. In allen vier Spielen öffnet **Start** die Pause: A spielt weiter, B führt zum Menü. Auf der Tastatur übernimmt **Enter** diese Funktion.

## ROM und Browserfassung neu bauen

Mit [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020) (gebaut mit 4.6.0) und Python 3 im Projektordner ausführen.

```sh
lcc -Wm-ynGAMEBOY -o ZEITREISE53.gb src/main.c
python3 tools/embed_rom.py
```

Spielregeln, Quiz und Sound liegen in `src/main.c` und `src/sorter.h`. Pixelgrafiken, Schrift, Logo und Menügrafik liegen in `src/tiles.h`, `src/font5x7.h`, `src/logo.h` und `src/menuart.h`.

Der unveränderte Emulator WasmBoy 0.7.1 von Aaron Turner liegt in `vendor`. Dort liegt auch seine GPL-Lizenz. [Quellcode für diese Version](https://github.com/torch2424/wasmBoy/tree/8e96bcb70969d943b1ffc4028b169c835098ce04) · [Veröffentlichtes Paket](https://www.npmjs.com/package/wasmboy/v/0.7.1)
