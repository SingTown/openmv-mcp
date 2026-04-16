# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with the VS Code extension.

## Install Dependencies

```bash
pnpm install
```

## Build

```bash
pnpm run build
pnpm run watch    # watch mode
```

## Code Quality

```bash
pnpm run lint     # check with biome
pnpm run format   # auto-format with biome
pnpm run check    # lint + format (auto-fix)
```

## Run & Debug

1. Open `vscode-extension/` in VS Code
2. Press `F5` to launch the Extension Development Host
3. Open Command Palette (`Cmd+Shift+P`) and run "OpenMV: Hello World"

## Architecture

This is a VS Code extension for OpenMV. Entry point is `src/extension.ts`.

- **src/extension.ts** — `activate()` registers commands, `deactivate()` cleans up
