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
pnpm run lint       # check with biome
pnpm run format     # auto-format with biome
pnpm run typecheck  # tsc --noEmit
pnpm run check      # biome (auto-fix) + typecheck
```

## Run & Debug

1. Open `vscode-extension/` in VS Code
2. Press `F5` to launch the Extension Development Host
3. The extension activates on startup, downloads the MCP server if needed, and launches it as a daemon

## Architecture

This is a VS Code extension for OpenMV. Uses esbuild for bundling (output to `dist/`). Entry point is `src/extension.ts`.

- **src/extension.ts** — `activate()` calls `ensureServer()` to download and start the MCP server
- **src/server-process.ts** — MCP server binary download, daemon launch, and health check via MCP ping
- **src/utils.ts** — `downloadFile()` with progress notification
