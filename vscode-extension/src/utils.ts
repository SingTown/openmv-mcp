import * as fs from "node:fs";
import * as path from "node:path";
import * as vscode from "vscode";

export async function downloadFile(
    url: string,
    destPath: string,
    title = `Downloading ${path.basename(destPath)}`,
): Promise<void> {
    await vscode.window.withProgress(
        {
            location: vscode.ProgressLocation.Notification,
            title,
            cancellable: false,
        },
        async (progress) => {
            const res = await fetch(url);
            if (!res.ok || !res.body) {
                throw new Error(`download failed: HTTP ${res.status}`);
            }
            const total = Number(res.headers.get("content-length")) || 0;
            let received = 0;
            let lastPct = 0;
            const out = fs.createWriteStream(destPath);
            const writeError = new Promise<never>((_, reject) => {
                out.once("error", reject);
            });
            const download = async () => {
                for await (const chunk of res.body as unknown as AsyncIterable<Uint8Array>) {
                    if (!out.write(chunk)) {
                        await new Promise<void>((resolve) =>
                            out.once("drain", () => resolve()),
                        );
                    }
                    received += chunk.byteLength;
                    if (total > 0) {
                        const pct = Math.floor((received / total) * 100);
                        if (pct > lastPct) {
                            progress.report({
                                increment: pct - lastPct,
                                message: `${pct}%`,
                            });
                            lastPct = pct;
                        }
                    } else {
                        progress.report({
                            message: `${(received / 1024 / 1024).toFixed(1)} MB`,
                        });
                    }
                }
                out.end();
                await new Promise<void>((resolve) =>
                    out.once("close", () => resolve()),
                );
            };
            try {
                await Promise.race([download(), writeError]);
            } catch (e) {
                out.destroy();
                await fs.promises.unlink(destPath).catch(() => {});
                throw e;
            }
        },
    );
}
