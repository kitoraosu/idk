# MiniTetris (single-file)

This repo contains a single-file Tetris clone (tetris.cpp) using the Win32 API and GDI for rendering.

Files added:
- tetris.cpp — the full game (single file)
- CMakeLists.txt — simple CMake to build on Windows
- .github/workflows/windows-build.yml — builds on windows-latest (x64) and creates a Release with the ZIP containing the exe

How it works:
- Push to main or run the workflow manually (Actions -> Build and release Windows x64).
- The workflow configures CMake, builds Release x64, packages the tetris.exe into tetris-windows-x64.zip and creates a GitHub Release with that ZIP asset.

Controls:
- Left / Right: move piece
- Up: rotate
- Down: soft drop
- Space: hard drop
- P: pause
- R (after game over): restart

Notes:
- The game uses plain Win32 GDI; no external libraries are required.
- The workflow uses the built-in CMake on the Windows runner; if you need different options adjust the workflow.