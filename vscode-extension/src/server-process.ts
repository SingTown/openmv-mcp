import { spawnSync } from "node:child_process";
import * as fs from "node:fs";
import * as path from "node:path";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import * as vscode from "vscode";

export const MCP_HOST = "127.0.0.1";
export const MCP_PORT = 15257;
export const MCP_BASE_URL = `http://${MCP_HOST}:${MCP_PORT}`;

const { version: MCP_VERSION } = require("../package.json");

const SERVER_NAME = "openmv-mcp-server";

interface ServerInfo {
    ok: boolean;
    name?: string;
    version?: string;
}

async function pingInfo(timeoutMs = 500): Promise<ServerInfo> {
    const client = new Client({
        name: "openmv-vscode-healthcheck",
        version: "1.0.0",
    });
    const transport = new StreamableHTTPClientTransport(
        new URL(`${MCP_BASE_URL}/mcp`),
    );
    try {
        await client.connect(transport);
        await client.ping({
            signal: AbortSignal.timeout(timeoutMs),
            timeout: timeoutMs,
        });
        const impl = client.getServerVersion();
        return { ok: true, name: impl?.name, version: impl?.version };
    } catch {
        return { ok: false };
    } finally {
        try {
            await client.close();
        } catch {}
    }
}

function isCurrent(info: ServerInfo): boolean {
    return info.ok && info.name === SERVER_NAME && info.version === MCP_VERSION;
}

async function shutdownRunningServer(): Promise<void> {
    try {
        await fetch(`${MCP_BASE_URL}/shutdown`, { method: "POST" });
    } catch {
        // server may have already exited — fall through to wait
    }
    for (let i = 0; i < 20; i++) {
        const info = await pingInfo();
        if (!info.ok) return;
        await new Promise((r) => setTimeout(r, 100));
    }
    throw new Error("existing openmv-mcp server did not shut down within 2s");
}

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
    const existing = await pingInfo();
    if (isCurrent(existing)) return;
    if (existing.ok && existing.name === SERVER_NAME) {
        vscode.window.showInformationMessage(
            vscode.l10n.t(
                "Restarting openmv-mcp server: {0} → {1}",
                existing.version ?? "",
                MCP_VERSION,
            ),
        );
        await shutdownRunningServer();
    }

    const bin = bundledServerPath(context);
    if (!fs.existsSync(bin)) {
        throw new Error(
            `bundled openmv_mcp_server missing for ${process.platform}-${process.arch}: ${bin}`,
        );
    }

    const result = spawnSync(bin, ["--daemon", "--port", String(MCP_PORT)], {
        stdio: ["ignore", "pipe", "pipe"],
        windowsHide: true,
        encoding: "utf8",
    });
    if (result.error) {
        throw new Error(`failed to launch ${bin}: ${result.error.message}`);
    }
    if (result.status !== 0) {
        const stderr = (result.stderr || "").trim();
        throw new Error(
            `${bin} exited with code ${result.status}${
                stderr ? `: ${stderr}` : ""
            }`,
        );
    }

    for (let i = 0; i < 40; i++) {
        if (isCurrent(await pingInfo())) return;
        await new Promise((r) => setTimeout(r, 250));
    }
    throw new Error("openmv_mcp_server did not become ready in time");
}
