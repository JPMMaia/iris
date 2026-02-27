import * as vscode from 'vscode';
import * as assert from 'assert';
import { get_document_uri, activate } from './helper.js';

suite("Should do completion", () => {
	test.skip("Completes 'module' at the beginning of file", async () => {
		const document_uri = get_document_uri('projects/other/completion_00.hltxt');
		await test_completion(document_uri, new vscode.Position(0, 0), {
			items: [
				{ label: 'module', kind: vscode.CompletionItemKind.Keyword }
			]
		});
	});

	test.skip("Completes after module declaration", async () => {
		const document_uri = get_document_uri('projects/other/completion_01.hltxt');
		await test_completion(document_uri, new vscode.Position(2, 0), {
			items: [
				{ label: "enum", kind: vscode.CompletionItemKind.Keyword },
				{ label: "export", kind: vscode.CompletionItemKind.Keyword },
				{ label: "function", kind: vscode.CompletionItemKind.Keyword },
				{ label: "function_constructor", kind: vscode.CompletionItemKind.Keyword },
				{ label: "import", kind: vscode.CompletionItemKind.Keyword },
				{ label: "mutable", kind: vscode.CompletionItemKind.Keyword },
				{ label: "struct", kind: vscode.CompletionItemKind.Keyword },
				{ label: "type_constructor", kind: vscode.CompletionItemKind.Keyword },
				{ label: "union", kind: vscode.CompletionItemKind.Keyword },
				{ label: "using", kind: vscode.CompletionItemKind.Keyword },
				{ label: "var", kind: vscode.CompletionItemKind.Keyword },
			]
		});
	});

	test("Completes return statement", async () => {
		const document_uri = get_document_uri('projects/other/completion_02.hltxt');
		await test_completion(document_uri, new vscode.Position(9, 11), {
			items: [
				{ label: "add", kind: vscode.CompletionItemKind.Function },
				{ label: "add_implementation", kind: vscode.CompletionItemKind.Function },
				{ label: "lhs", kind: vscode.CompletionItemKind.Variable },
				{ label: "rhs", kind: vscode.CompletionItemKind.Variable },
			]
		});
	});

	test("Completes function parameter type", async () => {
		const document_uri = get_document_uri('projects/other/completion_03.hltxt');
		await test_completion(document_uri, new vscode.Position(12, 18), {
			items: [
				{ label: "Any_type", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Bool", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Byte", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_bool", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_char", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_int", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_long", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_longdouble", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_longlong", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_schar", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_short", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_uchar", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_uint", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_ulong", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_ulonglong", kind: vscode.CompletionItemKind.Keyword },
				{ label: "C_ushort", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Float16", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Float32", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Float64", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Int16", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Int32", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Int64", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Int8", kind: vscode.CompletionItemKind.Keyword },
				{ label: "My_alias", kind: vscode.CompletionItemKind.TypeParameter },
				{ label: "My_enum", kind: vscode.CompletionItemKind.Enum },
				{ label: "My_struct", kind: vscode.CompletionItemKind.Struct },
				{ label: "String", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Uint16", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Uint32", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Uint64", kind: vscode.CompletionItemKind.Keyword },
				{ label: "Uint8", kind: vscode.CompletionItemKind.Keyword },
			]
		});
	});

	test("Completes import module name 1", async () => {
		const document_uri = get_document_uri('projects/project_0/import_completion_0.hltxt');
		await test_completion(document_uri, new vscode.Position(2, 7), {
			items: [
				{ label: "c.complex", kind: vscode.CompletionItemKind.Module },
				{ label: "c.entry", kind: vscode.CompletionItemKind.Module },
				{ label: "project_0.import_completion_1", kind: vscode.CompletionItemKind.Module },
				{ label: "project_0.main", kind: vscode.CompletionItemKind.Module },
			]
		});
	});

	test("Completes import module name 2", async () => {
		const document_uri = get_document_uri('projects/project_0/import_completion_1.hltxt');
		await test_completion(document_uri, new vscode.Position(2, 7), {
			items: [
				{ label: "c.complex", kind: vscode.CompletionItemKind.Module },
				{ label: "c.entry", kind: vscode.CompletionItemKind.Module },
				{ label: "project_0.import_completion_0", kind: vscode.CompletionItemKind.Module },
				{ label: "project_0.main", kind: vscode.CompletionItemKind.Module },
			]
		});
	});

	test.skip("Completes import module name 3", async () => {
		const document_uri = get_document_uri('projects/project_0/main.hltxt');
		await test_completion(document_uri, new vscode.Position(2, 10), {
			items: [
				{ label: "c.entry", kind: vscode.CompletionItemKind.Module },
				{ label: "project_0.import_completion_0", kind: vscode.CompletionItemKind.Module },
				{ label: "project_0.import_completion_1", kind: vscode.CompletionItemKind.Module },
			]
		});
	});

	test("Completes import module alias when expecting type 0", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_0.hltxt');
		await test_completion(document_uri, new vscode.Position(4, 27), {
			items: [
				{ label: "complex", kind: vscode.CompletionItemKind.Module },
			]
		},
			true);
	});

	test("Completes import module alias when expecting type 1", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_1.hltxt');
		await test_completion(document_uri, new vscode.Position(4, 35), {
			items: [
				{ label: "Complex", kind: vscode.CompletionItemKind.Struct },
				{ label: "Number", kind: vscode.CompletionItemKind.Struct },
				{ label: "Precision", kind: vscode.CompletionItemKind.Enum },
				{ label: "Precision_t", kind: vscode.CompletionItemKind.TypeParameter },
			]
		});
	});

	test("Completes import module alias when expecting type 2", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_2.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 15), {
			items: [
				{ label: "complex", kind: vscode.CompletionItemKind.Module },
			]
		},
			true);
	});

	test("Completes import module alias when expecting type 3", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_3.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 23), {
			items: [
				{ label: "Complex", kind: vscode.CompletionItemKind.Struct },
				{ label: "Number", kind: vscode.CompletionItemKind.Struct },
				{ label: "Precision", kind: vscode.CompletionItemKind.Enum },
				{ label: "Precision_t", kind: vscode.CompletionItemKind.TypeParameter },
			]
		});
	});

	test("Completes import module alias when expecting type 4", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_4.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 11), {
			items: [
				{ label: "complex", kind: vscode.CompletionItemKind.Module },
			]
		},
			true);
	});

	test("Completes import module alias when expecting type 5", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_5.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 19), {
			items: [
				{ label: "Complex", kind: vscode.CompletionItemKind.Struct },
				{ label: "Number", kind: vscode.CompletionItemKind.Struct },
				{ label: "Precision", kind: vscode.CompletionItemKind.Enum },
				{ label: "Precision_t", kind: vscode.CompletionItemKind.TypeParameter },
			]
		});
	});

	test("Completes import module alias when expecting type 6", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_6.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 11), {
			items: [
				{ label: "complex", kind: vscode.CompletionItemKind.Module },
			]
		},
			true);
	});

	test("Completes import module alias when expecting type 7", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_7.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 19), {
			items: [
				{ label: "Complex", kind: vscode.CompletionItemKind.Struct },
				{ label: "Number", kind: vscode.CompletionItemKind.Struct },
				{ label: "Precision", kind: vscode.CompletionItemKind.Enum },
				{ label: "Precision_t", kind: vscode.CompletionItemKind.TypeParameter },
			]
		});
	});

	test("Completes import module alias when expecting type 8", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_8.hltxt');
		await test_completion(document_uri, new vscode.Position(4, 19), {
			items: [
				{ label: "complex", kind: vscode.CompletionItemKind.Module },
			]
		},
			true);
	});

	test("Completes import module alias when expecting type 9", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_9.hltxt');
		await test_completion(document_uri, new vscode.Position(4, 27), {
			items: [
				{ label: "Complex", kind: vscode.CompletionItemKind.Struct },
				{ label: "Number", kind: vscode.CompletionItemKind.Struct },
				{ label: "Precision", kind: vscode.CompletionItemKind.Enum },
				{ label: "Precision_t", kind: vscode.CompletionItemKind.TypeParameter },
			]
		});
	});

	test.skip("Completes import module type when using module alias and expecting a type", async () => {
		const document_uri = get_document_uri('projects/project_0/main.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 19), {
			items: [
				{ label: "Complex", kind: vscode.CompletionItemKind.Struct },
				{ label: "Number", kind: vscode.CompletionItemKind.Struct },
				{ label: "Precision", kind: vscode.CompletionItemKind.Enum },
				{ label: "Precision_t", kind: vscode.CompletionItemKind.TypeParameter },
			]
		});
	});

	test("Completes import module alias when expecting a value", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_10.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 16), {
			items: [
				{ label: "complex", kind: vscode.CompletionItemKind.Module },
			]
		});
	});

	test("Completes import module function when using module alias and expecting a value", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_11.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 24), {
			items: [
				{ label: "add", kind: vscode.CompletionItemKind.Function },
				{ label: "PI", kind: vscode.CompletionItemKind.Constant },
			]
		});
	});

	test("Completes enum values", async () => {
		const document_uri = get_document_uri('projects/other/completion_enum_0.hltxt');
		await test_completion(document_uri, new vscode.Position(11, 26), {
			items: [
				{ label: "High", kind: vscode.CompletionItemKind.EnumMember },
				{ label: "Low", kind: vscode.CompletionItemKind.EnumMember },
				{ label: "Medium", kind: vscode.CompletionItemKind.EnumMember },
			]
		});
	});

	test("Completes enum values through alias", async () => {
		const document_uri = get_document_uri('projects/other/completion_enum_1.hltxt');
		await test_completion(document_uri, new vscode.Position(13, 28), {
			items: [
				{ label: "High", kind: vscode.CompletionItemKind.EnumMember },
				{ label: "Low", kind: vscode.CompletionItemKind.EnumMember },
				{ label: "Medium", kind: vscode.CompletionItemKind.EnumMember },
			]
		});
	});

	test("Completes import module enum values", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_enum_0.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 34), {
			items: [
				{ label: "High", kind: vscode.CompletionItemKind.EnumMember },
				{ label: "Low", kind: vscode.CompletionItemKind.EnumMember },
				{ label: "Medium", kind: vscode.CompletionItemKind.EnumMember },
			]
		});
	});

	test("Completes import module enum values through alias", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_enum_1.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 36), {
			items: [
				{ label: "High", kind: vscode.CompletionItemKind.EnumMember },
				{ label: "Low", kind: vscode.CompletionItemKind.EnumMember },
				{ label: "Medium", kind: vscode.CompletionItemKind.EnumMember },
			]
		});
	});

	test("Completes global variables", async () => {
		const document_uri = get_document_uri('projects/other/completion_global_variable_0.hltxt');
		await test_completion(document_uri, new vscode.Position(7, 16), {
			items: [
				{ label: "my_global_constant", kind: vscode.CompletionItemKind.Constant },
				{ label: "my_global_variable", kind: vscode.CompletionItemKind.Variable },
			]
		}, true);
	});

	test("Completes instantiate expression members", async () => {
		const document_uri = get_document_uri('projects/other/completion_instantiate_0.hltxt');
		await test_completion(document_uri, new vscode.Position(10, 26), {
			items: [
				{ label: "imaginary", kind: vscode.CompletionItemKind.Field },
				{ label: "real", kind: vscode.CompletionItemKind.Field },
			]
		});
	});

	test("Completes value inside instantiate expression", async () => {
		const document_uri = get_document_uri('projects/other/completion_instantiate_1.hltxt');
		await test_completion(document_uri, new vscode.Position(11, 14), {
			items: [
				{ label: "run", kind: vscode.CompletionItemKind.Function },
			]
		});
	});

	test("Completes import value inside instantiate expression", async () => {
		const document_uri = get_document_uri('projects/other/completion_instantiate_2.hltxt');
		await test_completion(document_uri, new vscode.Position(7, 22), {
			items: [
				{ label: "add", kind: vscode.CompletionItemKind.Function },
				{ label: "PI", kind: vscode.CompletionItemKind.Constant },
			]
		});
	});

	test("Completes struct members", async () => {
		const document_uri = get_document_uri('projects/other/completion_struct_0.hltxt');
		await test_completion(document_uri, new vscode.Position(11, 10), {
			items: [
				{ label: "imaginary", kind: vscode.CompletionItemKind.Field },
				{ label: "real", kind: vscode.CompletionItemKind.Field },
			]
		});
	});

	test("Completes struct members in a block", async () => {
		const document_uri = get_document_uri('projects/other/completion_struct_2.hltxt');
		await test_completion(document_uri, new vscode.Position(12, 14), {
			items: [
				{ label: "imaginary", kind: vscode.CompletionItemKind.Field },
				{ label: "real", kind: vscode.CompletionItemKind.Field },
			]
		});
	});

	test("Completes import module struct members", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_struct_0.hltxt');
		await test_completion(document_uri, new vscode.Position(7, 10), {
			items: [
				{ label: "imaginary", kind: vscode.CompletionItemKind.Field },
				{ label: "real", kind: vscode.CompletionItemKind.Field },
			]
		});
	});
	
	test("Completes struct members through pointers", async () => {
		const document_uri = get_document_uri('projects/other/completion_struct_1.hltxt');
		await test_completion(document_uri, new vscode.Position(10, 11), {
			items: [
				{ label: "imaginary", kind: vscode.CompletionItemKind.Field },
				{ label: "real", kind: vscode.CompletionItemKind.Field },
			]
		});
	});

	test("Completes union members", async () => {
		const document_uri = get_document_uri('projects/other/completion_union_0.hltxt');
		await test_completion(document_uri, new vscode.Position(14, 10), {
			items: [
				{ label: "float32", kind: vscode.CompletionItemKind.Field },
				{ label: "int16", kind: vscode.CompletionItemKind.Field },
				{ label: "int32", kind: vscode.CompletionItemKind.Field },
			]
		});
	});

	test("Completes import module union members", async () => {
		const document_uri = get_document_uri('projects/project_1/completion_union_0.hltxt');
		await test_completion(document_uri, new vscode.Position(9, 10), {
			items: [
				{ label: "float32", kind: vscode.CompletionItemKind.Field },
				{ label: "int16", kind: vscode.CompletionItemKind.Field },
				{ label: "int32", kind: vscode.CompletionItemKind.Field },
			]
		});
	});

	test("Completes a module alias which uses prefix transformations 0", async () => {
		const document_uri = get_document_uri('projects/with_prefix_use/main.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 12), {
			items: [
				{ label: "add", kind: vscode.CompletionItemKind.Function },
			]
		});
	});

	test("Completes a module alias which uses prefix transformations 1", async () => {
		const document_uri = get_document_uri('projects/with_prefix_use/completion_0.hltxt');
		await test_completion(document_uri, new vscode.Position(6, 19), {
			items: [
				{ label: "Complex", kind: vscode.CompletionItemKind.Struct },
			]
		});
	});

	test("Completes with local variables middle on function", async () => {
		const document_uri = get_document_uri('projects/other/completion_local_variables_0.hltxt');
		await test_completion(document_uri, new vscode.Position(5, 0), {
			items: [
				{ label: "a", kind: vscode.CompletionItemKind.Variable },
				{ label: "run", kind: vscode.CompletionItemKind.Function },
			]
		});
	});

	test("Completes with local variables end of function", async () => {
		const document_uri = get_document_uri('projects/other/completion_local_variables_0.hltxt');
		await test_completion(document_uri, new vscode.Position(7, 0), {
			items: [
				{ label: "a", kind: vscode.CompletionItemKind.Variable },
				{ label: "b", kind: vscode.CompletionItemKind.Variable },
				{ label: "run", kind: vscode.CompletionItemKind.Function },
			]
		});
	});

	test("Completes inside if expression", async () => {
		const document_uri = get_document_uri('projects/other/completion_if_expression_0.hltxt');
		await test_completion(document_uri, new vscode.Position(7, 0), {
			items: [
				{ label: "a", kind: vscode.CompletionItemKind.Variable },
				{ label: "run", kind: vscode.CompletionItemKind.Function },
			]
		});
	});

	test("Completes array slice 0", async () => {
		const document_uri = get_document_uri('projects/other/completion_array_slice_0.hltxt');
		await test_completion(document_uri, new vscode.Position(4, 10), {
			items: [
				{ label: "data", kind: vscode.CompletionItemKind.Field },
				{ label: "length", kind: vscode.CompletionItemKind.Field },
			]
		});
	});
});

async function test_completion(
	document_uri: vscode.Uri,
	position: vscode.Position,
	expected_completion_list: vscode.CompletionList,
	match_only_expected?: boolean
) {
	await activate(document_uri);

	// Executing the command `vscode.executeCompletionItemProvider` to simulate triggering completion
	const actual_completion_list = (await vscode.commands.executeCommand(
		'vscode.executeCompletionItemProvider',
		document_uri,
		position
	)) as vscode.CompletionList;

	assert.ok(actual_completion_list.items.length >= expected_completion_list.items.length);
	expected_completion_list.items.forEach((expected_item, i) => {
		if (match_only_expected !== undefined && match_only_expected) {
			const actual_item = actual_completion_list.items.find(value => value.label === expected_item.label);
			assert.notEqual(actual_item, undefined);
			if (actual_item !== undefined) {
				assert.equal(actual_item.label, expected_item.label);
				assert.equal(actual_item.kind, expected_item.kind);
			}
		}
		else {
			const actual_item = actual_completion_list.items[i];
			assert.equal(actual_item.label, expected_item.label);
			assert.equal(actual_item.kind, expected_item.kind);
		}
	});
}
