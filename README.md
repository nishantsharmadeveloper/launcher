# Launcher

A lightweight, native Windows application launcher in the style of macOS
Spotlight, Raycast, and PowerToys Run — built with **C++17** and the
**Win32 API only** (no Qt/Electron/.NET/MFC/wxWidgets).

Press **Tab + P** anywhere in Windows to summon it. Start typing, arrow
through matches, hit Enter to launch.

## Features

- Global `Tab + P` hotkey via a `WH_KEYBOARD_LL` low-level keyboard hook
- Borderless, rounded, dark-themed, always-on-top popup window with a
  native fade-in/out animation
- Background indexing of Start Menu, Desktop, Program Files, and
  Program Files (x86) — indexing never blocks startup or the UI
- Real-time fuzzy/ranked search (exact → starts-with → contains → fuzzy,
  with a frequency-of-use tiebreaker)
- Real Windows shell icons per result, cached for performance
- Full keyboard navigation (Up/Down/Tab to move, Enter to launch, Esc to
  dismiss), plus mouse click/double-click
- System tray icon with Show / Refresh Index / Launch on Startup / About
  / Exit
- "Launch on Windows startup" toggle, persisted to the registry
- Single-instance guard (a second launch just exits)

## Requirements

- **Visual Studio 2022** with the "Desktop development with C++" workload
- Windows 10 or later (Windows 11 gets native rounded corners via DWM;
  Windows 10 falls back to a region-based rounded window)

## Building

1. Open `Launcher.sln` in Visual Studio 2022.
2. Select the **Release** configuration and **x64** platform.
3. Build → Build Solution (Ctrl+Shift+B).
4. The executable is written to `bin\Release\x64\Launcher.exe`.

You can also build from the command line with:

```
msbuild Launcher.sln /p:Configuration=Release /p:Platform=x64
```

## Running

Run `Launcher.exe`. It has no visible window on launch — look for its
icon in the system tray. Press **Tab + P** to open it.

To have it start automatically with Windows, right-click the tray icon
and select **Launch on Startup**.

## Project Structure

```
Launcher/
├── Launcher.sln
└── Launcher/
    ├── Launcher.vcxproj
    ├── Launcher.vcxproj.filters
    ├── src/
    │   ├── main.cpp            Entry point (wWinMain), single-instance guard
    │   ├── App.h / App.cpp     Composition root: wires all subsystems together
    │   ├── Window.h / .cpp     The launcher popup window (search box, results list, animation)
    │   ├── KeyboardHook.h/.cpp WH_KEYBOARD_LL hook for the Tab+P chord
    │   ├── ProgramIndexer.*    Filesystem/shell scanning + IconCache
    │   ├── SearchEngine.*      Ranking logic (exact/starts-with/contains/fuzzy)
    │   ├── TrayIcon.h / .cpp   Shell_NotifyIcon wrapper + context menu
    │   ├── Settings.h / .cpp   Registry-backed preferences (startup toggle)
    │   └── Utils.h / .cpp      String conversion, fuzzy match, DPI/monitor helpers
    └── resources/
        ├── app.ico
        ├── resource.h
        └── resource.rc
```

## Architecture Notes

- **Application** (`App.h`) is the only class that knows about every
  other subsystem. `Window`, `KeyboardHook`, `ProgramIndexer`,
  `SearchEngine`, and `TrayIcon` are all independent and communicate
  only through `std::function` callbacks supplied at construction —
  there are no global variables anywhere in the project.
- **Threading**: `ProgramIndexer::BeginIndexAsync` runs on a background
  `std::thread`. The low-level keyboard hook callback and the indexer's
  completion callback both `PostMessageW` back to a hidden owner window
  rather than touching UI state directly, keeping all UI work on the
  main thread.
- **Rendering**: the results list is owner-drawn directly onto the
  window (double-buffered GDI), rather than a native ListView, so icon +
  name + path layout matches the Spotlight-style mock exactly.
- **Icons**: `IconCache` extracts icons via `SHGetFileInfoW` once per
  unique executable path and caches the `HICON`, since shell icon
  extraction is comparatively expensive.

## Extending

The codebase is intentionally modular so the following (listed in the
original spec as future-ready features) can be added without
restructuring existing classes:

- File/folder search — add a new source in `ProgramIndexer` or a
  sibling indexer, and a new `SearchResult` kind
- Calculator / unit converter / currency converter — intercept the
  query in `Application::OnQueryChanged` before falling through to
  `SearchEngine::Search`
- Plugin system — formalize the `onQueryChanged` → results contract in
  `Window::Callbacks` into a proper plugin interface
- Clipboard history, recent files, Everything integration, etc. — each
  as an additional result source merged in `Application::OnQueryChanged`

## Known Limitations

- WindowsApps (UWP/Store) packages are best-effort: most packaged apps
  are already picked up via their generated Start Menu shortcut, but
  raw `WindowsApps` folder enumeration is intentionally not deep-scanned
  (see the comment in `ProgramIndexer::ScanWindowsApps`) since resolving
  friendly names from package folders requires the Package Manager APIs.
- Fuzzy matching is a simple ordered-subsequence matcher, not a full
  Levenshtein/Smith-Waterman implementation — it's fast and good enough
  for app-name search, but won't correct transposed letters.
