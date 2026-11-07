# VectorGL technical guides

These Markdown guides document the library alongside its source code. The public
website is maintained and deployed independently in
[vectorgl-website](https://github.com/mahtab04/vectorgl-website), and is available at
<https://mahtab04.github.io/vectorgl-website/>.

- [Colors](colors.md)
- [UI context and event routing](ui-context.md)
- [GLFW host integration](ui-glfw.md)
- [Layout](ui-layout.md)
- [Scrolling and split panels](ui-containers.md)
- [Controls and editors](ui-controls.md)
- [Overlays and menus](ui-overlays.md)
- [Workspace controls](ui-workspace-controls.md)
- [Lists](ui-lists.md), [trees](ui-trees.md), and [tables](ui-tables.md)
- [Drag and drop](ui-drag-drop.md)
- [Native file dialogs](ui-file-dialogs.md)
- [Editor workspace](ui-editor-workspace.md)
- [Migration and capability boundaries](ui-migration.md)

Update these guides with changes to the corresponding library APIs. Update the
separate website repository when its examples or API summaries also need changes.

The legacy `https://mahtab04.github.io/vectorgl/` URLs are maintained by the tiny
pages in `.github/pages-redirects/`. The existing Pages workflow deploys only that
directory; technical guide edits do not trigger website deployments. Redirects
preserve query strings and fragment identifiers when JavaScript is available,
with a direct link and refresh fallback otherwise.
