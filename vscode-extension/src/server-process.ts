import { spawnSync } from "node:child_process";
import * as crypto from "node:crypto";
import * as fs from "node:fs";
import * as path from "node:path";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import * as vscode from "vscode";
import { downloadFile } from "./utils";

export const MCP_HOST = "127.0.0.1";
export const MCP_PORT = 15257;
export const MCP_BASE_URL = `http://${MCP_HOST}:${MCP_PORT}`;

const { version: MCP_VERSION } = require("../package.json");
const RELEASE_BASE_URL = `https://github.com/SingTown/openmv-mcp/releases/download/v${MCP_VERSION}`;

type PlatformKey = "macos-arm64" | "macos-x64" | "linux-x64" | "win-x64";

function platformKey(): PlatformKey | null {
    if (process.platform === "darwin" && process.arch === "arm64")
        return "macos-arm64";
    if (process.platform === "darwin" && process.arch === "x64")
        return "macos-x64";
    if (process.platform === "linux" && process.arch === "x64")
        return "linux-x64";
    if (process.platform === "win32" && process.arch === "x64")
        return "win-x64";
    return null;
}

function assetName(version: string, key: PlatformKey): string | null {
    switch (key) {
        case "macos-arm64":
            return `openmv_mcp_server-${version}-macos-arm64`;
        case "linux-x64":
            return `openmv_mcp_server-${version}-linux-x86_64`;
        case "win-x64":
            return `openmv_mcp_server-${version}-windows-x86_64.exe`;
        case "macos-x64":
            return null;
    }
}

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

export async function ensureServer(
    context: vscode.ExtensionContext,
): Promise<void> {
    const existing = await pingInfo();
    if (isCurrent(existing)) return;
    if (existing.ok && existing.name === SERVER_NAME) {
        vscode.window.showInformationMessage(
            `Restarting openmv-mcp server: ${existing.version} → ${MCP_VERSION}`,
        );
        await shutdownRunningServer();
    }

    const key = platformKey();
    if (!key) {
        throw new Error(
            `unsupported platform ${process.platform}/${process.arch}`,
        );
    }

    const exe =
        process.platform === "win32"
            ? "openmv_mcp_server.exe"
            : "openmv_mcp_server";
    const cacheRoot = path.join(context.globalStorageUri.fsPath, "mcp-server");
    const versionDir = path.join(cacheRoot, MCP_VERSION);
    const bin = path.join(versionDir, exe);

    if (!fs.existsSync(bin)) {
        const name = assetName(MCP_VERSION, key);
        if (!name) {
            throw new Error(
                `openmv-mcp does not publish a build for ${key} yet`,
            );
        }
        fs.mkdirSync(versionDir, { recursive: true });
        const tmpPath = `${bin}.downloading.${process.pid}.${crypto
            .randomBytes(4)
            .toString("hex")}`;
        await downloadFile(
            `${RELEASE_BASE_URL}/${name}`,
            tmpPath,
            `Downloading openmv-mcp ${MCP_VERSION}`,
        );
        if (process.platform !== "win32") {
            fs.chmodSync(tmpPath, 0o755);
        }
        try {
            fs.renameSync(tmpPath, bin);
        } catch (err) {
            // Another window may have won the race. On Windows `rename` fails
            // if the target exists; on POSIX it replaces atomically so this
            // branch only fires on real errors.
            try {
                fs.unlinkSync(tmpPath);
            } catch {}
            if (!fs.existsSync(bin)) throw err;
        }
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
