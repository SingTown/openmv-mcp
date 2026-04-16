import type * as vscode from "vscode";
import { ensureServer } from "./server-process";

export async function activate(context: vscode.ExtensionContext) {
    await ensureServer(context);
}

export function deactivate() {}
