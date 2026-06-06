import * as vscode from 'vscode';
import * as assert from 'assert';
import { get_document_uri, activate } from './helper.js';
import { LanguageClient } from 'vscode-languageclient/node.js';

interface Decoded_semantic_token {
	line: number;
	character: number;
	length: number;
	token_type: string;
	token_modifiers: string[];
}

suite("Should add semantic highlights", () => {

	test.skip("Provides tokens for semantic_highlight_alias.iris", async () => {
		const document_uri = get_document_uri("semantic_highlight_alias.iris");
		await test_semantic_highlight(
			document_uri,
			[
				{ line: 2, character: 0, length: 5, token_type: "keyword", token_modifiers: [] },
				{ line: 2, character: 6, length: 8, token_type: "type", token_modifiers: ["declaration"] },
				{ line: 2, character: 17, length: 5, token_type: "type", token_modifiers: [] },
			],
			to_range(1, 0, 3, 0)
		);
	});

	test.skip("Provides tokens for semantic_highlight_enum.iris", async () => {
		const document_uri = get_document_uri("semantic_highlight_enum.iris");
		await test_semantic_highlight(
			document_uri,
			[
				{ line: 2, character: 0, length: 6, token_type: "keyword", token_modifiers: [] },
				{ line: 2, character: 7, length: 4, token_type: "keyword", token_modifiers: [] },
				{ line: 2, character: 12, length: 7, token_type: "enum", token_modifiers: ["declaration"] },
				{ line: 4, character: 4, length: 7, token_type: "enumMember", token_modifiers: ["declaration"] },
				{ line: 4, character: 14, length: 1, token_type: "number", token_modifiers: [] },
				{ line: 5, character: 4, length: 7, token_type: "enumMember", token_modifiers: ["declaration"] },
				{ line: 5, character: 14, length: 1, token_type: "number", token_modifiers: [] },
			],
			to_range(1, 0, 7, 0)
		);
	});

	test.skip("Provides tokens for semantic_highlight_function.iris", async () => {
		const document_uri = get_document_uri("semantic_highlight_function.iris");
		await test_semantic_highlight(
			document_uri,
			[
				{ line: 2, character: 0, length: 8, token_type: "keyword", token_modifiers: [] },
				{ line: 2, character: 9, length: 3, token_type: "function", token_modifiers: ["declaration"] },
				{ line: 2, character: 13, length: 3, token_type: "parameter", token_modifiers: ["readonly"] },
				{ line: 2, character: 18, length: 5, token_type: "type", token_modifiers: [] },
				{ line: 2, character: 25, length: 3, token_type: "parameter", token_modifiers: ["readonly"] },
				{ line: 2, character: 30, length: 5, token_type: "type", token_modifiers: [] },
				{ line: 2, character: 41, length: 6, token_type: "parameter", token_modifiers: ["readonly"] },
				{ line: 2, character: 49, length: 5, token_type: "type", token_modifiers: [] },
				{ line: 4, character: 4, length: 3, token_type: "keyword", token_modifiers: [] },
				{ line: 4, character: 8, length: 6, token_type: "variable", token_modifiers: ["declaration", "readonly"] },
				{ line: 4, character: 17, length: 3, token_type: "variable", token_modifiers: [] },
				{ line: 4, character: 23, length: 3, token_type: "variable", token_modifiers: [] },
				{ line: 5, character: 4, length: 6, token_type: "keyword", token_modifiers: [] },
				{ line: 5, character: 11, length: 6, token_type: "variable", token_modifiers: [] },
			],
			to_range(1, 0, 7, 0)
		);
	});

	test.skip("Provides tokens for semantic_highlight_module_declaration.iris", async () => {
		const document_uri = get_document_uri("semantic_highlight_module_declaration.iris");
		await test_semantic_highlight(
			document_uri,
			[
				{ line: 0, character: 0, length: 6, token_type: "keyword", token_modifiers: [] },
				{ line: 0, character: 7, length: 9, token_type: "namespace", token_modifiers: ["declaration"] },
			]
		);
	});

	test.skip("Provides tokens for semantic_highlight_global_variable.iris", async () => {
		const document_uri = get_document_uri("semantic_highlight_global_variable.iris");
		await test_semantic_highlight(
			document_uri,
			[
				{ line: 2, character: 0, length: 3, token_type: "keyword", token_modifiers: [], },
				{ line: 2, character: 4, length: 15, token_type: "variable", token_modifiers: ["declaration", "readonly"], },
				{ line: 2, character: 22, length: 1, token_type: "number", token_modifiers: [], },
				{ line: 3, character: 0, length: 7, token_type: "keyword", token_modifiers: [], },
				{ line: 3, character: 8, length: 15, token_type: "variable", token_modifiers: ["declaration"], },
				{ line: 3, character: 26, length: 6, token_type: "number", token_modifiers: [], },
			],
			to_range(1, 0, 6, 0)
		);
	});

	test.skip("Provides tokens for semantic_highlight_struct.iris", async () => {
		const document_uri = get_document_uri("semantic_highlight_struct.iris");
		await test_semantic_highlight(
			document_uri,
			[
				{ line: 2, character: 0, length: 6, token_type: "keyword", token_modifiers: [] },
				{ line: 2, character: 7, length: 6, token_type: "keyword", token_modifiers: [] },
				{ line: 2, character: 14, length: 9, token_type: "struct", token_modifiers: ["declaration"] },
				{ line: 4, character: 4, length: 8, token_type: "property", token_modifiers: ["declaration"] },
				{ line: 4, character: 14, length: 5, token_type: "type", token_modifiers: [] },
				{ line: 4, character: 22, length: 1, token_type: "number", token_modifiers: [] },
			],
			to_range(1, 0, 6, 0)
		);
	});

	test.skip("Provides tokens for semantic_highlight_union.iris", async () => {
		const document_uri = get_document_uri("semantic_highlight_union.iris");
		await test_semantic_highlight(
			document_uri,
			[
				{ line: 2, character: 0, length: 6, token_type: "keyword", token_modifiers: [] },
				{ line: 2, character: 7, length: 5, token_type: "keyword", token_modifiers: [] },
				{ line: 2, character: 13, length: 8, token_type: "type", token_modifiers: ["declaration"] },
				{ line: 4, character: 4, length: 8, token_type: "property", token_modifiers: ["declaration"] },
				{ line: 4, character: 14, length: 5, token_type: "type", token_modifiers: [] },
				{ line: 5, character: 4, length: 8, token_type: "property", token_modifiers: ["declaration"] },
				{ line: 5, character: 14, length: 7, token_type: "type", token_modifiers: [] },
			],
			to_range(1, 0, 7, 0)
		);
	});
});

function to_range(start_line: number, start_character: number, end_line: number, end_character: number): vscode.Range {
	const start = new vscode.Position(start_line, start_character);
	const end = new vscode.Position(end_line, end_character);
	return new vscode.Range(start, end);
}

async function get_semantic_tokens(
	client: LanguageClient,
	document_uri: vscode.Uri,
	range?: vscode.Range
): Promise<Decoded_semantic_token[]> {

	if (client.initializeResult == undefined || client.initializeResult.capabilities.semanticTokensProvider == undefined)
		return [];

	const legend = client.initializeResult.capabilities.semanticTokensProvider.legend;

	if (range !== undefined) {
		const actual_semantic_tokens = (await vscode.commands.executeCommand(
			"vscode.provideDocumentRangeSemanticTokens",
			document_uri,
			range
		)) as vscode.SemanticTokens;

		return decode_semantic_tokens(actual_semantic_tokens.data, legend);
	}
	else {
		const actual_semantic_tokens = (await vscode.commands.executeCommand(
			"vscode.provideDocumentSemanticTokens",
			document_uri
		)) as vscode.SemanticTokens;

		return decode_semantic_tokens(actual_semantic_tokens.data, legend);
	}
}

async function test_semantic_highlight(
	document_uri: vscode.Uri,
	expected_decoded_semantic_tokens: Decoded_semantic_token[],
	range?: vscode.Range
) {
	const client = await activate(document_uri);

	const actual_decoded_semantic_tokens = await get_semantic_tokens(client, document_uri, range);

	if (actual_decoded_semantic_tokens.length !== expected_decoded_semantic_tokens.length) {
		assert.deepEqual(actual_decoded_semantic_tokens, expected_decoded_semantic_tokens);
	}

	assert.ok(actual_decoded_semantic_tokens.length === expected_decoded_semantic_tokens.length);
	expected_decoded_semantic_tokens.forEach((expected_token, index) => {
		const actual_token = actual_decoded_semantic_tokens[index];
		assert.deepEqual(actual_token, expected_token);
	});
}

function decode_semantic_tokens(data: Uint32Array, legend: vscode.SemanticTokensLegend): Decoded_semantic_token[] {
	const tokens: Decoded_semantic_token[] = [];
	let line = 0;
	let character = 0;

	for (let i = 0; i < data.length; i += 5) {
		const delta_line = data[i];
		const delta_start = data[i + 1];
		const length = data[i + 2];
		const token_type_index = data[i + 3];
		const token_modifier_set = data[i + 4];

		line += delta_line;
		character = delta_line === 0 ? character + delta_start : delta_start;

		const token_type = legend.tokenTypes[token_type_index];
		const token_modifiers = decode_modifiers(token_modifier_set, legend.tokenModifiers);

		tokens.push({
			line,
			character,
			length,
			token_type,
			token_modifiers
		});
	}

	return tokens;
}

function decode_modifiers(token_modifier_set: number, token_modifiers: string[]): string[] {
	const modifiers = [];
	for (let i = 0; i < token_modifiers.length; i++) {
		if (token_modifier_set & (1 << i)!) {
			modifiers.push(token_modifiers[i]);
		}
	}
	return modifiers;
}
