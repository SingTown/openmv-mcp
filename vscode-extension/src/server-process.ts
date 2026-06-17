import { execFile } from "node:child_process";
import * as fs from "node:fs";
import * as path from "node:path";
import { promisify } from "node:util";
import type * as vscode from "vscode";

export const MCP_HOST = "127.0.0.1";
export const MCP_PORT = 15257;
export const MCP_BASE_URL = `http://${MCP_HOST}:${MCP_PORT}`;

const execFileAsync = promisify(execFile);

function bundledServerPath(context: vscode.ExtensionContext): string {
    const exe =
        process.platform === "win32"
            ? "openmv_mcp_server.exe"
            : "openmv_mcp_server";
    const target = `${process.platform}-${process.arch}`;
    return path.join(
        context.extensionUri.fsPath,
        "tools",
        "mcp-server",
        target,
        exe,
    );
}

export async function ensureServer(
    context: vscode.ExtensionContext,
): Promise<void> {
    const bin = bundledServerPath(context);
    if (!fs.existsSync(bin)) {
        throw new Error(
            `bundled openmv_mcp_server missing for ${process.platform}-${process.arch}: ${bin}`,
        );
    }

    const logDir = context.globalStorageUri.fsPath;
    fs.mkdirSync(logDir, { recursive: true });
    const logPath = path.join(logDir, "openmv-mcp-server.log");

    await execFileAsync(bin, ["--port", String(MCP_PORT), "--log", logPath], {
        windowsHide: true,
        encoding: "utf8",
    });
}
