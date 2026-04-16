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

function getCurrentEditorText(languageId = "python"): string | null {
    let editor = vscode.window.activeTextEditor;
    if (editor?.document.languageId !== languageId) {
        editor = undefined;
    }
    if (!editor) {
        editor = vscode.window.visibleTextEditors.find(
            (x) => x.document.languageId === languageId,
        );
    }
    if (!editor) {
        vscode.window.showInformationMessage("Please open a Python file");
        return null;
    }
    return editor.document.getText();
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

    const runStatusBarItem = vscode.window.createStatusBarItem(
        vscode.StatusBarAlignment.Left,
        10,
    );
    runStatusBarItem.text = "$(debug-start) Run";
    runStatusBarItem.command = "openmv.runScript";

    const stopStatusBarItem = vscode.window.createStatusBarItem(
        vscode.StatusBarAlignment.Left,
        10,
    );
    stopStatusBarItem.text = "$(debug-stop) Stop";
    stopStatusBarItem.command = "openmv.stopScript";

    context.subscriptions.push(connectStatusBarItem);
    context.subscriptions.push(disconnectStatusBarItem);
    context.subscriptions.push(runStatusBarItem);
    context.subscriptions.push(stopStatusBarItem);
    connectStatusBarItem.show();
    disconnectStatusBarItem.hide();
    runStatusBarItem.hide();
    stopStatusBarItem.hide();

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

    async function runCommand() {
        if (!openmv.isConnected()) {
            vscode.window.showErrorMessage("Please connect a camera first");
            return;
        }
        const code = getCurrentEditorText();
        if (!code) {
            return;
        }
        try {
            await openmv.runScript(code);
        } catch (err) {
            vscode.window.showErrorMessage(toMessage(err));
        }
    }

    async function stopCommand() {
        if (!openmv.isConnected()) {
            vscode.window.showErrorMessage("Please connect a camera first");
            return;
        }
        try {
            await openmv.stopScript();
        } catch (err) {
            vscode.window.showErrorMessage(toMessage(err));
        }
    }

    context.subscriptions.push(
        vscode.commands.registerCommand("openmv.connect", connectCommand),
        vscode.commands.registerCommand("openmv.disconnect", disconnectCommand),
        vscode.commands.registerCommand("openmv.runScript", runCommand),
        vscode.commands.registerCommand("openmv.stopScript", stopCommand),
    );

    function syncButtons() {
        if (!openmv.isConnected()) {
            connectStatusBarItem.show();
            disconnectStatusBarItem.hide();
            runStatusBarItem.hide();
            stopStatusBarItem.hide();
            return;
        }
        connectStatusBarItem.hide();
        disconnectStatusBarItem.show();
        if (openmv.isRunning()) {
            runStatusBarItem.hide();
            stopStatusBarItem.show();
        } else {
            runStatusBarItem.show();
            stopStatusBarItem.hide();
        }
    }

    openmv.on("connected", syncButtons);
    openmv.on("running", syncButtons);

    openmv.on("error", (err: unknown) => {
        vscode.window.showErrorMessage(`OpenMV: ${toMessage(err)}`);
    });
}
