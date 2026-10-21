# OpenScore

An open-source sheet music reader for Linux, inspired by forScore. Read PDF scores, turn pages with a pedal, mark them up with a pen, and keep a library that puts what you played last at the top.

**Status: early scaffold (v0.1).** The code has not been compiled yet. Expect a first round of build fixes.

## Features (MVP)

- **Fast page turns.** MuPDF renders on a dedicated thread; the pages around the current one are pre-rendered into an in-memory cache, so a turn is a texture swap. Single-page and two-page views.
- **Library that mirrors your folders.** Your scores stay where you keep them. OpenScore adds metadata (title, composer, page count, cover thumbnails) and history on top. Recently opened scores appear in a strip at the top, and in every folder view entries are sorted by recency, with a folder counting as recent when anything inside it was. Also: breadcrumbs, search, sort by name or date added, new folder, and moved files keep their history (matched by content hash).
- **Annotations.** Pen (pressure-sensitive), highlighter, eraser (including the pen's eraser end), undo/redo. Ink is stored outside the PDF, in `~/.local/share/openscore/annotations/<sha1>.json`, keyed by the file's content hash. Touch never draws, so palms and fingers turn pages instead.
- **Pedals and MIDI.** Bluetooth page turners that act as keyboards work out of the box (arrows, PageUp/PageDown, Space). Any key or MIDI message (CC, program change, note) can be re-bound with "Learn". MIDI needs RtMidi at build time.
- Resumes at the last page you were on.

## Build

Dependencies: Qt 6.5+ (Quick, QuickControls2, Sql, Concurrent), MuPDF, CMake 3.21+, optionally RtMidi.

```sh
# Debian / Ubuntu
sudo apt install build-essential cmake pkg-config qt6-base-dev qt6-declarative-dev \
                 qml6-module-qtquick-controls qml6-module-qtquick-dialogs \
                 libqt6sql6-sqlite libmupdf-dev librtmidi-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/openscore            # or: ./build/openscore some-score.pdf
```

The default library folder is `~/Documents/Scores`; change it with "Library folder…".

## Layout

| Path | What |
| --- | --- |
| `src/PdfDocument.*` | MuPDF wrapper |
| `src/PageRenderService.*` | Render thread, urgent/prefetch queues |
| `src/ScoreView.*` | Scene-graph page view and page cache |
| `src/InkLayer.*`, `src/AnnotationStore.*` | Drawing, erasing, undo, JSON persistence |
| `src/Library.*`, `src/EntryModel.*` | Folder scan, SQLite metadata, recency-first browsing |
| `src/ActionMap.*`, `src/MidiInput.*` | Key and MIDI bindings |
| `qml/` | Library, reader, settings UI |

## Roadmap

Setlists and tags, bookmarks and a page index, half-page turns, pinch-zoom and crop margins, annotation layers, drag-and-drop into folders, import from forScore backups, a custom scene-graph ink renderer for even lower pen latency, Flatpak packaging.

## License

AGPL-3.0-or-later (required because MuPDF is AGPL). Add the full license text as `LICENSE` (`curl -o LICENSE https://www.gnu.org/licenses/agpl-3.0.txt`).
