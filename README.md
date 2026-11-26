# Gameboy · OSTWALD EDITION

Vier selbst programmierte Spiele für einen Quellenvortrag in Geschichte (Klasse 5).

## Starten

- **Im Browser** `index.html` öffnen und **Einschalten** wählen. Bei GitHub Pages führt der Link in der Projektbeschreibung zur selben Seite. Für eine lokale Vorführung den Ordner `vendor` neben der HTML-Datei lassen. Die Seite lädt den Emulator und die Spieldaten lokal und braucht kein WLAN.
- **Am Originalgerät** `ZEITREISE53.gb` auf eine kompatible Flashkarte kopieren. Browser und Flashkarte verwenden dieselbe ROM. Gerät, Batterien und Flashkarte vor der Vorführung zusammen ausprobieren.

**Großbild** vergrößert den Spielbildschirm in unterstützten Browsern. **Spielton** schaltet den Ton ein oder aus.

## Die Spiele

- **Ostwald-Archiv** führt durch fünf Regale. Quellenpaare füllen die Lücken. Volle Reihen verschwinden. Mit jedem Level steigt das Tempo. Links/rechts verschiebt, A dreht, unten beschleunigt und B lässt das Paar sofort fallen.
- **Quellen-Snake** sammelt Münze, Brief, Kassette und Foto. Die ersten vier Funde sind verschieden. Alle fünf Funde steigt die Geschwindigkeit bis zur höchsten Stufe.
- **Batterie-Regen** lässt den Game Boy Batterien fangen und Heften ausweichen. Die freie Runde hat drei Schwierigkeitsstufen.
- **Zeitreise-Quiz** umfasst zehn A/B-Fragen zu Game Boy, historischen Quellen und Schule.

## Steuerung

| Game Boy | Browser-Tastatur |
| --- | --- |
| Steuerkreuz | Pfeiltasten |
| A | X oder Leertaste |
| B | Z oder Y |
| Start | Enter |
| Select | Umschalttaste |

Auf den Bildschirmtasten lässt sich auch mit Maus oder Touch spielen. Bei Ostwald-Archiv, Snake und Batterie-Regen öffnet **Start** die Pause. **A** setzt fort, **B** führt zurück ins Menü. Im Quiz beantwortet **A/B** die Frage, **Start** führt ins Menü zurück.

## ROM und Browserfassung neu bauen

Mit [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020) (gebaut mit 4.6.0) und Python 3 im Projektordner ausführen.

```sh
lcc -Wm-ynGAMEBOY -o ZEITREISE53.gb src/main.c
python3 tools/embed_rom.py
```

Spielregeln, Quiz und Sound liegen in `src/main.c` und `src/archive.h`. Pixelgrafiken, Schrift und Logo liegen in `src/tiles.h`, `src/font5x7.h` und `src/logo.h`.

Der unveränderte Emulator WasmBoy 0.7.1 von Aaron Turner liegt in `vendor`. Dort liegt auch seine GPL-Lizenz. [Quellcode für diese Version](https://github.com/torch2424/wasmBoy/tree/8e96bcb70969d943b1ffc4028b169c835098ce04) · [Veröffentlichtes Paket](https://www.npmjs.com/package/wasmboy/v/0.7.1)
