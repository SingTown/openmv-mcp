import type * as vscode from "vscode";
import { openmv } from "./openmv";
import { ensureServer } from "./server-process";
import { initStatusBar } from "./status-bar";

export async function activate(context: vscode.ExtensionContext) {
    openmv.init(context.extension.packageJSON.version);
    await ensureServer(context);
    initStatusBar(context);
}

export function deactivate() {}
