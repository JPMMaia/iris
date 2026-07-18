// Webview client script for the Iris module dependency graph.
// Receives { nodes, edges } from the extension and renders it with Cytoscape,
// with zoom/pan and a sidebar to hide/unhide individual nodes (and their edges).
(function () {
    const vscode = acquireVsCodeApi();

    let cy = undefined;
    // Maps a node id to its sidebar checkbox so hiding from other places
    // (e.g. the right-click menu) can keep the checkbox in sync.
    const checkbox_by_id = new Map();

    // Compute a dependency-depth rank for every node so the layout can be laid
    // out in layers: a node with no dependencies gets rank 0 (bottom layer), and
    // every other node sits one layer above its deepest dependency.
    // Edges point importer -> imported, so a node's dependencies are its edge
    // targets. Cycles are guarded by treating in-progress nodes as rank 0.
    function compute_ranks(graph) {
        const dependencies_by_id = new Map();
        for (const node of graph.nodes) {
            dependencies_by_id.set(node.id, []);
        }
        for (const edge of graph.edges) {
            const list = dependencies_by_id.get(edge.from);
            if (list !== undefined && dependencies_by_id.has(edge.to)) {
                list.push(edge.to);
            }
        }

        const rank = new Map();
        const state = new Map(); // undefined = unvisited, 1 = in progress, 2 = done

        function visit(id) {
            const current_state = state.get(id);
            if (current_state === 2) {
                return rank.get(id);
            }
            if (current_state === 1) {
                return 0; // back edge (cycle) -> do not let it increase the rank
            }

            state.set(id, 1);
            let result = 0;
            for (const dependency of dependencies_by_id.get(id)) {
                result = Math.max(result, 1 + visit(dependency));
            }
            state.set(id, 2);
            rank.set(id, result);
            return result;
        }

        for (const node of graph.nodes) {
            visit(node.id);
        }

        return rank;
    }

    // Assign preset positions from the ranks: nodes are grouped into horizontal
    // layers (rank 0 at the bottom) and spread out evenly within each layer.
    function compute_positions(graph, ranks) {
        const ids_by_rank = new Map();
        let max_rank = 0;
        for (const node of graph.nodes) {
            const node_rank = ranks.get(node.id) ?? 0;
            max_rank = Math.max(max_rank, node_rank);
            if (!ids_by_rank.has(node_rank)) {
                ids_by_rank.set(node_rank, []);
            }
            ids_by_rank.get(node_rank).push(node.id);
        }

        const x_spacing = 200;
        const y_spacing = 130;
        const positions = new Map();

        for (const [node_rank, ids] of ids_by_rank) {
            ids.sort((a, b) => a.localeCompare(b));
            const count = ids.length;
            ids.forEach((id, index) => {
                positions.set(id, {
                    // Center each layer around x = 0.
                    x: (index - (count - 1) / 2) * x_spacing,
                    // Rank 0 (no dependencies) sits at the largest y (bottom).
                    y: (max_rank - node_rank) * y_spacing,
                });
            });
        }

        return positions;
    }

    function build_elements(graph, positions) {
        const elements = [];

        for (const node of graph.nodes) {
            elements.push({
                data: {
                    id: node.id,
                    label: node.label,
                    external: node.external === true,
                    filePath: node.filePath,
                },
                position: positions.get(node.id),
            });
        }

        for (const edge of graph.edges) {
            elements.push({
                data: {
                    id: 'edge:' + edge.from + '->' + edge.to,
                    source: edge.from,
                    target: edge.to,
                },
            });
        }

        return elements;
    }

    function render(graph) {
        const container = document.getElementById('cy');

        const ranks = compute_ranks(graph);
        const positions = compute_positions(graph, ranks);

        cy = cytoscape({
            container: container,
            elements: build_elements(graph, positions),
            wheelSensitivity: 0.2,
            style: [
                {
                    selector: 'node',
                    style: {
                        'label': 'data(label)',
                        'font-size': 10,
                        'color': getComputedStyle(document.body).getPropertyValue('--vscode-editor-foreground'),
                        'text-valign': 'center',
                        'text-halign': 'center',
                        'text-wrap': 'wrap',
                        'text-max-width': 120,
                        'background-color': '#4c8bf5',
                        'width': 'label',
                        'height': 'label',
                        'padding': 8,
                        'shape': 'round-rectangle',
                    },
                },
                {
                    selector: 'node[?external]',
                    style: {
                        'background-color': '#9aa0a6',
                        'border-width': 1,
                        'border-style': 'dashed',
                        'border-color': '#5f6368',
                    },
                },
                {
                    selector: 'edge',
                    style: {
                        'width': 1.5,
                        'line-color': '#888',
                        'target-arrow-color': '#888',
                        'target-arrow-shape': 'triangle',
                        'curve-style': 'bezier',
                        'arrow-scale': 0.9,
                    },
                },
                {
                    selector: '.hidden',
                    style: { 'display': 'none' },
                },
            ],
            // Positions are computed above (dependency-depth layers); 'preset'
            // just places nodes at those coordinates.
            layout: {
                name: 'preset',
                padding: 30,
                fit: true,
            },
        });

        build_sidebar(graph);
        wire_context_menu();
    }

    function set_node_hidden(id, hidden) {
        const node = cy.getElementById(id);
        if (node.empty()) {
            return;
        }
        // Hiding a node also hides its connected edges (an edge is only shown when
        // both endpoints are visible).
        if (hidden) {
            node.addClass('hidden');
        } else {
            node.removeClass('hidden');
        }

        // Keep the sidebar checkbox in sync (checked == visible).
        const checkbox = checkbox_by_id.get(id);
        if (checkbox !== undefined) {
            checkbox.checked = !hidden;
        }
    }

    function build_sidebar(graph) {
        const list = document.getElementById('node-list');
        list.replaceChildren();
        checkbox_by_id.clear();

        const sorted = [...graph.nodes].sort((a, b) => a.label.localeCompare(b.label));

        for (const node of sorted) {
            const item = document.createElement('label');
            item.className = 'node-item';

            const checkbox = document.createElement('input');
            checkbox.type = 'checkbox';
            checkbox.checked = true;
            checkbox.addEventListener('change', () => {
                set_node_hidden(node.id, !checkbox.checked);
            });
            checkbox_by_id.set(node.id, checkbox);

            const text = document.createElement('span');
            text.textContent = node.label + (node.external ? ' (external)' : '');

            item.appendChild(checkbox);
            item.appendChild(text);
            list.appendChild(item);
        }
    }

    // Right-click a node to get a small dropdown menu that can hide it.
    function wire_context_menu() {
        const menu = document.getElementById('context-menu');
        let target_id = undefined;

        const hide_menu = () => {
            menu.style.display = 'none';
            target_id = undefined;
        };

        // Suppress the webview's native context menu (copy/cut/paste) so only our
        // custom dropdown appears on right-click.
        document.getElementById('cy').addEventListener('contextmenu', (event) => {
            event.preventDefault();
        });

        cy.on('cxttap', 'node', (event) => {
            target_id = event.target.id();
            const original_event = event.originalEvent;
            menu.style.left = original_event.clientX + 'px';
            menu.style.top = original_event.clientY + 'px';
            menu.style.display = 'block';
        });

        // Dismiss the menu on any interaction elsewhere.
        cy.on('tap', hide_menu);
        cy.on('pan zoom', hide_menu);
        window.addEventListener('blur', hide_menu);
        document.addEventListener('mousedown', (event) => {
            if (!menu.contains(event.target)) {
                hide_menu();
            }
        });

        document.getElementById('context-menu-hide').addEventListener('click', () => {
            if (target_id !== undefined) {
                set_node_hidden(target_id, true);
            }
            hide_menu();
        });
    }

    function set_all_checkboxes(checked) {
        const checkboxes = document.querySelectorAll('#node-list input[type=checkbox]');
        checkboxes.forEach((checkbox) => {
            checkbox.checked = checked;
        });
    }

    document.getElementById('show-all').addEventListener('click', () => {
        set_all_checkboxes(true);
        cy.nodes().removeClass('hidden');
    });

    document.getElementById('hide-all').addEventListener('click', () => {
        set_all_checkboxes(false);
        cy.nodes().addClass('hidden');
    });

    document.getElementById('fit').addEventListener('click', () => {
        cy.fit(undefined, 30);
    });

    window.addEventListener('message', (event) => {
        const message = event.data;
        if (message.type === 'setGraph') {
            render(message.graph);
        }
    });

    // Ask the extension for the graph data now that the webview is ready.
    vscode.postMessage({ type: 'ready' });
})();
