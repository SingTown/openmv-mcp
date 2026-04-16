import type * as vscode from "vscode";
import { initFrameView } from "./frame";
import { openmv } from "./openmv";
import { ensureServer } from "./server-process";
import { initStatusBar } from "./status-bar";
import { initTerminal } from "./terminal";

export async function activate(context: vscode.ExtensionContext) {
    openmv.init(context.extension.packageJSON.version);
    await ensureServer(context);
    initStatusBar(context);
    initTerminal(context);
    initFrameView(context);
}

export function deactivate() {}
