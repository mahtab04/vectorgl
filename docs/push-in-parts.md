# Push In Parts
This guide shows how to publish the current VectorGL work step by step, the same way you would build a feature step by step.

Instead of pushing one large batch, push each small implementation step as its own commit.
## Two Valid Push Orders
There are two different cases:

- You are only publishing the new textbox and UI work on top of an already existing project.
- You want GitHub history to show how the whole engine was built from the ground up.

If you want case 2, use the dedicated guide in docs/push-foundation-first.md.

If your goal is the second case, then you should not start from TextBox.

You should start from the foundation:

- basic OpenGL and GLFW setup
- window creation
- renderer bootstrap
- shader loading
- frame begin/end flow
- basic drawing primitives
- text measurement and text rendering
- textbox and input handling
- higher-level UI helpers
- demo and docs

The textbox-first order only makes sense if you are pushing just the recent textbox/UI changes as a separate feature branch of work.
## Idea
Think like this:

- First create the base API.
- Then make it work.
- Then improve interaction.
- Then add convenience helpers.
- Then update the demo.
- Then update documentation.

That gives you a natural development story on GitHub.
## If This Folder Is Not Yet A Git Repo
Run these commands first:

git init

git branch -M main

git remote add origin <your-github-repo-url>

Example:

git remote add origin https://github.com/your-name/vectorgl.git
## Step-By-Step Push Plan
## Recommended Foundation-First Order
If you want your public history to read like an implementation timeline, use this order instead of starting from textbox code.
### Stage 1: Platform And Windowing
Start with the minimum code required to open a window and create an OpenGL context.

Typical scope:

- GLFW setup
- GLAD loading
- window creation
- shutdown path

Example commit titles:

- setup glfw and glad
- create example window and opengl context
- add clean shutdown path
### Stage 2: Core Renderer Bootstrap
Then push the pieces needed to make rendering possible.

Typical scope:

- renderer class skeleton
- shader compile and link helpers
- frame begin/end API
- viewport and projection setup

Example commit titles:

- add renderer bootstrap
- add shader compilation utilities
- add frame lifecycle methods
### Stage 3: Basic Drawing
Only after the renderer exists should you push the first visible drawing features.

Typical scope:

- fill color and stroke state
- rectangle drawing
- circle and rounded rectangle drawing
- path basics

Example commit titles:

- add canvas fill and stroke state
- add rectangle drawing primitives
- add circle and rounded rectangle rendering
- add basic path drawing api
### Stage 4: Text Support
Text input should come after text rendering basics exist.

Typical scope:

- font loading
- text rendering
- text measurement helpers

Example commit titles:

- add font loading support
- add text rendering
- add canvas text measurement helpers
### Stage 5: Interactive Text Editing
Only now does textbox work become a natural next step.

Typical scope:

- textbox public API
- textbox implementation
- editing behavior
- GLFW input adapter
- selection
- clipboard

Example commit titles:

- add textbox public api
- add textbox rendering and state implementation
- add textbox editing behavior
- add glfw textbox controller
- add textbox selection support
- add textbox clipboard shortcuts
### Stage 6: Higher-Level UI Layer
After the lower-level parts work, add convenience widgets.

Typical scope:

- label helper
- button helper
- ui wrapper header

Example commit titles:

- add lightweight label helper
- add lightweight button helper
- add small ui helper layer
### Stage 7: Demo And Documentation
Finish with examples and docs.

Typical scope:

- text input demo update
- public header docs
- README guide

Example commit titles:

- update text input demo for ui widgets
- document textbox and ui headers
- document text input widgets in readme
## Step 1: Add Text Measurement Support To Canvas
This is the base needed before a textbox can measure text width and caret position.

Files:

- include/PublicHeaders/vectorgl/canvas.hpp
- src/canvas.cpp

git add include/PublicHeaders/vectorgl/canvas.hpp src/canvas.cpp

git commit -m "add text measurement support to canvas"

git push -u origin main
## Step 2: Add Basic TextBox API Only
This step introduces the public textbox type and the initial implementation.

Files:

- include/PublicHeaders/vectorgl/text_box.hpp
- src/text_box.cpp

git add include/PublicHeaders/vectorgl/text_box.hpp src/text_box.cpp

git commit -m "add basic textbox widget"

git push
## Step 3: Add Text Editing Behavior
If your textbox file contains both widget creation and editing logic, this is where git add -p is useful. Stage only the hunks that add typing, caret movement, backspace, delete, and submit behavior.

Files:

- include/PublicHeaders/vectorgl/text_box.hpp
- src/text_box.cpp

git add -p include/PublicHeaders/vectorgl/text_box.hpp src/text_box.cpp

git commit -m "add textbox editing and caret behavior"

git push
## Step 4: Add GLFW Input Adapter
Now connect the textbox to real keyboard and mouse input.

Files:

- include/PublicHeaders/vectorgl/glfw_text_box.hpp

git add include/PublicHeaders/vectorgl/glfw_text_box.hpp

git commit -m "add glfw textbox input adapter"

git push
## Step 5: Add Pointer Selection Support
This is a separate behavior step: clicking, dragging, and selecting text.

Files:

- include/PublicHeaders/vectorgl/text_box.hpp
- src/text_box.cpp
- include/PublicHeaders/vectorgl/glfw_text_box.hpp

git add -p include/PublicHeaders/vectorgl/text_box.hpp src/text_box.cpp include/PublicHeaders/vectorgl/glfw_text_box.hpp

git commit -m "add textbox selection support"

git push
## Step 6: Add Clipboard Shortcuts
This is another small logical step: copy, cut, paste, and select-all.

Files:

- include/PublicHeaders/vectorgl/text_box.hpp
- src/text_box.cpp
- include/PublicHeaders/vectorgl/glfw_text_box.hpp

git add -p include/PublicHeaders/vectorgl/text_box.hpp src/text_box.cpp include/PublicHeaders/vectorgl/glfw_text_box.hpp

git commit -m "add clipboard shortcuts to textbox"

git push
## Step 7: Add Small UI Layer
Now add the lightweight UI wrapper types.

Files:

- include/PublicHeaders/vectorgl/ui.hpp
- include/PublicHeaders/CMakeLists.txt
- CMakeLists.txt

git add include/PublicHeaders/vectorgl/ui.hpp include/PublicHeaders/CMakeLists.txt CMakeLists.txt

git commit -m "add small ui layer with label button and textbox aliases"

git push
## Step 8: Update Demo To Use The New Widgets
Now show the feature in action.

Files:

- example/text_input_demo.cpp

git add example/text_input_demo.cpp

git commit -m "update text input demo to use ui widgets"

git push
## Step 9: Add Public API Documentation
This step is only for public header comments.

Files:

- include/PublicHeaders/vectorgl/text_box.hpp
- include/PublicHeaders/vectorgl/glfw_text_box.hpp
- include/PublicHeaders/vectorgl/ui.hpp

git add include/PublicHeaders/vectorgl/text_box.hpp include/PublicHeaders/vectorgl/glfw_text_box.hpp include/PublicHeaders/vectorgl/ui.hpp

git commit -m "document textbox and ui public api"

git push
## Step 10: Update README
Documentation should come after the implementation is already in place.

Files:

- README.md

git add README.md

git commit -m "document text input and ui widget usage in readme"

git push
## When To Use git add -p
Use patch mode when one file contains multiple kinds of work and you want to split them into separate commits.

Example:

- one hunk adds caret movement
- another hunk adds selection
- another hunk adds clipboard support

Then do:

git add -p

and stage only the parts needed for the current step.
## Better Commit Style
Bad:

- update ui
- textbox changes
- many fixes

Better:

- add basic textbox widget
- add glfw textbox input adapter
- add textbox selection support
- add clipboard shortcuts to textbox
- document textbox and ui public api
## Very Small Push Strategy
If you want to go even smaller, use this order:

- create API
- add implementation
- add input integration
- add selection
- add clipboard
- add helper UI layer
- update demo
- document headers
- update README

This is the cleanest way to make your GitHub history read like a development timeline.
## Tiny Commit Checklist
If you want to push in very small parts, follow this exact checklist.
### Commit 1
Purpose: Add text measurement support needed by future text widgets.

Files:

- include/PublicHeaders/vectorgl/canvas.hpp
- src/canvas.cpp

Commit message:

add canvas text measurement helpers
### Commit 2
Purpose: Create the public TextBox type and its basic API surface.

Files:

- include/PublicHeaders/vectorgl/text_box.hpp

Commit message:

add textbox public api
### Commit 3
Purpose: Add the initial textbox implementation and rendering.

Files:

- src/text_box.cpp

Commit message:

add textbox rendering and state implementation
### Commit 4
Purpose: Add basic keyboard editing support such as typing, backspace, delete, and caret movement.

Files:

- include/PublicHeaders/vectorgl/text_box.hpp
- src/text_box.cpp

Commit message:

add textbox editing behavior
### Commit 5
Purpose: Connect the textbox to GLFW input events.

Files:

- include/PublicHeaders/vectorgl/glfw_text_box.hpp

Commit message:

add glfw textbox controller
### Commit 6
Purpose: Add mouse-based selection support.

Files:

- include/PublicHeaders/vectorgl/text_box.hpp
- src/text_box.cpp
- include/PublicHeaders/vectorgl/glfw_text_box.hpp

Commit message:

add textbox selection support
### Commit 7
Purpose: Add clipboard operations and shortcuts.

Files:

- include/PublicHeaders/vectorgl/text_box.hpp
- src/text_box.cpp
- include/PublicHeaders/vectorgl/glfw_text_box.hpp

Commit message:

add textbox clipboard shortcuts
### Commit 8
Purpose: Add the small ui.hpp convenience layer.

Files:

- include/PublicHeaders/vectorgl/ui.hpp
- include/PublicHeaders/CMakeLists.txt
- CMakeLists.txt

Commit message:

add lightweight ui helper layer
### Commit 9
Purpose: Update the text input demo to use the new UI helpers.

Files:

- example/text_input_demo.cpp

Commit message:

update text input demo for ui widgets
### Commit 10
Purpose: Add public header documentation for the new API.

Files:

- include/PublicHeaders/vectorgl/text_box.hpp
- include/PublicHeaders/vectorgl/glfw_text_box.hpp
- include/PublicHeaders/vectorgl/ui.hpp

Commit message:

document textbox and ui headers
### Commit 11
Purpose: Document the user-facing setup in the README.

Files:

- README.md

Commit message:

document text input widgets in readme
## Exact Command Pattern
For each step, use the same pattern:

git add <files-for-this-step>

git commit -m "<message-for-this-step>"

git push

For the first push only:

git push -u origin main
## Best Way To Split A Shared File
Some steps reuse the same files, especially:

- include/PublicHeaders/vectorgl/text_box.hpp
- src/text_box.cpp
- include/PublicHeaders/vectorgl/glfw_text_box.hpp

When that happens, use:

git add -p

That lets you stage only the hunks for the current tiny step.
## Copy-Paste Command Sequence
If you want an exact command flow, use this sequence and stop after each push.
### First-time repo setup
git init

git branch -M main

git remote add origin <your-github-repo-url>
### Push 1
git add include/PublicHeaders/vectorgl/canvas.hpp src/canvas.cpp

git commit -m "add canvas text measurement helpers"

git push -u origin main
### Push 2
git add -p include/PublicHeaders/vectorgl/text_box.hpp

git commit -m "add textbox public api"

git push
### Push 3
git add -p src/text_box.cpp

git commit -m "add textbox rendering and state implementation"

git push
### Push 4
git add -p include/PublicHeaders/vectorgl/text_box.hpp src/text_box.cpp

git commit -m "add textbox editing behavior"

git push
### Push 5
git add include/PublicHeaders/vectorgl/glfw_text_box.hpp

git commit -m "add glfw textbox controller"

git push
### Push 6
git add -p include/PublicHeaders/vectorgl/text_box.hpp src/text_box.cpp include/PublicHeaders/vectorgl/glfw_text_box.hpp

git commit -m "add textbox selection support"

git push
### Push 7
git add -p include/PublicHeaders/vectorgl/text_box.hpp src/text_box.cpp include/PublicHeaders/vectorgl/glfw_text_box.hpp

git commit -m "add textbox clipboard shortcuts"

git push
### Push 8
git add include/PublicHeaders/vectorgl/ui.hpp include/PublicHeaders/CMakeLists.txt CMakeLists.txt

git commit -m "add lightweight ui helper layer"

git push
### Push 9
git add example/text_input_demo.cpp

git commit -m "update text input demo for ui widgets"

git push
### Push 10
git add include/PublicHeaders/vectorgl/text_box.hpp include/PublicHeaders/vectorgl/glfw_text_box.hpp include/PublicHeaders/vectorgl/ui.hpp

git commit -m "document textbox and ui headers"

git push
### Push 11
git add README.md

git commit -m "document text input widgets in readme"

git push
## Mapping To The Implementation Plan
If you want your GitHub history to match the engineering story in implementationplan.md, use this interpretation:

- Canvas text helpers This is the foundation needed before text input can measure glyph advance and line height.

- TextBox public API This is similar to defining the public surface before filling in engine internals.

- TextBox rendering and state This is the first working implementation pass.

- Text editing behavior This is the first usable interaction pass.

- GLFW adapter This is the platform integration step, similar to connecting engine code to the app layer.

- Selection support This is the first UX refinement pass.

- Clipboard shortcuts This is the second UX refinement pass.

- UI helper layer This is the convenience abstraction step, like building a small higher-level layer on top of the core primitives.

- Demo update This is the validation/demo phase.

- Header documentation This is public API polish.

- README update This is external documentation and onboarding.
## If You Want To Go Even Slower
You can spread these 11 pushes across multiple days.

Example schedule:

- Day 1: Push 1 and Push 2
- Day 2: Push 3 and Push 4
- Day 3: Push 5
- Day 4: Push 6 and Push 7
- Day 5: Push 8
- Day 6: Push 9
- Day 7: Push 10 and Push 11

That keeps each day small and easy to explain.
## Notes
- Try to make each step build before committing.
- Push after each small clean step.
- Keep demo and docs after core library code.
- If one step feels too large, split it again.
