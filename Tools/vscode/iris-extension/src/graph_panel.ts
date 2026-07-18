import * as vscode from 'vscode';

export interface Graph_node {
	id: string;
	label: string;
	external: boolean;
	filePath?: string;
}

export interface Graph_edge {
	from: string;
	to: string;
}

export interface Dependency_graph {
	nodes: Graph_node[];
	edges: Graph_edge[];
}

let current_panel: vscode.WebviewPanel | undefined = undefined;

export function show_graph_panel(
	context: vscode.ExtensionContext,
	title: string,
	graph: Dependency_graph
): void {
	const column = vscode.window.activeTextEditor?.viewColumn;

	if (current_panel !== undefined) {
		current_panel.title = title;
		current_panel.reveal(column);
	} else {
		current_panel = vscode.window.createWebviewPanel(
			'irisModuleDependencyGraph',
			title,
			column ?? vscode.ViewColumn.One,
			{
				enableScripts: true,
				retainContextWhenHidden: true,
				localResourceRoots: [
					vscode.Uri.joinPath(context.extensionUri, 'media'),
					vscode.Uri.joinPath(context.extensionUri, 'node_modules', 'cytoscape', 'dist')
				]
			}
		);

		current_panel.onDidDispose(() => {
			current_panel = undefined;
		});
	}

	const webview = current_panel.webview;

	// The webview posts 'ready' once its script has loaded; only then does it have a
	// listener for the graph data. Re-send on every render so re-running the command
	// updates the existing panel.
	const message_subscription = webview.onDidReceiveMessage((message) => {
		if (message?.type === 'ready') {
			webview.postMessage({ type: 'setGraph', graph });
		}
	});
	current_panel.onDidDispose(() => message_subscription.dispose());

	webview.html = get_html(webview, context.extensionUri);

	// If the panel already existed the webview is already loaded and will not fire
	// 'ready' again, so push the new graph directly as well.
	webview.postMessage({ type: 'setGraph', graph });
}

function get_html(webview: vscode.Webview, extension_uri: vscode.Uri): string {
	const cytoscape_uri = webview.asWebviewUri(
		vscode.Uri.joinPath(extension_uri, 'node_modules', 'cytoscape', 'dist', 'cytoscape.min.js')
	);
	const script_uri = webview.asWebviewUri(
		vscode.Uri.joinPath(extension_uri, 'media', 'graph.js')
	);

	const nonce = get_nonce();

	return `<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8">
	<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src ${webview.cspSource} 'unsafe-inline'; script-src 'nonce-${nonce}';">
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<title>Iris Module Dependency Graph</title>
	<style>
		html, body { height: 100%; margin: 0; padding: 0; }
		body {
			display: flex;
			color: var(--vscode-editor-foreground);
			background-color: var(--vscode-editor-background);
			font-family: var(--vscode-font-family);
			font-size: var(--vscode-font-size);
		}
		#sidebar {
			width: 240px;
			min-width: 240px;
			border-right: 1px solid var(--vscode-panel-border, #444);
			display: flex;
			flex-direction: column;
			overflow: hidden;
		}
		#toolbar {
			padding: 8px;
			display: flex;
			gap: 6px;
			flex-wrap: wrap;
			border-bottom: 1px solid var(--vscode-panel-border, #444);
		}
		#toolbar button {
			color: var(--vscode-button-foreground);
			background-color: var(--vscode-button-background);
			border: none;
			padding: 4px 8px;
			cursor: pointer;
		}
		#toolbar button:hover { background-color: var(--vscode-button-hoverBackground); }
		#node-list {
			overflow-y: auto;
			padding: 8px;
			flex: 1;
		}
		.node-item {
			display: flex;
			align-items: center;
			gap: 6px;
			padding: 2px 0;
			cursor: pointer;
			word-break: break-all;
		}
		#cy {
			flex: 1;
			height: 100%;
		}
		#context-menu {
			position: absolute;
			display: none;
			z-index: 1000;
			min-width: 120px;
			padding: 4px 0;
			background-color: var(--vscode-menu-background, var(--vscode-editor-background));
			color: var(--vscode-menu-foreground, var(--vscode-editor-foreground));
			border: 1px solid var(--vscode-menu-border, var(--vscode-panel-border, #444));
			box-shadow: 0 2px 8px rgba(0, 0, 0, 0.4);
		}
		#context-menu .menu-item {
			padding: 4px 12px;
			cursor: pointer;
			white-space: nowrap;
		}
		#context-menu .menu-item:hover {
			background-color: var(--vscode-menu-selectionBackground, var(--vscode-list-hoverBackground));
			color: var(--vscode-menu-selectionForeground, var(--vscode-menu-foreground));
		}
	</style>
</head>
<body>
	<div id="sidebar">
		<div id="toolbar">
			<button id="show-all">Show all</button>
			<button id="hide-all">Hide all</button>
			<button id="fit">Fit</button>
		</div>
		<div id="node-list"></div>
	</div>
	<div id="cy"></div>
	<div id="context-menu">
		<div class="menu-item" id="context-menu-hide">Hide node</div>
	</div>
	<script nonce="${nonce}" src="${cytoscape_uri}"></script>
	<script nonce="${nonce}" src="${script_uri}"></script>
</body>
</html>`;
}

function get_nonce(): string {
	const characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
	let text = '';
	for (let i = 0; i < 32; i++) {
		text += characters.charAt(Math.floor(Math.random() * characters.length));
	}
	return text;
}
