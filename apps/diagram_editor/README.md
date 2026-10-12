# Flow Studio

A local desktop diagram editor built entirely with VectorGL and GLFW. No browser,
web UI toolkit or external service is required.

## Build and run

From the repository root:

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_APPS=ON
cmake --build build/windows-debug --config Debug --target flow_studio
.\build\windows-debug\apps\diagram_editor\Debug\flow_studio.exe
```

On Linux/macOS:

```sh
cmake -S . -B build -DVECTORGL_BUILD_APPS=ON
cmake --build build --target flow_studio
./build/apps/diagram_editor/flow_studio
```

Pass a board path to reopen it, or `--font path/to/font.ttf` to choose a font.
The app searches common Segoe UI, Arial, DejaVu Sans and Liberation Sans paths.
It requires an OpenGL 3.3 context. On a fresh configure, apps follow
`VECTORGL_BUILD_EXAMPLES` by default. Set `-DVECTORGL_BUILD_APPS=ON` to build the
app independently, or `OFF` for a library-only build. Existing CMake caches keep
their previous setting until explicitly changed.

## Workspace

- Task, decision and note cards with editable UTF-8 titles and descriptions.
- Connections: drag a card's right port onto another card's left port.
- Single/multiple selection, box selection, group dragging, grid snapping,
  duplication (including connections within the selection), and deletion.
- Scroll to zoom around the pointer; middle-drag or Space-drag to pan.
- Card library, scrollable board list, properties inspector, overview minimap,
  fit-to-board controls, and light/dark themes.
- New/Open/Save/Save as, board renaming, SVG export, and an in-app shortcut guide.
- Unsaved-change prompts for closing, creating or opening boards.
- Undo/redo transactions; an entire drag or completed property edit is one action.

Click a card to edit it in the inspector. Enter or click outside a property field
to apply it; Escape cancels an uncommitted field edit. Double-click a card to focus
its title. Tab switches between title and description fields. Text fields support
selection and system clipboard copy/cut/paste. Text follows VectorGL's existing
font coverage and UTF-8 codepoint editing; complex-script shaping and IME are not
provided by this app.

| Shortcut | Action |
|---|---|
| Ctrl/Cmd+N | New board |
| Ctrl/Cmd+O | Open |
| Ctrl/Cmd+S | Save |
| Ctrl/Cmd+Shift+S | Save as |
| Ctrl/Cmd+Z | Undo |
| Ctrl/Cmd+Shift+Z or Ctrl/Cmd+Y | Redo |
| Ctrl/Cmd+D | Duplicate selected cards |
| Ctrl/Cmd+A | Select all (or select text in a field) |
| Delete / Backspace | Delete selected cards or connection |
| F | Fit board |
| Escape | Deselect / cancel field or dialog |

Windows uses native file dialogs; Linux/macOS use a file-path dialog in the app.
SVG exports contain editable vector cards, connections and text; text wrapping
and font appearance may differ from the app. Exports include the entire board,
independent of the current zoom/pan and selection.

## Documents and safety

Boards use versioned JSON. Opening validates UTF-8, unique IDs, finite bounded
coordinates, valid connection endpoints, and document limits before replacing
the workspace. Limits: 2 MiB files, 2,000 cards, 4,000 connections, 1,024 title
bytes, 4,096 description bytes and 256 board-name bytes. Self/duplicate
connections are rejected. Deleting a card removes incident connections.

Saves write a temporary file in the destination directory and rename/replace it
only after a successful write. A failed save leaves the previous board intact.
Undo/redo retains up to 100 snapshots with a 16 MiB accounted-storage limit per
stack. Saved-state tracking also works across undo/redo. There is no automatic
cloud synchronization or crash recovery; save documents explicitly.

## Verification

```sh
ctest --test-dir build/windows-debug -C Debug -R 'flow_' --output-on-failure
```

`flow_document_tests` exercises JSON roundtrips/validation, Unicode escapes,
history, connection deletion/duplication, rollback and file replacement.
With `VECTORGL_BUILD_GPU_TESTS=ON`, `flow_studio_smoke` also exercises rendering,
property editing, save/reopen, SVG export and toolbar input in a hidden window.

To capture a preview with a real font:

```powershell
.\build\windows-debug\apps\diagram_editor\Debug\flow_studio.exe --smoke-test --screenshot build/flow-studio.bmp
```

`document.*` contains the GL-independent document model, persistence and history;
`app.*` contains drawing/input; `main.cpp` owns the window/context lifecycle.
