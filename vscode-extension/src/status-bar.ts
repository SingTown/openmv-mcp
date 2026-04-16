import * as vscode from "vscode";
import { type CameraListInfo, openmv } from "./openmv";

class PortQuickPickItem implements vscode.QuickPickItem {
    public label: string;
    public description: string;
    public constructor(public port: CameraListInfo) {
        this.label = port.name;
        this.description = port.path;
    }
}

function toMessage(err: unknown): string {
    return err instanceof Error ? err.message : String(err);
}

export function initStatusBar(context: vscode.ExtensionContext) {
    const connectStatusBarItem = vscode.window.createStatusBarItem(
        vscode.StatusBarAlignment.Left,
        20,
    );
    connectStatusBarItem.text = "$(link) Connect";
    connectStatusBarItem.command = "openmv.connect";

    const disconnectStatusBarItem = vscode.window.createStatusBarItem(
        vscode.StatusBarAlignment.Left,
        20,
    );
    disconnectStatusBarItem.text = "$(sync-ignored) Disconnect";
    disconnectStatusBarItem.command = "openmv.disconnect";

    context.subscriptions.push(connectStatusBarItem);
    context.subscriptions.push(disconnectStatusBarItem);
    connectStatusBarItem.show();
    disconnectStatusBarItem.hide();

    async function connectCommand() {
        try {
            const ports = await openmv.scan();
            if (ports.length === 0) {
                vscode.window.showErrorMessage("No serial port found");
                return;
            }
            if (ports.length === 1) {
                await openmv.connect(ports[0].path);
                return;
            }
            const items = ports.map((port) => new PortQuickPickItem(port));
            const selected = await vscode.window.showQuickPick(items, {
                placeHolder: "Select a serial port",
            });
            if (selected) {
                await openmv.connect(selected.port.path);
            }
        } catch (err) {
            vscode.window.showErrorMessage(toMessage(err));
        }
    }

    async function disconnectCommand() {
        try {
            await openmv.disconnect();
        } catch (err) {
            vscode.window.showErrorMessage(toMessage(err));
        }
    }

    context.subscriptions.push(
        vscode.commands.registerCommand("openmv.connect", connectCommand),
        vscode.commands.registerCommand("openmv.disconnect", disconnectCommand),
    );

    openmv.on("connected", (isConnected: boolean) => {
        if (isConnected) {
            connectStatusBarItem.hide();
            disconnectStatusBarItem.show();
        } else {
            connectStatusBarItem.show();
            disconnectStatusBarItem.hide();
        }
    });

    openmv.on("error", (err: unknown) => {
        vscode.window.showErrorMessage(`OpenMV: ${toMessage(err)}`);
    });
}
