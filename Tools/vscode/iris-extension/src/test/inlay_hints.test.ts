import * as vscode from 'vscode';
import * as assert from 'assert';
import { get_document_uri, activate } from './helper.js';

suite("Should get inlay hints", () => {

	test("Creates hint for variable declaration expression", async () => {
		const document_uri = get_document_uri("projects/other/inlay_hints_0.iris");
		await test_inlay_hints(document_uri, to_range(4, 4, 4, 14), [
			{
				label: [
					{
						value: ": ",
						tooltip: undefined
					},
					{
						value: "Int32",
						tooltip: undefined,
					}
				],
				position: new vscode.Position(4, 9)
			},
		]);
	});

	test("Creates hint for variable declaration of a struct", async () => {
		const document_uri = get_document_uri("projects/other/inlay_hints_1.iris");

		await test_inlay_hints(document_uri, to_range(17, 4, 17, 35), [
			{
				label: [
					{
						value: ": ",
						tooltip: undefined
					},
					{
						value: "Complex",
						tooltip: undefined,
						location: {
							uri: document_uri,
							range: to_range(4, 7, 4, 7)
						}
					}
				],
				position: new vscode.Position(17, 15)
			},
		]);
	});

	test("Creates hint for variable declaration of a struct of a different module", async () => {
		const document_uri = get_document_uri("projects/project_1/inlay_hints_0.iris");
		const imported_module_document_uri = get_document_uri("projects/complex/complex.h");

		const module_tooltip = new vscode.MarkdownString(
			[
				'```hlang',
				'module c.complex',
				'```'
			].join("\n")
		);

		await test_inlay_hints(document_uri, to_range(8, 4, 8, 9), [
			{
				label: [
					{
						value: ": ",
						tooltip: undefined
					},
					{
						value: "complex.Complex",
						tooltip: undefined,
						location: {
							uri: imported_module_document_uri,
							range: to_range(0, 15, 0, 15)
						}
					}
				],
				position: new vscode.Position(8, 9)
			},
		]);
	});

	test.skip("Creates hint for function input parameter", async () => {
		const document_uri = get_document_uri("projects/other/inlay_hints_2.iris");

		const lhs_tooltip = new vscode.MarkdownString(
			[
				'```hlang',
				'lhs: Int32',
				'```'
			].join("\n")
		);

		const rhs_tooltip = new vscode.MarkdownString(
			[
				'```hlang',
				'rhs: Int32',
				'```'
			].join("\n")
		);

		await test_inlay_hints(document_uri, to_range(9, 4, 9, 27), [
			{
				label: [
					{
						value: ": ",
						tooltip: undefined
					},
					{
						value: "Int32",
						tooltip: "Built-in type: Int32"
					}
				],
				position: new vscode.Position(9, 14)
			},
			{
				label: [
					{
						value: "lhs",
						tooltip: lhs_tooltip,
						location: {
							uri: document_uri,
							range: to_range(2, 13, 2, 16)
						}
					},
					{
						value: ": ",
						tooltip: undefined
					}
				],
				position: new vscode.Position(9, 21)
			},
			{
				label: [
					{
						value: "rhs",
						tooltip: rhs_tooltip,
						location: {
							uri: document_uri,
							range: to_range(2, 25, 2, 28)
						}
					},
					{
						value: ": ",
						tooltip: undefined
					}
				],
				position: new vscode.Position(9, 24)
			},
		]);
	});
});

function to_range(start_line: number, start_character: number, end_line: number, end_character: number): vscode.Range {
	const start = new vscode.Position(start_line, start_character);
	const end = new vscode.Position(end_line, end_character);
	return new vscode.Range(start, end);
}

async function test_inlay_hints(document_uri: vscode.Uri, range: vscode.Range, expected_inlay_hints: vscode.InlayHint[]) {
	await activate(document_uri);

	const actual_inlay_hints = (await vscode.commands.executeCommand(
		'vscode.executeInlayHintProvider',
		document_uri,
		range
	)) as vscode.InlayHint[];

	assert.equal(actual_inlay_hints.length, expected_inlay_hints.length);

	expected_inlay_hints.forEach((expected_inlay_hint, i) => {
		const actual_inlay_hint = actual_inlay_hints[i];

		const actual_label_parts = actual_inlay_hint.label as vscode.InlayHintLabelPart[];
		const expected_label_parts = expected_inlay_hint.label as vscode.InlayHintLabelPart[];

		assert.equal(actual_label_parts.length, expected_label_parts.length);
		for (let index = 0; index < expected_label_parts.length; ++index) {
			const actual_label_part = actual_label_parts[index];
			const expected_label_part = expected_label_parts[index];
			assert.equal(actual_label_part.value, expected_label_part.value);

			if (expected_label_part.tooltip === undefined) {
				assert.equal(actual_label_part.tooltip, undefined);
			}
			else {
				assert.notEqual(actual_label_part.tooltip, undefined);
				const actual_tooltip = actual_label_part.tooltip as vscode.MarkdownString;
				const expected_tooltip = expected_label_part.tooltip as vscode.MarkdownString;

				assert.equal(actual_tooltip.value, expected_tooltip.value);
			}


			if (expected_label_part.location === undefined) {
				assert.equal(actual_label_part.location, undefined);
			}
			else if (actual_label_part.location != undefined) {
				assert.equal(actual_label_part.location.uri.fsPath, expected_label_part.location.uri.fsPath);
				assert.deepEqual(actual_label_part.location.range, expected_label_part.location.range);
			}
		}

		assert.deepEqual(actual_inlay_hint.position, expected_inlay_hint.position);
	});
}
