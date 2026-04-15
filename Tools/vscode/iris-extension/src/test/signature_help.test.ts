import * as vscode from 'vscode';
import * as assert from 'assert';
import { get_document_uri, activate } from './helper.js';

suite("Should display function signature", () => {
    test.skip("Should show function signature with first parameter selected, and documentation 0", async () => {
        const document_uri = get_document_uri('signature_help_function_0.hltxt');
        await test_signature_help(document_uri, new vscode.Position(17, 21), {
            signatures: [create_add_function_signature()],
            activeSignature: 0,
            activeParameter: 0
        });
    });

    test.skip("Should show function signature with first parameter selected, and documentation 1", async () => {
        const document_uri = get_document_uri('signature_help_function_1.hltxt');
        await test_signature_help(document_uri, new vscode.Position(17, 24), {
            signatures: [create_add_function_signature()],
            activeSignature: 0,
            activeParameter: 0
        });
    });

    test.skip("Should show function signature with second parameter selected, and documentation 0", async () => {
        const document_uri = get_document_uri('signature_help_function_2.hltxt');
        await test_signature_help(document_uri, new vscode.Position(17, 25), {
            signatures: [create_add_function_signature()],
            activeSignature: 0,
            activeParameter: 1
        });
    });

    test.skip("Should show function signature with second parameter selected, and documentation 1", async () => {
        const document_uri = get_document_uri('signature_help_function_3.hltxt');
        await test_signature_help(document_uri, new vscode.Position(17, 29), {
            signatures: [create_add_function_signature()],
            activeSignature: 0,
            activeParameter: 1
        });
    });

    test.skip("Should show function signature of imported function", async () => {
        const document_uri = get_document_uri('projects/project_1/signature_help_function_0.hltxt');
        await test_signature_help(document_uri, new vscode.Position(8, 24), {
            signatures: [create_complex_add_function_signature()],
            activeSignature: 0,
            activeParameter: 0
        });
        await test_signature_help(document_uri, new vscode.Position(8, 27), {
            signatures: [create_complex_add_function_signature()],
            activeSignature: 0,
            activeParameter: 1
        });
    });
});

function create_add_function_signature(): vscode.SignatureInformation {
    return {
        label: "function add(lhs: Int32, rhs: Int32) -> (result: Int32)",
        parameters: [
            {
                label: [13, 23],
                documentation: "Left hand side of add expression"
            },
            {
                label: [25, 35],
                documentation: "Right hand side of add expression"
            }
        ],
        documentation: "Add two integers\n\nAdd two 32-bit integers.\nIt returns the result of adding lhs and rhs.",
        activeParameter: undefined
    };
}

function create_complex_add_function_signature(): vscode.SignatureInformation {
    return {
        label: "function add(lhs: complex.Complex, rhs: complex.Complex) -> (result: complex.Complex)",
        parameters: [
            {
                label: [13, 33],
                documentation: undefined
            },
            {
                label: [35, 55],
                documentation: undefined
            }
        ],
        documentation: undefined,
        activeParameter: undefined
    };
}

suite("Should display struct signature", () => {
    test.skip("Should show struct signature 0", async () => {
        const document_uri = get_document_uri('signature_help_struct_0.hltxt');
        await test_signature_help(document_uri, new vscode.Position(14, 31), {
            signatures: [create_complex_struct_signature()],
            activeSignature: 0,
            activeParameter: 0
        });
    });

    test.skip("Should show struct signature 1", async () => {
        const document_uri = get_document_uri('signature_help_struct_1.hltxt');
        await test_signature_help(document_uri, new vscode.Position(15, 21), {
            signatures: [create_complex_struct_signature()],
            activeSignature: 0,
            activeParameter: 1
        });
    });

    test.skip("Should show struct signature 2", async () => {
        const document_uri = get_document_uri('signature_help_struct_2.hltxt');
        await test_signature_help(document_uri, new vscode.Position(21, 30), {
            signatures: [create_complex_struct_signature()],
            activeSignature: 0,
            activeParameter: 0
        });
    });

    test.skip("Should show struct signature 3", async () => {
        const document_uri = get_document_uri('signature_help_struct_3.hltxt');
        await test_signature_help(document_uri, new vscode.Position(26, 25), {
            signatures: [create_complex_struct_signature()],
            activeSignature: 0,
            activeParameter: 1
        });
    });

    test.skip("Should show struct signature 4", async () => {
        const document_uri = get_document_uri('signature_help_struct_4.hltxt');
        await test_signature_help(document_uri, new vscode.Position(16, 19), {
            signatures: [create_complex_struct_signature()],
            activeSignature: 0,
            activeParameter: 0
        });
    });

    test.skip("Should show struct signature 5", async () => {
        const document_uri = get_document_uri('signature_help_struct_5.hltxt');
        await test_signature_help(document_uri, new vscode.Position(20, 21), {
            signatures: [create_complex_struct_signature()],
            activeSignature: 0,
            activeParameter: 1
        });
    });

    test.skip("Should show struct signature 6", async () => {
        const document_uri = get_document_uri('signature_help_struct_6.hltxt');
        await test_signature_help(document_uri, new vscode.Position(15, 21), {
            signatures: [create_complex_struct_signature()],
            activeSignature: 0,
            activeParameter: 1
        });
    });

    test.skip("Should show struct signature 7", async () => {
        const document_uri = get_document_uri('signature_help_struct_7.hltxt');
        await test_signature_help(document_uri, new vscode.Position(18, 26), {
            signatures: [create_complex_struct_signature()],
            activeSignature: 0,
            activeParameter: 1
        });
    });

    test.skip("Should show struct signature 8", async () => {
        const document_uri = get_document_uri('signature_help_struct_8.hltxt');
        await test_signature_help(document_uri, new vscode.Position(16, 21), {
            signatures: [create_complex_struct_signature()],
            activeSignature: 0,
            activeParameter: 1
        });
    });

    test.skip("Active parameter should be the member after the previous parameter name if nothing is written yet", async () => {
        const document_uri = get_document_uri('signature_help_struct_9.hltxt');
        await test_signature_help(document_uri, new vscode.Position(13, 20), {
            signatures: [create_foo_struct_signature()],
            activeSignature: 0,
            activeParameter: 2
        });
    });

    test.skip("Active parameter should be the member whose name best matches what is being written", async () => {
        const document_uri = get_document_uri('signature_help_struct_10.hltxt');
        await test_signature_help(document_uri, new vscode.Position(14, 10), {
            signatures: [create_foo_struct_signature()],
            activeSignature: 0,
            activeParameter: 3
        });
    });

    test.skip("Active parameter should show the member that already exists at the cursor 0", async () => {
        const document_uri = get_document_uri('signature_help_struct_11.hltxt');
        await test_signature_help(document_uri, new vscode.Position(12, 25), {
            signatures: [create_foo_struct_signature()],
            activeSignature: 0,
            activeParameter: 1
        });
    });

    test.skip("Active parameter should show the member that already exists at the cursor 1", async () => {
        const document_uri = get_document_uri('signature_help_struct_11.hltxt');
        await test_signature_help(document_uri, new vscode.Position(13, 19), {
            signatures: [create_foo_struct_signature()],
            activeSignature: 0,
            activeParameter: 1
        });
    });

    test.skip("Active parameter should show the member that already exists at the cursor 2", async () => {
        const document_uri = get_document_uri('signature_help_struct_11.hltxt');
        await test_signature_help(document_uri, new vscode.Position(13, 20), {
            signatures: [create_foo_struct_signature()],
            activeSignature: 0,
            activeParameter: 3
        });
    });

    test.skip("Active parameter should show the member that already exists at the cursor 3", async () => {
        const document_uri = get_document_uri('signature_help_struct_11.hltxt');
        await test_signature_help(document_uri, new vscode.Position(14, 15), {
            signatures: [create_foo_struct_signature()],
            activeSignature: 0,
            activeParameter: 3
        });
    });
});

function create_complex_struct_signature(): vscode.SignatureInformation {
    return {
        label: "Complex {\n    real: Float32 = 0.0f32,\n    imaginary: Float32 = 0.0f32\n}",
        parameters: [
            {
                label: [14, 36],
                documentation: "The real part."
            },
            {
                label: [42, 69],
                documentation: "The imaginary part."
            }
        ],
        documentation: "Represents complex numbers. Uses 32-bit floats.",
        activeParameter: undefined
    };
}

function create_foo_struct_signature(): vscode.SignatureInformation {
    return {
        label: "Foo {\n    application: Int32 = 0,\n    instance: Int32 = 0,\n    command: Int32 = 0,\n    include: Int32 = 0\n}",
        parameters: [
            {
                label: [10, 32],
                documentation: undefined
            },
            {
                label: [38, 57],
                documentation: undefined
            },
            {
                label: [63, 81],
                documentation: undefined
            },
            {
                label: [87, 105],
                documentation: undefined
            }
        ],
        documentation: undefined,
        activeParameter: undefined
    };
}

async function test_signature_help(
    document_uri: vscode.Uri,
    position: vscode.Position,
    expected_signature_help: vscode.SignatureHelp
) {
    await activate(document_uri);

    const actual_signature_help = (await vscode.commands.executeCommand(
        'vscode.executeSignatureHelpProvider',
        document_uri,
        position
    )) as vscode.SignatureHelp;

    assert.deepEqual(actual_signature_help, expected_signature_help);
}
