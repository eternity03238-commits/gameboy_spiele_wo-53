# Ostwald-Boy – Zeitreise 5/3

Ein selbst programmiertes Gameboy-Spiel für einen Quellenvortrag im Fach Geschichte (Klasse 5).

**Im Browser spielen:** über GitHub Pages (Link in der Projektbeschreibung).

## Inhalt

| Datei | Wozu |
| --- | --- |
| `index.html` | Gameboy im Browser (Emulator WasmBoy) mit eingebetteter ROM |
| `ZEITREISE53.gb` | Die Spieldatei für einen echten Gameboy (z. B. über eine Flashkarte) |
| `src/main.c`, `src/tiles.h` | Quellcode in C |

## Die Spiele

- **Quellen-Snake** – frisst Münze, Brief, Kassette und Foto und zeigt die Quellenart.
- **Batterie-Regen** – Batterien fangen, Hausaufgaben ausweichen.
- **Zeitreise-Quiz** – 10 Fragen zu Gameboy, Quellen und Schule.

## Steuerung im Browser

Pfeiltasten = Steuerkreuz, X/Leertaste = A, Z/Y = B, Enter = Start, Shift = Select. Auf dem Handy die Tasten antippen.

## Selbst bauen

Mit [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020):

```sh
lcc -Wm-yn"ZEITREISE53" -o ZEITREISE53.gb src/main.c
```
