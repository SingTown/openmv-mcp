import * as vscode from "vscode";
import { openmv } from "./openmv";
import { MCP_BASE_URL } from "./server-process";

let currentStreamUrl: string | null = null;

function renderHtml(streamUrl: string | null): string {
    if (!streamUrl) {
        return `<!doctype html><html><head><style>html,body{margin:0!important;padding:0!important;width:100%;height:100%;background:#000}</style></head><body></body></html>`;
    }
    return `<!doctype html><html><head><meta http-equiv="Content-Security-Policy" content="default-src 'none'; img-src http: https:; style-src 'unsafe-inline'"><style>html,body{margin:0!important;padding:0!important;width:100%;height:100%;background:#000;display:flex;align-items:center;justify-content:center;overflow:hidden}</style></head><body><img src="${streamUrl}" style="width:100%;height:100%;object-fit:contain;display:block"/></body></html>`;
}

class FramebufferWebviewViewProvider implements vscode.WebviewViewProvider {
    public view: vscode.WebviewView | null = null;

    resolveWebviewView(webviewView: vscode.WebviewView) {
        webviewView.webview.options = { enableScripts: false };
        webviewView.webview.html = renderHtml(currentStreamUrl);
        this.view = webviewView;
    }

    refresh() {
        if (this.view) {
            this.view.webview.html = renderHtml(currentStreamUrl);
        }
    }
}

export function initFrameView(context: vscode.ExtensionContext) {
    const provider = new FramebufferWebviewViewProvider();
    context.subscriptions.push(
        vscode.window.registerWebviewViewProvider("openmv.frameView", provider),
    );

    openmv.on("connected", (isConnected: boolean) => {
        if (isConnected) {
            const path = openmv.getConnectedPath();
            if (!path) return;
            currentStreamUrl = `${MCP_BASE_URL}/stream/frame?camera=${encodeURIComponent(path)}`;
        } else {
            currentStreamUrl = null;
        }
        provider.refresh();
    });

    openmv.on("running", (isRunning: boolean) => {
        if (isRunning) {
            provider.view?.show(true);
        }
    });
}
