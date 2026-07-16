import * as language_client from 'vscode-languageclient';
import { LanguageClient } from 'vscode-languageclient/node.js';
import * as assert from 'assert';
import * as vscode from 'vscode';
import { get_document_path, get_document_uri, activate } from './helper.js';

// The 'other' artifact includes "./**/*.iris", so a file created here is picked up by a rebuild
// without any change to the fixture.
const created_document_path = get_document_path("projects/other/file_watcher_created.iris");
const created_document_uri = vscode.Uri.file(created_document_path);

const created_document_contents = `module file_watcher_created;

export function file_watcher_created_function() -> ()
{
    return 0
`;

async function delete_created_document(): Promise<void> {
	try {
		await vscode.workspace.fs.delete(created_document_uri, { useTrash: false });
	} catch {
		// The file is only present when a test got far enough to write it.
	}
}

// The server rebuilds asynchronously, several hops after the file is written: the editor's
// watcher notifies the client, which notifies the server, which rebuilds. Waiting for the
// rebuild notification avoids guessing how long that takes.
function wait_for_workspace_rebuilt(client: LanguageClient, timeout_in_milliseconds: number): Promise<void> {
	return new Promise<void>((resolve, reject) => {
		const disposable = client.onNotification("iris/workspaceRebuilt", () => {
			clearTimeout(timeout);
			disposable.dispose();
			resolve();
		});

		const timeout = setTimeout(() => {
			disposable.dispose();
			reject(new Error("Timed out waiting for iris/workspaceRebuilt."));
		}, timeout_in_milliseconds);
	});
}

suite("File watcher", () => {

	teardown(async () => {
		await delete_created_document();
	});

	test("A created file is picked up and a deleted file is dropped", async () => {
		const anchor_document_uri = get_document_uri("projects/other/diagnostics_parser_error.iris");
		const client = await activate(anchor_document_uri);

		await delete_created_document();

		const request_diagnostics = async (): Promise<language_client.DocumentDiagnosticReport> => {
			return await client.sendRequest(
				language_client.DocumentDiagnosticRequest.method,
				{ textDocument: { uri: created_document_uri.toString() } }
			);
		};

		// The file does not exist yet, so the server knows nothing about it and cannot report on it.
		const before: language_client.DocumentDiagnosticReport = await request_diagnostics();
		assert.equal(before.kind, language_client.DocumentDiagnosticReportKind.Unchanged);

		const created = wait_for_workspace_rebuilt(client, 20000);
		await vscode.workspace.fs.writeFile(created_document_uri, Buffer.from(created_document_contents));
		await created;

		// The rebuild made the new file a core module, so it is now diagnosed. Its contents have a
		// deliberate syntax error, which proves the file was actually parsed rather than merely listed.
		const after: language_client.DocumentDiagnosticReport = await request_diagnostics();
		assert.equal(after.kind, language_client.DocumentDiagnosticReportKind.Full);
		const after_full = after as language_client.RelatedFullDocumentDiagnosticReport;
		assert.ok(after_full.items.length > 0, "Expected the created file to report its syntax error.");

		const deleted = wait_for_workspace_rebuilt(client, 20000);
		await delete_created_document();
		await deleted;

		// Once the file is gone the server must forget it again.
		const after_delete: language_client.DocumentDiagnosticReport = await request_diagnostics();
		assert.equal(after_delete.kind, language_client.DocumentDiagnosticReportKind.Unchanged);
	});
});
