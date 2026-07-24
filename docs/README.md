# VectorGL Docs
This folder contains the static documentation site for VectorGL.
## Local preview
Open index.html directly in a browser, or serve the folder with any static file server.

Examples:

python -m http.server 8080

or

npx serve docs

Then open the local URL and start at index.html.
## GitHub Pages
This folder is designed to be published as a GitHub Pages site.

Recommended setup:

- Push the repository to GitHub.
- Open repository settings.
- Enable Pages.
- Set the source to the active branch and the /docs folder.
- Use the generated Pages URL as the public documentation link.
## Files
- index.html: landing page
- overview.html: architecture and getting started
- guides.html: task-oriented usage guides
- api.html: searchable API reference
- examples.html: repository example summaries
- styles.css: shared visual system
- app.js: shared page data and interactions
