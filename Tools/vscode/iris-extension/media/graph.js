// Webview client script for the Iris module dependency graph.
// Receives { nodes, edges } from the extension and renders it with Cytoscape,
// with zoom/pan and a sidebar to hide/unhide individual nodes (and their edges).
(function () {
    const vscode = acquireVsCodeApi();

    let cy = undefined;
    let resize_observer = undefined;
    // The node the right-click menu currently acts on, and whether the menu's
    // document-level listeners have already been installed.
    let context_menu_target_id = undefined;
    let context_menu_wired = false;
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

    // Node geometry. Sizes are measured here rather than left to cytoscape's
    // `width: label` because that property is deprecated and inherently circular: the
    // size depends on a label that is only measured while rendering, so the first pass
    // sees zero-sized nodes, and an edge whose endpoints take up no space is skipped by
    // `findEdgeControlPoints` and never gets the control points `drawEdge` needs.
    const NODE_FONT_SIZE = 10;
    const NODE_PADDING = 8;
    const NODE_MAX_TEXT_WIDTH = 120;
    const NODE_LINE_HEIGHT = 1.2;
    const NODE_MIN_WIDTH = 24;

    let measure_canvas_context = undefined;

    function node_font_family() {
        const font_family = getComputedStyle(document.body).fontFamily;
        return font_family !== '' ? font_family : 'sans-serif';
    }

    // Mirrors 'text-wrap: wrap' with 'text-max-width': cytoscape only breaks on
    // whitespace, so labels without spaces (most module names) stay on one line.
    function measure_label(label) {
        if (measure_canvas_context === undefined) {
            measure_canvas_context = document.createElement('canvas').getContext('2d');
        }
        measure_canvas_context.font = NODE_FONT_SIZE + 'px ' + node_font_family();

        const lines = [];
        let current_line = '';
        for (const word of String(label).split(/\s+/)) {
            const candidate = current_line === '' ? word : current_line + ' ' + word;
            if (current_line !== '' && measure_canvas_context.measureText(candidate).width > NODE_MAX_TEXT_WIDTH) {
                lines.push(current_line);
                current_line = word;
            } else {
                current_line = candidate;
            }
        }
        lines.push(current_line);

        const text_width = Math.max(...lines.map((line) => measure_canvas_context.measureText(line).width));

        return {
            width: Math.max(Math.ceil(text_width), NODE_MIN_WIDTH) + 2 * NODE_PADDING,
            height: Math.ceil(lines.length * NODE_FONT_SIZE * NODE_LINE_HEIGHT) + 2 * NODE_PADDING,
        };
    }

    function build_elements(graph, positions) {
        const elements = [];

        for (const node of graph.nodes) {
            const size = measure_label(node.label);
            elements.push({
                data: {
                    id: node.id,
                    label: node.label,
                    external: node.external === true,
                    filePath: node.filePath,
                    width: size.width,
                    height: size.height,
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

    // Node sizes are derived from the label ('width'/'height' are 'label'), so laying
    // out before the font is available produces zero-sized nodes and degenerate edges.
    // Waiting for the font and for one frame of layout also guarantees the container
    // has its final size, which `fit` below depends on.
    async function render(graph) {
        try {
            await document.fonts.ready;
        } catch {
            // Not fatal: fall through and render with whatever metrics are available.
        }
        await new Promise((resolve) => requestAnimationFrame(resolve));

        render_now(graph);
    }

    function render_now(graph) {
        const container = document.getElementById('cy');

        // Re-rendering into a container that still holds a previous instance would stack
        // a second set of canvases on top of the old ones.
        if (cy !== undefined) {
            cy.destroy();
            cy = undefined;
        }

        const ranks = compute_ranks(graph);
        const positions = compute_positions(graph, ranks);

        cy = cytoscape({
            container: container,
            elements: build_elements(graph, positions),
            wheelSensitivity: 2.0,
            style: [
                {
                    selector: 'node',
                    style: {
                        'label': 'data(label)',
                        'font-size': NODE_FONT_SIZE,
                        // Must match what measure_label sizes the nodes with.
                        'font-family': node_font_family(),
                        'color': getComputedStyle(document.body).getPropertyValue('--vscode-editor-foreground'),
                        'text-valign': 'center',
                        'text-halign': 'center',
                        'text-wrap': 'wrap',
                        'text-max-width': NODE_MAX_TEXT_WIDTH,
                        'background-color': '#4c8bf5',
                        // Measured in build_elements; see the note there for why this is
                        // not cytoscape's deprecated 'label' sizing.
                        'width': 'data(width)',
                        'height': 'data(height)',
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

        refresh_caches();

        // The panel is often still being laid out when the graph arrives, so pick up the
        // final container size and frame the graph against it.
        cy.resize();
        cy.fit(undefined, 30);

        observe_container_size(container);

        build_sidebar(graph);
        wire_context_menu();
    }

    // Elements built before the first paint can end up with caches that describe an
    // earlier, incomplete state -- most importantly a bounding box cached as empty. The
    // renderer culls by bounding box, so edges with valid geometry and visibility still
    // never get drawn. Dropping both caches is what a class change does internally, and
    // is why toggling Hide all and Show all used to be the only way to see the edges.
    function refresh_caches() {
        const refresh = () => {
            cy.elements().dirtyStyleCache();
            cy.elements().dirtyBoundingBoxCache();

            // `false` disables the cache; without it every element already marked clean
            // is skipped and the geometry is never recomputed.
            cy.elements().recalculateRenderedStyle(false);

            cy.forceRender();
        };

        refresh();
        cy.one('render', refresh);
    }

    // Cytoscape only tracks window resizes on its own, but the container also changes
    // size when the panel is revealed, split, or the sidebar is toggled.
    function observe_container_size(container) {
        if (resize_observer !== undefined) {
            return;
        }

        resize_observer = new ResizeObserver(() => {
            if (cy !== undefined) {
                cy.resize();
            }
        });
        resize_observer.observe(container);
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

        const hide_menu = () => {
            menu.style.display = 'none';
            context_menu_target_id = undefined;
        };

        // Listeners on the document and the window outlive the cytoscape instance, so
        // they are wired once instead of on every render.
        if (!context_menu_wired) {
            context_menu_wired = true;

            // Suppress the webview's native context menu (copy/cut/paste) so only our
            // custom dropdown appears on right-click. The listener runs in the capture
            // phase on the document so that it also covers the canvas that cytoscape
            // renders into.
            document.addEventListener('contextmenu', (event) => {
                event.preventDefault();
            }, true);

            window.addEventListener('blur', hide_menu);
            document.addEventListener('mousedown', (event) => {
                if (!menu.contains(event.target)) {
                    hide_menu();
                }
            });

            document.getElementById('context-menu-hide').addEventListener('click', () => {
                if (context_menu_target_id !== undefined) {
                    set_node_hidden(context_menu_target_id, true);
                }
                hide_menu();
            });
        }

        cy.on('cxttap', 'node', (event) => {
            context_menu_target_id = event.target.id();
            const original_event = event.originalEvent;
            menu.style.left = original_event.clientX + 'px';
            menu.style.top = original_event.clientY + 'px';
            menu.style.display = 'block';
        });

        // Dismiss the menu on any interaction elsewhere.
        cy.on('tap', hide_menu);
        cy.on('pan zoom', hide_menu);

        hide_menu();
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
