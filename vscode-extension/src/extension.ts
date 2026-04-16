import * as vscode from "vscode";

export function activate(context: vscode.ExtensionContext) {
    const disposable = vscode.commands.registerCommand(
        "openmv.helloWorld",
        () => {
            vscode.window.showInformationMessage("Hello World from OpenMV!");
        },
    );

    context.subscriptions.push(disposable);
}

export function deactivate() {}
