import * as child_process from 'child_process';
import * as path from 'path';
import * as net from 'net';
import * as vscode from 'vscode';

import {
	Executable,
	LanguageClient,
	LanguageClientOptions,
	NodeModule,
	ServerOptions,
	StreamInfo,
	TransportKind
} from 'vscode-languageclient/node.js';

import { setup_test_executables_watcher } from './test_adapter';

let client: LanguageClient | undefined = undefined;
let server_process: child_process.ChildProcess | undefined = undefined;

async function create_server_options_to_create(
	configuration: vscode.WorkspaceConfiguration
): Promise<ServerOptions> {

	const server_env_path = process.env.hlang_language_server;
	const server_path = server_env_path !== undefined ? server_env_path : configuration.get<string>("hlang_language_server_executable", "hlang_language_server") ;

	const options: child_process.ExecSyncOptions = {};

	const language_server_process = child_process.spawn(server_path, options);

	let server_is_ready = false;

	if (language_server_process.stdout != null) {
		language_server_process.stdout.on("data", (data) => {
			const message = data.toString();
			if (message.startsWith("Listening...")) {
				server_is_ready = true;
			}
			console.log("Server:", data.toString());
		});
	}
    
	if (language_server_process.stderr != null) {
		language_server_process.stderr.on("data", (data) => {
			console.error("Server error:", data.toString());
		});
	}

	language_server_process.on("exit", () => {
		if (!language_server_process.killed) {
			language_server_process.kill();
		}
	});

	const wait_for_server = new Promise<void>((resolve, reject) => {
		const loop = () => {
            if (server_is_ready) {
                resolve();
            } else {
                setTimeout(loop, 500);
            }
        };
        loop();
	});

	await wait_for_server;

	server_process = language_server_process;

	return create_server_options_to_attach("127.0.0.1", 12345);
}

async function create_server_options_to_attach(
	server_host: string,
	server_port: number
): Promise<ServerOptions> {
	 
	const server_options = () => {
        return new Promise<StreamInfo>((resolve, reject) => {
            const socket = net.connect(server_port, server_host, () => {
                resolve({
                    reader: socket,
                    writer: socket
                });
            });

            socket.on('error', (err) => {
                reject(err);
            });
        });
    };

	return server_options;
}

export async function activate(context: vscode.ExtensionContext): Promise<LanguageClient> {

	const configuration = vscode.workspace.getConfiguration("hlang");

	const mode: string | undefined = process.env.mode;
	const attach_to_server = mode !== undefined && mode === "debug";

	const server_options = 
		attach_to_server ?
		await create_server_options_to_attach("127.0.0.1", 12345) :
		await create_server_options_to_create(configuration);

	// Options to control the language client
	const client_options: LanguageClientOptions = {
		// Register the server for plain text documents
		documentSelector: [{ scheme: 'file', language: 'hlang' }],
		synchronize: {
			// Notify the server about file changes to '.clientrc files contained in the workspace
			fileEvents: vscode.workspace.createFileSystemWatcher('**/.clientrc')
		}
	};

	// Create the language client and start the client.
	client = new LanguageClient(
		'HLanglanguageServer',
		'HLang Language Server',
		server_options,
		client_options
	);

	/*client.middleware.didOpen = on_did_open;
	client.middleware.didChange = on_did_change;
	client.middleware.provideCompletionItem = provide_completion_item;
	client.middleware.provideInlayHints = provide_inlay_hints;*/

	const workspace_initialized_promise = wait_for_workspace_initialized_notification(client);

	await client.start();

	await workspace_initialized_promise;

	// Set up test executable watcher (discover and show tests in the Test Explorer)
	try {
		const tests_configuration = vscode.workspace.getConfiguration('hlang');
		const glob = tests_configuration.get<string>('test_executable_glob', 'build/bin/*.hlang.test*');
		const watcher_disposable = setup_test_executables_watcher(glob);
		context.subscriptions.push(watcher_disposable);
	} catch (error) {
		console.error('Failed to set up test executables watcher:', error);
	}

	return client;
}

async function wait_for_workspace_initialized_notification(
	client: LanguageClient
): Promise<void> {
	return new Promise( resolve => {
		client.onNotification("hlang/workspaceInitialized", () => {
			resolve();
		});
	});
}

export async function deactivate(): Promise<void> {

	if (client !== undefined) {
		await client.sendNotification("exit");
		client.stop();
		client = undefined;
	}

	if (server_process !== undefined) {
		server_process.kill();
		server_process = undefined;
	}
	
	return undefined;
}

/*interface Document_state {
	text: string;
	pending_change_events: vscode.TextDocumentContentChangeEvent[];
}

const g_documents = new Map<string, Document_state>();
let g_cancel_debounces: (() => void)[] = [];

function create_debounce_promise<T>(
	delay: number,
	predicate: () => T
): Promise<T> {

	let timeout: NodeJS.Timeout | undefined = undefined;
	let reject_promise: (value: T | PromiseLike<T>) => void = null;

	const cancel = () => {
		if (timeout !== undefined) {
			clearTimeout(timeout);
		}
		if (reject_promise !== null) {
			reject_promise(undefined);
		}
	};
	g_cancel_debounces.push(cancel);

	const promise = new Promise<T>((resolve, reject) => {
		reject_promise = resolve;
		timeout = setTimeout(() => {
			resolve(predicate());

			const index = g_cancel_debounces.findIndex(element => element === cancel);
			g_cancel_debounces.splice(index, 1);
		}, delay);
	});

	return promise;
}

function cancel_all_debounce_timeouts() {
	for (const cancel of g_cancel_debounces) {
		cancel();
	}
	g_cancel_debounces = [];
}

function on_did_open(data: vscode.TextDocument, next: (data: vscode.TextDocument) => Promise<void>): Promise<void> {

	const document_state: Document_state = {
		text: data.getText(),
		pending_change_events: []
	};

	g_documents.set(data.uri.toString(), document_state);

	return next(data);
}

function on_did_change(data: vscode.TextDocumentChangeEvent, next: (data: vscode.TextDocumentChangeEvent) => Promise<void>): Promise<void> {

	cancel_all_debounce_timeouts();

	const document_state = g_documents.get(data.document.uri.toString());
	if (document_state === undefined) {
		return next(data);
	}

	document_state.pending_change_events.push(...data.contentChanges);

	return create_debounce_promise<void>(100, () => {
		const content_changes = [...document_state.pending_change_events];
		document_state.pending_change_events = [];

		const new_event: vscode.TextDocumentChangeEvent = {
			document: data.document,
			contentChanges: content_changes,
			reason: data.reason,
		};

		return next(new_event);
	});
}

function provide_completion_item(
	document: vscode.TextDocument,
	position: vscode.Position,
	context: vscode.CompletionContext,
	token: vscode.CancellationToken,
	next: (document: vscode.TextDocument, position: vscode.Position, context: vscode.CompletionContext, token: vscode.CancellationToken) => Promise<vscode.CompletionItem[] | vscode.CompletionList>
): Promise<vscode.CompletionItem[] | vscode.CompletionList> {
	const result = create_debounce_promise(200, () => {
		return next(document, position, context, token);
	});
	return Promise.resolve(result);
}

function provide_inlay_hints(
	document: vscode.TextDocument,
	view_port: vscode.Range,
	token: vscode.CancellationToken,
	next: (document: vscode.TextDocument, viewPort: vscode.Range, token: vscode.CancellationToken) => vscode.ProviderResult<vscode.InlayHint[]>
): Promise<vscode.InlayHint[]> {
	const result = create_debounce_promise(400, () => {
		return next(document, view_port, token);
	});
	return Promise.resolve(result);
}*/
