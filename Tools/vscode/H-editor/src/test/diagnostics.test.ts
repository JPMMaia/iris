import * as language_client from 'vscode-languageclient';
import * as assert from 'assert';
import * as vscode from 'vscode';
import { get_document_uri, activate } from './helper.js';

suite("Should get diagnostics", () => {

	test("Diagnoses parsing error", async () => {
		const document_uri = get_document_uri("projects/other/diagnostics_parser_error.hltxt");
		await test_diagnostics(document_uri, [
			{ message: "Unexpected token.", range: to_range(2, 0, 5, 1), severity: vscode.DiagnosticSeverity.Error, source: "Parser" },
		]);
	});

	test.skip("Diagnoses incorrect float suffix", async () => {
		const document_uri = get_document_uri("projects/other/diagnostics_float_suffix.hltxt");
		await test_diagnostics(document_uri, [
			{ message: "Did not expect 'f' as number suffix. Did you mean 'f16', 'f32' or 'f64'?", range: to_range(4, 12, 4, 16), severity: vscode.DiagnosticSeverity.Error, source: "Compiler" },
		]);
	});

	test("Diagnoses missing members in an explicit instantiate expression", async () => {
		const document_uri = get_document_uri("projects/other/diagnostics_missing_explicit_instantiate_members.hltxt");
		await test_diagnostics(document_uri, [
			{ message: "'My_struct.a' is not set. Explicit instantiate expression requires all members to be set.", range: to_range(10, 30, 10, 41), severity: vscode.DiagnosticSeverity.Error, source: "Compiler" },
			{ message: "'My_struct.b' is not set. Explicit instantiate expression requires all members to be set.", range: to_range(10, 30, 10, 41), severity: vscode.DiagnosticSeverity.Error, source: "Compiler" },
		]);
	});
});

function to_range(sLine: number, sChar: number, eLine: number, eChar: number) {
	const start = new vscode.Position(sLine, sChar);
	const end = new vscode.Position(eLine, eChar);
	return new vscode.Range(start, end);
}

function find_document_diagnostics(
	report: language_client.WorkspaceDiagnosticReport,
	document_uri: vscode.Uri
): language_client.Diagnostic[]
{
	const found_item = report.items.find(
		item => {
			const item_uri = vscode.Uri.parse(item.uri, true);
			return item_uri.fsPath == document_uri.fsPath;
		}
	);
	assert.notEqual(found_item, undefined);

	if (found_item != undefined && found_item.kind === language_client.DocumentDiagnosticReportKind.Full)
	{
		const full_report_item = found_item as language_client.FullDocumentDiagnosticReport;
		return full_report_item.items;
	}
	else
	{
		return [];
	}
}

function to_vscode_range(input: language_client.Range): vscode.Range {
	return new vscode.Range(
		input.start.line,
		input.start.character,
		input.end.line,
		input.end.character
	);
}

function to_vscode_diagnostic_severity(input: language_client.DiagnosticSeverity | undefined): vscode.DiagnosticSeverity | undefined {
	if (input == undefined)
		return undefined;

	switch (input)
	{
	case language_client.DiagnosticSeverity.Error:
		return vscode.DiagnosticSeverity.Error;
	case language_client.DiagnosticSeverity.Warning:
		return vscode.DiagnosticSeverity.Warning;
	case language_client.DiagnosticSeverity.Information:
		return vscode.DiagnosticSeverity.Information;
	case language_client.DiagnosticSeverity.Hint:
		return vscode.DiagnosticSeverity.Hint;
	}
}

async function test_diagnostics(document_uri: vscode.Uri, expected_diagnostics: vscode.Diagnostic[]) {
	const client = await activate(document_uri);

	const parameters: language_client.WorkspaceDiagnosticParams = {
		identifier: "hlang",
		previousResultIds: []
	};

	const report: language_client.WorkspaceDiagnosticReport = await client.sendRequest(language_client.WorkspaceDiagnosticRequest.method, parameters);

	const actual_diagnostics = find_document_diagnostics(report, document_uri);

	assert.equal(actual_diagnostics.length, expected_diagnostics.length);

	expected_diagnostics.forEach((expected_diagnostic, i) => {
		const actual_diagnostic = actual_diagnostics[i];
		assert.equal(actual_diagnostic.message, expected_diagnostic.message);
		assert.deepEqual(to_vscode_range(actual_diagnostic.range), expected_diagnostic.range);
		assert.equal(to_vscode_diagnostic_severity(actual_diagnostic.severity), expected_diagnostic.severity);
		assert.equal(actual_diagnostic.source, expected_diagnostic.source);
	});
}
