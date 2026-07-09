import * as language_client from 'vscode-languageclient';
import * as assert from 'assert';
import * as vscode from 'vscode';
import { get_document_uri, activate } from './helper.js';

suite("Recompute diagnostics command", () => {

	test("Forces a full diagnostics report even when nothing changed", async () => {
		const document_uri = get_document_uri("projects/other/diagnostics_parser_error.iris");
		const client = await activate(document_uri);

		const find_item = (report: language_client.WorkspaceDiagnosticReport) => {
			return report.items.find(item => {
				const item_uri = vscode.Uri.parse(item.uri, true);
				return item_uri.fsPath == document_uri.fsPath;
			});
		};

		// Initial pull assigns result ids to every document.
		const initial: language_client.WorkspaceDiagnosticReport = await client.sendRequest(
			language_client.WorkspaceDiagnosticRequest.method,
			{ identifier: "iris", previousResultIds: [] }
		);

		const previous_result_ids = initial.items
			.filter(item => item.resultId != undefined)
			.map(item => ({ uri: item.uri, value: item.resultId as string }));

		// Nothing changed, so re-pulling with the previous ids reports the document as unchanged.
		const unchanged: language_client.WorkspaceDiagnosticReport = await client.sendRequest(
			language_client.WorkspaceDiagnosticRequest.method,
			{ identifier: "iris", previousResultIds: previous_result_ids }
		);
		const unchanged_item = find_item(unchanged);
		assert.notEqual(unchanged_item, undefined);
		assert.equal(unchanged_item!.kind, language_client.DocumentDiagnosticReportKind.Unchanged);

		// The command invalidates all diagnostics on the server.
		await vscode.commands.executeCommand("iris.recomputeDiagnostics");

		// The same pull must now return a full (recomputed) report for the document.
		const recomputed: language_client.WorkspaceDiagnosticReport = await client.sendRequest(
			language_client.WorkspaceDiagnosticRequest.method,
			{ identifier: "iris", previousResultIds: previous_result_ids }
		);
		const recomputed_item = find_item(recomputed);
		assert.notEqual(recomputed_item, undefined);
		assert.equal(recomputed_item!.kind, language_client.DocumentDiagnosticReportKind.Full);
	});
});
