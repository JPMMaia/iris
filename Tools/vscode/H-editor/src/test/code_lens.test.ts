import * as vscode from 'vscode';
import * as assert from 'assert';
import { get_document_uri, activate } from './helper.js';

suite("Should add code lens describing the layout of structs", () => {
	test.skip("Add code lens 0", async () => {
		const document_uri = get_document_uri('code_lens_0.hltxt');
		await test_code_lens(document_uri, [
			new vscode.CodeLens(to_range(2, 7, 2, 16), { title: "Size: 24 bytes | Alignment: 8 bytes", command: "", arguments: undefined }),
			new vscode.CodeLens(to_range(4, 4, 4, 6), { title: "Offset: 0 bytes | Size: 1 bytes | Alignment: 1 bytes", command: "", arguments: undefined }),
			new vscode.CodeLens(to_range(5, 4, 5, 6), { title: "Offset: 2 bytes | Size: 2 bytes | Alignment: 2 bytes", command: "", arguments: undefined }),
			new vscode.CodeLens(to_range(6, 4, 6, 6), { title: "Offset: 4 bytes | Size: 1 bytes | Alignment: 1 bytes", command: "", arguments: undefined }),
			new vscode.CodeLens(to_range(7, 4, 7, 6), { title: "Offset: 8 bytes | Size: 4 bytes | Alignment: 4 bytes", command: "", arguments: undefined }),
			new vscode.CodeLens(to_range(8, 4, 8, 6), { title: "Offset: 12 bytes | Size: 1 bytes | Alignment: 1 bytes", command: "", arguments: undefined }),
			new vscode.CodeLens(to_range(9, 4, 9, 6), { title: "Offset: 16 bytes | Size: 8 bytes | Alignment: 8 bytes", command: "", arguments: undefined }),
		]);
	});
});

function to_range(start_line: number, start_character: number, end_line: number, end_character: number): vscode.Range {
	const start = new vscode.Position(start_line, start_character);
	const end = new vscode.Position(end_line, end_character);
	return new vscode.Range(start, end);
}

async function test_code_lens(
	document_uri: vscode.Uri,
	expected_code_lens_list: vscode.CodeLens[]
) {
	await activate(document_uri);

	const actual_code_lens_list = (await vscode.commands.executeCommand(
		'vscode.executeCodeLensProvider',
		document_uri,
		expected_code_lens_list.length
	)) as vscode.CodeLens[];

	assert.ok(actual_code_lens_list.length >= expected_code_lens_list.length);
	expected_code_lens_list.forEach((expected_code_lens, i) => {
		const actual_code_lens = actual_code_lens_list[i];
		assert.deepEqual(actual_code_lens, expected_code_lens);
	});
}
