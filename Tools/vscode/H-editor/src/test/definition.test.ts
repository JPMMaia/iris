import * as vscode from 'vscode';
import * as assert from 'assert';
import { get_document_uri, activate } from './helper.js';

suite("Should get definition location of structs", () => {

    test.skip("Gets definition location of itself", async () => {
        const document_uri = get_document_uri("projects/other/definition_struct_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(2, 9), [
            new vscode.Location(get_document_uri("projects/other/definition_struct_0.hltxt"), to_range(2, 7, 2, 16))
        ]);
    });

    test("Gets definition location of struct as a function input parameter type", async () => {
        const document_uri = get_document_uri("projects/other/definition_struct_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(7, 22), [
            new vscode.Location(get_document_uri("projects/other/definition_struct_0.hltxt"), to_range(2, 7, 2, 16))
        ]);
    });

    test("Gets definition location of struct as a function output parameter type", async () => {
        const document_uri = get_document_uri("projects/other/definition_struct_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(7, 43), [
            new vscode.Location(get_document_uri("projects/other/definition_struct_0.hltxt"), to_range(2, 7, 2, 16))
        ]);
    });

    test("Gets definition location of struct as a variable declaration type", async () => {
        const document_uri = get_document_uri("projects/other/definition_struct_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(9, 21), [
            new vscode.Location(get_document_uri("projects/other/definition_struct_0.hltxt"), to_range(2, 7, 2, 16))
        ]);
    });

    test("Gets definition location of struct member 0 at instantiate expression", async () => {
        const document_uri = get_document_uri("projects/other/definition_struct_1.hltxt");
        await test_definitions(document_uri, new vscode.Position(11, 9), [
            new vscode.Location(get_document_uri("projects/other/definition_struct_1.hltxt"), to_range(4, 4, 4, 5))
        ]);
    });

    test("Gets definition location of struct member 1 at instantiate expression", async () => {
        const document_uri = get_document_uri("projects/other/definition_struct_1.hltxt");
        await test_definitions(document_uri, new vscode.Position(12, 8), [
            new vscode.Location(get_document_uri("projects/other/definition_struct_1.hltxt"), to_range(5, 4, 5, 5))
        ]);
    });

    test("Gets definition location of struct member 2 at instantiate expression", async () => {
        const document_uri = get_document_uri("projects/other/definition_struct_2.hltxt");
        await test_definitions(document_uri, new vscode.Position(14, 21), [
            new vscode.Location(get_document_uri("projects/other/definition_struct_2.hltxt"), to_range(5, 4, 5, 5))
        ]);
    });

    test("Gets definition location of struct member at access expression 0", async () => {
        const document_uri = get_document_uri("projects/other/definition_struct_3.hltxt");
        await test_definitions(document_uri, new vscode.Position(11, 21), [
            new vscode.Location(get_document_uri("projects/other/definition_struct_3.hltxt"), to_range(4, 4, 4, 5))
        ]);
    });

    test("Gets definition location of struct member at access expression 1", async () => {
        const document_uri = get_document_uri("projects/other/definition_struct_3.hltxt");
        await test_definitions(document_uri, new vscode.Position(11, 22), [
            new vscode.Location(get_document_uri("projects/other/definition_struct_3.hltxt"), to_range(4, 4, 4, 5))
        ]);
    });

    test("Gets definition location of struct member at access expression 2", async () => {
        const document_uri = get_document_uri("projects/other/definition_struct_3.hltxt");
        await test_definitions(document_uri, new vscode.Position(12, 21), [
            new vscode.Location(get_document_uri("projects/other/definition_struct_3.hltxt"), to_range(5, 4, 5, 5))
        ]);
    });

    test("Gets definition location of struct at struct member type", async () => {
        const document_uri = get_document_uri("projects/other/definition_struct_4.hltxt");
        await test_definitions(document_uri, new vscode.Position(4, 11), [
            new vscode.Location(get_document_uri("projects/other/definition_struct_4.hltxt"), to_range(2, 7, 2, 16))
        ]);
    });
});

suite("Should get definition location of unions", () => {

    test.skip("Gets definition location of itself", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(2, 6), [
            new vscode.Location(get_document_uri("projects/other/definition_union_0.hltxt"), to_range(2, 6, 2, 14))
        ]);
    });

    test("Gets definition location of union as a function input parameter type", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(8, 20), [
            new vscode.Location(get_document_uri("projects/other/definition_union_0.hltxt"), to_range(2, 6, 2, 14))
        ]);
    });

    test("Gets definition location of union as a function output parameter type", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(8, 42), [
            new vscode.Location(get_document_uri("projects/other/definition_union_0.hltxt"), to_range(2, 6, 2, 14))
        ]);
    });

    test("Gets definition location of union as a variable declaration type", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(10, 20), [
            new vscode.Location(get_document_uri("projects/other/definition_union_0.hltxt"), to_range(2, 6, 2, 14))
        ]);
    });

    test("Gets definition location of union member at instantiate expression 0", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(11, 8), [
            new vscode.Location(get_document_uri("projects/other/definition_union_0.hltxt"), to_range(4, 4, 4, 5))
        ]);
    });

    test("Gets definition location of union member at instantiate expression 1", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(16, 8), [
            new vscode.Location(get_document_uri("projects/other/definition_union_0.hltxt"), to_range(5, 4, 5, 5))
        ]);
    });

    test("Gets definition location of union member at instantiate expression 2", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(16, 9), [
            new vscode.Location(get_document_uri("projects/other/definition_union_0.hltxt"), to_range(5, 4, 5, 5))
        ]);
    });

    test("Gets definition location of union member at instantiate expression 3", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_1.hltxt");
        await test_definitions(document_uri, new vscode.Position(14, 11), [
            new vscode.Location(get_document_uri("projects/other/definition_union_1.hltxt"), to_range(4, 4, 4, 5))
        ]);
    });

    test("Gets definition location of union member at instantiate expression 4", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_1.hltxt");
        await test_definitions(document_uri, new vscode.Position(14, 22), [
            new vscode.Location(get_document_uri("projects/other/definition_union_1.hltxt"), to_range(5, 4, 5, 5))
        ]);
    });

    test("Gets definition location of union member at access expression 0", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(13, 23), [
            new vscode.Location(get_document_uri("projects/other/definition_union_0.hltxt"), to_range(4, 4, 4, 5))
        ]);
    });

    test("Gets definition location of union member at access expression 1", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(13, 24), [
            new vscode.Location(get_document_uri("projects/other/definition_union_0.hltxt"), to_range(4, 4, 4, 5))
        ]);
    });

    test("Gets definition location of union member at access expression 2", async () => {
        const document_uri = get_document_uri("projects/other/definition_union_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(18, 23), [
            new vscode.Location(get_document_uri("projects/other/definition_union_0.hltxt"), to_range(5, 4, 5, 5))
        ]);
    });
});

suite("Should get definition location of functions", () => {

    test("Gets definition location of function name 0", async () => {
        const document_uri = get_document_uri("projects/other/definition_function_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(9, 12), [
            new vscode.Location(get_document_uri("projects/other/definition_function_0.hltxt"), to_range(2, 9, 2, 12))
        ]);
    });

    test("Gets definition location of function name 1", async () => {
        const document_uri = get_document_uri("projects/other/definition_function_1.hltxt");
        await test_definitions(document_uri, new vscode.Position(17, 11), [
            new vscode.Location(get_document_uri("projects/other/definition_function_1.hltxt"), to_range(8, 9, 8, 12))
        ]);
    });

    test.skip("Gets definition location of function input parameter name 0", async () => {
        const document_uri = get_document_uri("projects/other/definition_function_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(2, 13), [
            new vscode.Location(get_document_uri("projects/other/definition_function_0.hltxt"), to_range(2, 13, 2, 16))
        ]);
    });

    test.skip("Gets definition location of function input parameter name 1", async () => {
        const document_uri = get_document_uri("projects/other/definition_function_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(2, 28), [
            new vscode.Location(get_document_uri("projects/other/definition_function_0.hltxt"), to_range(2, 25, 2, 28))
        ]);
    });

});

suite("Should get definition location of enums", () => {

    test.skip("Gets definition location of enum at itself", async () => {
        const document_uri = get_document_uri("projects/other/definition_enum_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(2, 5), [
            new vscode.Location(get_document_uri("projects/other/definition_enum_0.hltxt"), to_range(2, 5, 2, 12))
        ]);
    });

    test("Gets definition location of enum at function input parameter type", async () => {
        const document_uri = get_document_uri("projects/other/definition_enum_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(9, 20), [
            new vscode.Location(get_document_uri("projects/other/definition_enum_0.hltxt"), to_range(2, 5, 2, 12))
        ]);
    });

    test("Gets definition location of enum at function output parameter type", async () => {
        const document_uri = get_document_uri("projects/other/definition_enum_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(9, 41), [
            new vscode.Location(get_document_uri("projects/other/definition_enum_0.hltxt"), to_range(2, 5, 2, 12))
        ]);
    });

    test("Gets definition location of enum at access expression 0", async () => {
        const document_uri = get_document_uri("projects/other/definition_enum_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(11, 12), [
            new vscode.Location(get_document_uri("projects/other/definition_enum_0.hltxt"), to_range(2, 5, 2, 12))
        ]);
    });

    test("Gets definition location of enum at access expression 1", async () => {
        const document_uri = get_document_uri("projects/other/definition_enum_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(12, 19), [
            new vscode.Location(get_document_uri("projects/other/definition_enum_0.hltxt"), to_range(2, 5, 2, 12))
        ]);
    });

    test("Gets definition location of enum at access expression 2", async () => {
        const document_uri = get_document_uri("projects/other/definition_enum_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(12, 15), [
            new vscode.Location(get_document_uri("projects/other/definition_enum_0.hltxt"), to_range(2, 5, 2, 12))
        ]);
    });

    test("Gets definition location of enum value 0", async () => {
        const document_uri = get_document_uri("projects/other/definition_enum_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(11, 20), [
            new vscode.Location(get_document_uri("projects/other/definition_enum_0.hltxt"), to_range(4, 4, 4, 11))
        ]);
    });

    test("Gets definition location of enum value 1", async () => {
        const document_uri = get_document_uri("projects/other/definition_enum_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(12, 27), [
            new vscode.Location(get_document_uri("projects/other/definition_enum_0.hltxt"), to_range(5, 4, 5, 11))
        ]);
    });

    test("Gets definition location of enum value 2", async () => {
        const document_uri = get_document_uri("projects/other/definition_enum_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(13, 23), [
            new vscode.Location(get_document_uri("projects/other/definition_enum_0.hltxt"), to_range(6, 4, 6, 11))
        ]);
    });
});

suite("Should get definition location of global variables", () => {

    test.skip("Gets global variable definition location of itself", async () => {
        const document_uri = get_document_uri("projects/other/definition_global_variable_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(2, 6), [
            new vscode.Location(get_document_uri("projects/other/definition_global_variable_0.hltxt"), to_range(2, 4, 2, 22))
        ]);
    });

    test("Gets definition location of global variable when used as expression", async () => {
        const document_uri = get_document_uri("projects/other/definition_global_variable_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(6, 15), [
            new vscode.Location(get_document_uri("projects/other/definition_global_variable_0.hltxt"), to_range(2, 4, 2, 22))
        ]);
    });

    test("Gets definition location of define when used as expression", async () => {
        const document_uri = get_document_uri("projects/project_1/definition_global_variable_0.hltxt");
        await test_definitions(document_uri, new vscode.Position(6, 20), [
            new vscode.Location(get_document_uri("projects/complex/complex.h"), to_range(24, 8, 24, 10))
        ]);
    });
});


function to_range(start_line: number, start_character: number, end_line: number, end_character: number): vscode.Range {
    const start = new vscode.Position(start_line, start_character);
    const end = new vscode.Position(end_line, end_character);
    return new vscode.Range(start, end);
}

async function test_definitions(document_uri: vscode.Uri, position: vscode.Position, expected_definitions: vscode.Location[]) {
    await activate(document_uri);

    const actual_definitions = (await vscode.commands.executeCommand(
        'vscode.executeDefinitionProvider',
        document_uri,
        position
    )) as vscode.LocationLink[];

    assert.equal(actual_definitions.length, expected_definitions.length);

    expected_definitions.forEach((expected_definition, i) => {
        const actual_definition = actual_definitions[i];

        assert.equal(actual_definition.targetUri.fsPath, expected_definition.uri.fsPath);
        assert.deepEqual(actual_definition.targetSelectionRange, expected_definition.range);
    });
}
