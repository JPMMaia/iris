import * as vscode from 'vscode';
import { LanguageClient } from 'vscode-languageclient/node.js';
import * as path from 'path';

export let doc: vscode.TextDocument;
export let editor: vscode.TextEditor;
export let documentEol: string;
export let platformEol: string;

export async function activate(docUri: vscode.Uri): Promise<LanguageClient> {
	// The extensionId is `publisher.name` from package.json
	const ext = vscode.extensions.getExtension('JPMMaia.iris')!;
	const client: LanguageClient = await ext.activate();
	try {
		doc = await vscode.workspace.openTextDocument(docUri);
		editor = await vscode.window.showTextDocument(doc);
	} catch (e) {
		console.error(e);
	}
	return client;
}

async function sleep(ms: number) {
	return new Promise(resolve => setTimeout(resolve, ms));
}

export const get_document_path = (p: string) => {
	return path.resolve(__dirname, '../../test_fixture', p);
};
export const get_document_uri = (p: string) => {
	return vscode.Uri.file(get_document_path(p));
};

export async function set_test_content(content: string): Promise<boolean> {
	const all = new vscode.Range(
		doc.positionAt(0),
		doc.positionAt(doc.getText().length)
	);
	return editor.edit(eb => eb.replace(all, content));
}
