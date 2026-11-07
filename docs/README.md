# VectorGL documentation website

This is a static website. HTML, CSS and browser JavaScript are served directly;
there is no frontend build step, framework, external font or CDN dependency.

## Preview locally

From the repository root:

```sh
python -m http.server 8765 --directory docs
```

Open <http://localhost:8765/>. The existing GitHub Pages workflow publishes this
directory when changes to it land on `main`.

## Editing the site

- `index.html`: introduction, Canvas/Scene/UI examples and CMake integration.
- `overview.html`: architecture, build profiles and installation.
- `guides.html`: lifecycle examples and links to detailed guides.
- `examples.html`: screenshot gallery and runnable repository examples.
- `api.html`: searchable reference, populated from `app.js`.
- `styles.css`: shared responsive layout, light/dark themes and focus styles.
- `app.js`: reference data, syntax highlighting, search, code tabs and copying.

Keep the main content and navigation usable without JavaScript. Search, theme
switching and code tabs enhance the static pages. Code blocks must be keyboard
focusable so users can scroll long lines on small screens. Respect reduced motion
and verify both themes at mobile and desktop widths after layout changes.

Screenshots in `images/` show actual repository examples. `ui-workspace.png` was
captured from `vectorgl_ui_editor_workspace` at 1280 × 780 using its `--smoke-test`
and `--screenshot` options with Segoe UI, then converted from BMP to PNG.
Update screenshots when the corresponding examples change.

The design uses original styles and markup. The clear getting-started paths on
[raylib](https://www.raylib.com/) and visual demonstrations on
[Slint](https://slint.dev/) informed the information hierarchy.
