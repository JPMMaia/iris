import * as vscode from 'vscode';
import * as assert from 'assert';
import * as language_client from 'vscode-languageclient';
import { get_document_uri, activate } from './helper.js';

suite("Should get instantiate expression add missing members code action", () => {

    test("Add missing instantiate members 0", async () => {
        const document_uri = get_document_uri("projects/other/code_action_instantiate_0.hltxt");
        await test_code_actions(document_uri, to_range(12, 31, 12, 31), vscode.CodeActionKind.RefactorRewrite, [
            {
                title: "Add missing instantiate members",
                kind: vscode.CodeActionKind.RefactorRewrite,
                edit: create_replace_workspace_edit(document_uri, to_range(12, 30, 12, 32), "{\n        a: 0,\n        b: 1,\n        c: 2,\n        d: 3\n    }")
            }
        ]);
    });

    test("Add missing instantiate members, without modifying existing member values", async () => {
        const document_uri = get_document_uri("projects/other/code_action_instantiate_1.hltxt");
        await test_code_actions(document_uri, to_range(12, 31, 12, 31), vscode.CodeActionKind.RefactorRewrite, [
            {
                title: "Add missing instantiate members",
                kind: vscode.CodeActionKind.RefactorRewrite,
                edit: create_replace_workspace_edit(document_uri, to_range(12, 30, 14, 5), "{\n        a: 0,\n        b: 2,\n        c: 2,\n        d: 3\n    }")
            }
        ]);
    });

    test("Do not get any code action if all instantiate members are present", async () => {
        const document_uri = get_document_uri("projects/other/code_action_instantiate_2.hltxt");
        await test_code_actions(document_uri, to_range(12, 31, 12, 31), vscode.CodeActionKind.RefactorRewrite, []);
    });

    test("Add missing instantiate members which are also structs", async () => {
        const document_uri = get_document_uri("projects/other/code_action_instantiate_3.hltxt");
        await test_code_actions(document_uri, to_range(23, 29, 23, 29), vscode.CodeActionKind.RefactorRewrite, [
            {
                title: "Add missing instantiate members",
                kind: vscode.CodeActionKind.RefactorRewrite,
                edit: create_replace_workspace_edit(document_uri, to_range(23, 28, 23, 30), "{\n        x: {},\n        y: {\n            b: 5\n        },\n        z: {\n            d: 7\n        }\n    }")
            }
        ]);
    });

    test("Add missing instantiate members inside a instantiate expression", async () => {
        const document_uri = get_document_uri("projects/other/code_action_instantiate_4.hltxt");
        await test_code_actions(document_uri, to_range(24, 12, 24, 12), vscode.CodeActionKind.RefactorRewrite, [
            {
                title: "Add missing instantiate members",
                kind: vscode.CodeActionKind.RefactorRewrite,
                edit: create_replace_workspace_edit(document_uri, to_range(24, 11, 24, 13), "{\n            a: 0,\n            b: 1,\n            c: 2,\n            d: 3\n        }")
            }
        ]);
    });

    test("Add missing instantiate members inside an explicit instantiate expression", async () => {
        const document_uri = get_document_uri("projects/other/code_action_instantiate_5.hltxt");
        await test_code_actions(document_uri, to_range(12, 40, 12, 40), vscode.CodeActionKind.RefactorRewrite, [
            {
                title: "Add missing instantiate members",
                kind: vscode.CodeActionKind.QuickFix,
                edit: create_replace_workspace_edit(document_uri, to_range(12, 30, 12, 41), "explicit {\n        a: 0,\n        b: 1,\n        c: 2,\n        d: 3\n    }")
            }
        ]);
    });

    test("Add missing instantiate member that is imported", async () => {
        const document_uri = get_document_uri("projects/other/code_action_instantiate_6.hltxt");
        await test_code_actions(document_uri, to_range(6, 44, 6, 44), vscode.CodeActionKind.RefactorRewrite, [
            {
                title: "Add missing instantiate members",
                kind: vscode.CodeActionKind.QuickFix,
                edit: create_replace_workspace_edit(document_uri, to_range(6, 34, 6, 45), "explicit {\n        a: ext.My_enum.Member_1\n    }")
            }
        ]);
    });
});

suite("Code action should fix type mismatch using cast", () => {

    test("Fix mismatch using cast 0", async () => {
        const document_uri = get_document_uri("projects/other/code_action_fix_type_mismatch_using_cast_0.hltxt");
        await test_code_actions(document_uri, to_range(6, 21, 6, 21), vscode.CodeActionKind.QuickFix, [
            {
                title: "Add cast from 'Int32' to 'My_int' ('Int32' to 'Int32')",
                kind: vscode.CodeActionKind.QuickFix,
                edit: create_replace_workspace_edit(document_uri, to_range(6, 22, 6, 22), " as My_int")
            }
        ]);
    });

    test("Fix mismatch using cast 1", async () => {
        const document_uri = get_document_uri("projects/other/code_action_fix_type_mismatch_using_cast_0.hltxt");
        await test_code_actions(document_uri, to_range(7, 21, 7, 21), vscode.CodeActionKind.QuickFix, [
            {
                title: "Add cast from 'Uint32' to 'My_int' ('Uint32' to 'Int32')",
                kind: vscode.CodeActionKind.QuickFix,
                edit: create_replace_workspace_edit(document_uri, to_range(7, 25, 7, 25), " as My_int")
            }
        ]);
    });

    test("Fix mismatch using cast 2", async () => {
        const document_uri = get_document_uri("projects/other/code_action_fix_type_mismatch_using_cast_0.hltxt");
        await test_code_actions(document_uri, to_range(8, 22, 8, 22), vscode.CodeActionKind.QuickFix, [
            {
                title: "Add cast from 'Uint32' to 'Float32' ('Uint32' to 'Float32')",
                kind: vscode.CodeActionKind.QuickFix,
                edit: create_replace_workspace_edit(document_uri, to_range(8, 26, 8, 26), " as Float32")
            }
        ]);
    });
});

function to_range(start_line: number, start_character: number, end_line: number, end_character: number): vscode.Range {
    const start = new vscode.Position(start_line, start_character);
    const end = new vscode.Position(end_line, end_character);
    return new vscode.Range(start, end);
}

function create_replace_workspace_edit(
    uri: vscode.Uri,
    range: vscode.Range,
    new_text: string
): vscode.WorkspaceEdit {
    const edit = new vscode.WorkspaceEdit();
    edit.replace(uri, range, new_text);
    return edit;
}

async function test_code_actions(document_uri: vscode.Uri, range_or_selection: vscode.Range | vscode.Selection, kind: vscode.CodeActionKind, expected_code_actions: vscode.CodeAction[]) {
    const client = await activate(document_uri);
    
    const parameters: language_client.WorkspaceDiagnosticParams = {
            identifier: "hlang",
            previousResultIds: []
        };
    
    await client.sendRequest(language_client.WorkspaceDiagnosticRequest.method, parameters);

    const actual_code_actions = (await vscode.commands.executeCommand(
        'vscode.executeCodeActionProvider',
        document_uri,
        range_or_selection
    )) as vscode.CodeAction[];

    assert.equal(actual_code_actions.length, expected_code_actions.length);

    expected_code_actions.forEach((expected_code_action, i) => {
        const actual_code_action = actual_code_actions[i];

        assert.equal(actual_code_action.title, expected_code_action.title);
        assert.deepEqual(actual_code_action.kind, expected_code_action.kind);
        assert_workspace_edits(actual_code_action.edit, expected_code_action.edit);
    });
}

function assert_workspace_edits(actual: vscode.WorkspaceEdit | undefined, expected: vscode.WorkspaceEdit | undefined): void {

    if (actual === undefined || expected === undefined) {
        assert.equal(actual, expected);
        return;
    }

    const actual_entries = actual.entries();
    const expected_entries = expected.entries();

    assert.equal(actual_entries.length, expected_entries.length);

    for (let i = 0; i < actual_entries.length; i++) {
        const [actual_uri, actual_edits] = actual_entries[i];
        const [expected_uri, expected_edits] = expected_entries[i];

        assert.equal(actual_uri.fsPath, expected_uri.fsPath);

        assert.equal(actual_edits.length, expected_edits.length);

        for (let j = 0; j < actual_edits.length; j++) {
            const actual_edit = actual_edits[j];
            const expected_edit = expected_edits[j];

            assert.deepEqual(actual_edit.range, expected_edit.range);
            assert.equal(actual_edit.newText, expected_edit.newText);
        }
    }
}
