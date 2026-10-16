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
- Card copy/cut/paste, alignment and distribution, and arrow-key nudging.
- Scroll to zoom around the pointer; middle-drag or Space-drag to pan.
- Card library, scrollable board list, properties inspector, overview minimap,
  fit-to-board controls, and light/dark themes.
- New/Open/Save/Save as, board renaming, SVG export, and an in-app shortcut guide.
- Open Recent (10 files), remembered theme/grid/snapping and last board viewport.
- Separate recovery snapshots every 30 seconds, including untitled boards and
  uncommitted property text. Startup offers Restore, Discard or Later.
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
| Ctrl/Cmd+C / X / V | Copy / cut / paste cards (or text in a focused field) |
| Arrow keys | Move selected cards by 1 world unit |
| Shift+Arrow keys | Move selected cards by 10 world units |
| Ctrl/Cmd+A | Select all (or select text in a field) |
| Delete / Backspace | Delete selected cards or connection |
| F | Fit board |
| Escape | Deselect / cancel field or dialog |

Windows uses native file dialogs; Linux/macOS use a file-path dialog in the app.

## Arrange and reuse cards

Use the contextual Copy/Cut/Paste toolbar above the canvas, or keyboard shortcuts.
Copy includes selected cards and connections whose endpoints are both selected.
Paste gives every card/connection a fresh ID, preserves text/style and relative
positions, and places the group near the viewport center. Repeated pastes offset
the group by 24 world units. Cut removes incident connections; undo restores them.
Selections can be pasted between boards or running app instances. Clipboard data
is validated before insertion; invalid text or document-limit failures leave the
board unchanged. Ordinary text paste still works in focused property fields.

For multiple selected cards, the inspector offers left/right/top/bottom and
horizontal/vertical center alignment. **Space across/down** requires three cards
and keeps the outermost cards fixed while spacing the rest evenly. Alignment,
distribution, paste and cut each create one undo action. Holding an arrow key
groups its repeats into one undo action, completed on release, focus loss or
another operation. Nudging ignores grid snapping for precise positioning. Group
movement/paste at coordinate limits preserves spacing rather than clamping each
card separately.

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
stack. Saved-state tracking also works across undo/redo. Recovery is a local
backup, not a replacement for explicitly saving documents. Changes since the
last successful snapshot can be lost after an abrupt shutdown.

## Workspace persistence and recovery

Flow Studio reopens the last available saved board and restores its viewport,
theme, grid and snapping settings. Choose **Open recent** in the sidebar to
reopen one of the last 10 saved/opened boards or clear the history. Missing files
remain listed as unavailable; a failed open preserves the current board.

Every 30 seconds the app writes changed work to `recovery.json` in its workspace
data folder, independently of the original board. It also checkpoints before a
close confirmation. Snapshots include the last saved baseline and viewport so a
restart can distinguish unsaved edits from externally changed files. Saving,
undoing back to the saved state, or explicitly discarding current work clears an
owned snapshot. A successful restore remains recoverable until saved/discarded.

On startup, **Restore** opens the snapshot as unsaved work; **Discard** removes
it; **Later** retains it for review through Open Recent. Unresolved snapshots,
including corrupt ones, are never overwritten automatically. While a previous
snapshot remains pending, current edits require explicit saves. Restore also
honors the current board's unsaved-change prompt.

If the original saved file is missing or differs from the snapshot's baseline,
restored work becomes an untitled board and requires Save As. Recovery never
automatically writes to the source board. Corrupt settings fall back to defaults
with a status message; corrupt recovery stays on disk until explicitly discarded.

Default data locations:

- Windows: `%LOCALAPPDATA%/FlowStudio`
- Linux: `$XDG_STATE_HOME/flow-studio`, or `~/.local/state/flow-studio`
- macOS: `~/Library/Application Support/FlowStudio`

Use `--state-dir path/to/folder` for a separate or portable workspace. One app
instance may use each folder at a time; the OS releases its lock on exit/crash.
`settings.json`, `recovery.json` and `workspace.lock` are reserved paths, not
document/export destinations. Workspace files retain local board content and
file paths; there is no cloud synchronization. Recovery files are capped at
12 MiB and settings at 128 KiB, and their contents are validated before use.

Smoke tests always use isolated temporary workspaces and ignore `--state-dir`;
they do not read/write the normal user profile.

## Verification

```sh
ctest --test-dir build/windows-debug -C Debug -R 'flow_' --output-on-failure
```

`flow_document_tests` exercises JSON roundtrips/validation, Unicode escapes,
history, clipboard validation/remapping, arrangement and movement bounds,
connection deletion/duplication, rollback and file replacement.
`flow_workspace_tests` covers interrupted-session recovery, preferences, recent
files, locking, corrupt-data preservation and checkpoint failure. Both model
tests also build with apps/examples/GPU tests disabled, without GLFW.
With `VECTORGL_BUILD_GPU_TESTS=ON`, `flow_studio_smoke` also exercises rendering,
property editing, save/reopen, SVG export, recovery prompts, external-file
conflicts, deferred snapshots and toolbar input in a hidden window.
Clipboard GUI checks inject text instead of changing the user's system clipboard.

To capture a preview with a real font:

```powershell
.\build\windows-debug\apps\diagram_editor\Debug\flow_studio.exe --smoke-test --screenshot build/flow-studio.bmp
```

`document.*` contains the GL-independent document model, persistence and history;
`workspace.*` manages persistent settings, locks and recovery; `app.*` contains
drawing/input; `main.cpp` owns the window/context lifecycle.
