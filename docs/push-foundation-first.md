# Push Foundation First
Use this guide if you want your GitHub history to show how VectorGL was built from the ground up.

This is the better order for a public repository because it tells a proper engineering story:

- platform setup
- renderer bootstrap
- shader pipeline
- frame lifecycle
- basic drawing
- text support
- textbox and input
- UI helpers
- example integration
- docs

Do not start from TextBox if your goal is to present the whole engine journey.
## Important Scope
This guide is mainly about the core library files.

That means you should think in this order first:

- public headers in include/PublicHeaders/vectorgl/
- internal headers in include/InternalHeaders/vectorgl/detail/
- implementation files in src/
- shaders in shaders/
- build wiring in CMakeLists.txt and include/*/CMakeLists.txt

Only after the main library code is in a good sequence should you push:

- example/
- README.md
- files in docs/
## Phase 1: Platform Setup
Goal: Create the minimum base required to build the library and link the graphics stack.

Main files to look at in this repo:

- CMakeLists.txt
- cmake/dependencies.cmake
- example/CMakeLists.txt

Code parts to stage separately if needed:

- dependency setup for GLFW
- dependency setup for GLAD
- library target wiring in CMake

Typical work:

- add GLFW dependency
- add GLAD dependency
- configure CMake for example app

Example commit titles:

- add glfw dependency
- add glad dependency
- setup example application build
## Phase 2: Renderer Bootstrap
Goal: Create the renderer backbone before adding real drawing features.

Main files to look at in this repo:

- include/PublicHeaders/vectorgl/renderer.hpp
- include/InternalHeaders/vectorgl/detail/gl_handle.hpp
- include/InternalHeaders/vectorgl/detail/shader_utils.hpp
- src/renderer.cpp
- src/shader_utils.cpp

Code parts to stage separately if needed:

- renderer class declaration
- OpenGL object wrappers in gl_handle.hpp
- shader compile and link helpers
- renderer initialization and cleanup

Typical work:

- create example window
- initialize GLFW
- create OpenGL context
- add shutdown cleanup

Example commit titles:

- initialize glfw for examples
- create example window and opengl context
- add window shutdown cleanup
## Phase 3: Shader Pipeline
Goal: Add the shader assets and shader-loading path used by the renderer.

Main files to look at in this repo:

- shaders/sdf.vert
- shaders/sdf.frag
- shaders/path.vert
- shaders/path.frag
- shaders/textured.vert
- shaders/textured.frag
- src/shader_utils.cpp
- src/renderer.cpp
- CMakeLists.txt

Code parts to stage separately if needed:

- shader source files
- shader copy/setup in CMake
- shader compile/load path in code

Typical work:

- renderer class skeleton
- buffer and VAO setup
- shader utility helpers
- renderer lifetime management

Example commit titles:

- add renderer class skeleton
- add renderer buffer setup
- add shader compile and link helpers
- add renderer initialization and teardown
## Phase 4: Frame Lifecycle
Goal: Define how a frame starts, renders, and ends.

Main files to look at in this repo:

- include/PublicHeaders/vectorgl/canvas.hpp
- src/canvas.cpp
- src/renderer.cpp

Code parts to stage separately if needed:

- Canvas::init() / Canvas::destroy()
- Canvas::beginFrame(...)
- Canvas::endFrame()
- viewport and projection setup inside renderer/canvas flow

Typical work:

- begin frame
- end frame
- viewport setup
- projection setup

Example commit titles:

- add canvas frame begin and end api
- add viewport and projection setup
## Phase 5: Basic Drawing
Goal: Make the engine render visible primitives.

Main files to look at in this repo:

- include/PublicHeaders/vectorgl/canvas.hpp
- include/PublicHeaders/vectorgl/color.hpp
- include/PublicHeaders/vectorgl/path.hpp
- src/canvas.cpp
- src/path.cpp
- src/renderer.cpp
- shaders/sdf.vert
- shaders/sdf.frag
- shaders/path.vert
- shaders/path.frag

Code parts to stage separately if needed:

- fill and stroke state setters
- rectangle drawing
- circle drawing
- rounded rectangle drawing
- path API and path rendering

Typical work:

- fill and stroke state
- rectangle rendering
- circle rendering
- rounded rectangle rendering
- path basics

Example commit titles:

- add canvas fill and stroke state
- add rectangle rendering
- add circle rendering
- add rounded rectangle rendering
- add basic path drawing api
## Phase 6: Text Support
Goal: Add the text system before attempting text input widgets.

Main files to look at in this repo:

- include/PublicHeaders/vectorgl/font.hpp
- include/PublicHeaders/vectorgl/canvas.hpp
- src/font.cpp
- src/canvas.cpp
- src/renderer.cpp
- shaders/textured.vert
- shaders/textured.frag

Code parts to stage separately if needed:

- font loading
- atlas generation
- text rendering path
- measureText(...)
- lineHeight()

Typical work:

- font loading
- text rendering
- text measurement helpers
- line height support

Example commit titles:

- add font loading support
- add text rendering
- add canvas text measurement helpers
- add canvas line height helper
## Phase 7: TextBox And Input Handling
Goal: Add editable text only after text rendering already exists.

Main files to look at in this repo:

- include/PublicHeaders/vectorgl/text_box.hpp
- include/PublicHeaders/vectorgl/glfw_text_box.hpp
- src/text_box.cpp

Code parts to stage separately if needed:

- textbox public API
- textbox rendering and focus state
- typing and caret behavior
- GLFW adapter binding and key mapping
- selection handling
- clipboard shortcuts

Typical work:

- textbox public API
- textbox rendering and state
- textbox editing behavior
- GLFW input adapter
- selection support
- clipboard shortcuts

Example commit titles:

- add textbox public api
- add textbox rendering and state implementation
- add textbox editing behavior
- add glfw textbox controller
- add textbox selection support
- add textbox clipboard shortcuts
## Phase 8: Higher-Level UI Helpers
Goal: Add convenience wrappers once the lower-level parts are stable.

Main files to look at in this repo:

- include/PublicHeaders/vectorgl/ui.hpp
- include/PublicHeaders/CMakeLists.txt
- CMakeLists.txt

Code parts to stage separately if needed:

- Label helper
- Button helper
- TextBox aliases in ui.hpp
- public header export registration

Typical work:

- label helper
- button helper
- ui wrapper header

Example commit titles:

- add lightweight label helper
- add lightweight button helper
- add small ui helper layer
## Phase 9: Example Integration
Goal: After the main library code is in place, connect it to a working example.

Main files to look at in this repo:

- example/text_input_demo.cpp
- example/main.cpp
- example/demo_all.cpp
- example/CMakeLists.txt

Code parts to stage separately if needed:

- demo target registration
- callback wiring
- widget creation and layout
- render loop integration

Typical work:

- connect widgets to a demo
- add interaction behavior
- show current value and status

Example commit titles:

- add text input demo
- update text input demo for ui widgets
- polish text input demo interaction
## Phase 10: Public Documentation
Goal: Document usage only after the implementation is stable.

Main files to look at in this repo:

- include/PublicHeaders/vectorgl/text_box.hpp
- include/PublicHeaders/vectorgl/glfw_text_box.hpp
- include/PublicHeaders/vectorgl/ui.hpp
- README.md
- docs/push-in-parts.md
- docs/push-foundation-first.md

Code parts to stage separately if needed:

- public Doxygen comments in headers
- README usage examples
- push strategy guides

Typical work:

- header docs
- README usage section
- extra guides

Example commit titles:

- document textbox and ui headers
- document text input widgets in readme
- add push guide for incremental publishing
## Main Code Push Order
If you want to focus only on main code first, use this order:

- CMakeLists.txt
- cmake/dependencies.cmake
- include/PublicHeaders/vectorgl/renderer.hpp
- include/InternalHeaders/vectorgl/detail/gl_handle.hpp
- include/InternalHeaders/vectorgl/detail/shader_utils.hpp
- src/shader_utils.cpp
- src/renderer.cpp
- include/PublicHeaders/vectorgl/canvas.hpp
- src/canvas.cpp
- include/PublicHeaders/vectorgl/path.hpp
- src/path.cpp
- include/PublicHeaders/vectorgl/font.hpp
- src/font.cpp
- include/PublicHeaders/vectorgl/text_box.hpp
- src/text_box.cpp
- include/PublicHeaders/vectorgl/glfw_text_box.hpp
- include/PublicHeaders/vectorgl/ui.hpp

Only after that should you push:

- files in example/
- README.md
- docs/
## Suggested Multi-Day Schedule
If you do not want too much code pushed in one day, spread it like this:

- Day 1: platform setup and renderer bootstrap
- Day 2: shader pipeline and frame lifecycle
- Day 3: basic drawing primitives
- Day 4: text support
- Day 5: textbox base and editing behavior
- Day 6: input adapter, selection, and clipboard
- Day 7: UI helpers
- Day 8: example update
- Day 9: public docs and README
## Rule For Every Push
For each push:

- pick one small step
- stage only that step
- commit with a clear message
- build if possible
- push

Command pattern:

git add <files>

git commit -m "<clear message>"

git push

For the first push:

git push -u origin main
## When Files Overlap
If a file contains code for more than one phase, use:

git add -p

That is the correct way to keep your history aligned with the implementation order.
## Real Repo File Map By Area
If you want a quick mapping from feature area to repo files, use this:
### Build and setup
- CMakeLists.txt
- cmake/dependencies.cmake
- cmake/tooling.cmake
- CMakePresets.json
- example/CMakeLists.txt
### Core drawing API
- include/PublicHeaders/vectorgl/canvas.hpp
- include/PublicHeaders/vectorgl/color.hpp
- include/PublicHeaders/vectorgl/path.hpp
- src/canvas.cpp
- src/path.cpp
### Renderer internals
- include/PublicHeaders/vectorgl/renderer.hpp
- include/InternalHeaders/vectorgl/detail/gl_handle.hpp
- include/InternalHeaders/vectorgl/detail/shader_utils.hpp
- src/renderer.cpp
- src/shader_utils.cpp
### Text system
- include/PublicHeaders/vectorgl/font.hpp
- src/font.cpp
- shaders/textured.vert
- shaders/textured.frag
### Text input
- include/PublicHeaders/vectorgl/text_box.hpp
- include/PublicHeaders/vectorgl/glfw_text_box.hpp
- src/text_box.cpp
### Small UI layer
- include/PublicHeaders/vectorgl/ui.hpp
- include/PublicHeaders/CMakeLists.txt
### Example and docs
- example/text_input_demo.cpp
- README.md
- docs/push-in-parts.md
- docs/push-foundation-first.md
