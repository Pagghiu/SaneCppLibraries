"""
Module for generating interactive HTML dependency graph using vis.js.
"""
import os
import json
import config


def write_interactive_html(project_root, libraries, dep_map, minimal_map, transitive_map, reverse_map, rank_map, ranks):
    """
    Generates interactive HTML file for dependency graph.
    """
    output_path = os.path.join(project_root, config.OUTPUT_HTML)
    # Create output directory if it doesn't exist
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    # Prepare data for JavaScript
    nodes = []
    edges = []

    # Create nodes with ranks
    for lib in sorted(libraries):
        nodes.append({
            'id': lib,
            'label': lib,
            'level': rank_map[lib]
        })

    # Create edges (from lib to its minimal dependencies)
    for lib in sorted(minimal_map):
        for dep in sorted(minimal_map[lib]):
            edges.append({
                'from': lib,
                'to': dep,
                'arrows': 'to'
            })

    # Prepare reverse dependencies
    reverse_deps = {lib: sorted(list(deps)) for lib, deps in reverse_map.items()}

    # Prepare transitive dependencies
    transitive_deps = {lib: sorted(list(deps)) for lib, deps in transitive_map.items()}

    # Generate embeddable HTML fragment
    html_content = f'''<div id="dependency-graph-container">
    <style>
        #dependency-network {{
            width: 100%;
            height: 600px;
            border: 1px solid lightgray;
        }}
        .dependency-controls {{
            margin-bottom: 10px;
        }}
        .dependency-legend {{
            margin-top: 10px;
            font-size: 12px;
        }}
        .dependency-legend-item {{
            display: inline-block;
            margin-right: 20px;
        }}
    </style>
    <script src="https://unpkg.com/vis-network@9.1.2/standalone/umd/vis-network.min.js"></script>
    <div class="dependency-controls">
        <button id="clearSelection">Clear Selection</button>
        <span id="selectedLibs"></span>
    </div>
    <div id="dependency-network"></div>
    <div class="dependency-legend">
        <div class="dependency-legend-item"><span style="background-color: #FFD700; color: black; padding: 2px 4px; border-radius: 3px;">Selected</span> Selected Libraries</div>
        <div class="dependency-legend-item"><span style="background-color: #FF6B6B; color: black; padding: 2px 4px; border-radius: 3px;">Deps</span> Dependencies</div>
        <div class="dependency-legend-item"><span style="background-color: #4ECDC4; color: black; padding: 2px 4px; border-radius: 3px;">Deps</span> Dependents</div>
        <div class="dependency-legend-item" style="margin-top: 8px; font-style: italic;">Click to select, Ctrl+Click to multi-select</div>
    </div>
    <script>
        (function() {{
            // Data
            const nodesData = {json.dumps(nodes)};
            const edgesData = {json.dumps(edges)};
            const reverseDeps = {json.dumps(reverse_deps)};
            const transitiveDeps = {json.dumps(transitive_deps)};

            // Create network
            const container = document.getElementById('dependency-network');
            const data = {{
                nodes: new vis.DataSet(nodesData),
                edges: new vis.DataSet(edgesData)
            }};

            const options = {{
                layout: {{
                    hierarchical: {{
                        direction: 'UD',
                        sortMethod: 'directed',
                        levelSeparation: 150,
                        nodeSpacing: 100
                    }}
                }},
                physics: false,
                interaction: {{
                    multiselect: false,
                    selectConnectedEdges: false,
                    zoomView: false
                }},
                nodes: {{
                    shape: 'box',
                    font: {{
                        size: 14
                    }}
                }},
                edges: {{
                    arrows: {{
                        to: {{
                            enabled: true,
                            scaleFactor: 0.5
                        }}
                    }}
                }}
            }};

            const network = new vis.Network(container, data, options);

            let selectedNodes = new Set();

            function updateHighlighting() {{
                const allNodeIds = data.nodes.getIds();
                const allEdgeIds = data.edges.getIds();
                const nodeUpdates = [];
                const edgeUpdates = [];

                if (selectedNodes.size === 0) {{
                    allNodeIds.forEach(id => {{
                        nodeUpdates.push({{
                            id: id,
                            color: null, // Use null to reset to default color
                            font: {{ color: 'black' }}
                        }});
                    }});
                    allEdgeIds.forEach(id => {{
                        edgeUpdates.push({{
                            id: id,
                            color: null // Use null to reset to default color
                        }});
                    }});
                    data.nodes.update(nodeUpdates);
                    data.edges.update(edgeUpdates);
                    document.getElementById('selectedLibs').textContent = '';
                    return;
                }}

                const dependencies = new Set();
                const dependents = new Set();

                selectedNodes.forEach(lib => {{
                    if (transitiveDeps[lib]) {{
                        transitiveDeps[lib].forEach(dep => dependencies.add(dep));
                    }}
                    if (reverseDeps[lib]) {{
                        reverseDeps[lib].forEach(dep => dependents.add(dep));
                    }}
                }});

                allNodeIds.forEach(id => {{
                    let color = null; // Default color
                    if (selectedNodes.has(id)) {{
                        color = {{ background: '#FFD700', border: '#B8860B' }};
                    }} else if (dependencies.has(id)) {{
                        color = {{ background: '#FF6B6B', border: '#CC5555' }};
                    }} else if (dependents.has(id)) {{
                        color = {{ background: '#4ECDC4', border: '#3A9B94' }};
                    }}
                    nodeUpdates.push({{ id: id, color: color }});
                }});

                allEdgeIds.forEach(id => {{
                    const edge = data.edges.get(id);
                    let color = null; // Default color
                    if (selectedNodes.has(edge.from) && dependencies.has(edge.to)) {{
                        color = '#FF6B6B';
                    }} else if (dependents.has(edge.from) && selectedNodes.has(edge.to)) {{
                        color = '#4ECDC4';
                    }}
                    edgeUpdates.push({{ id: id, color: color }});
                }});

                data.nodes.update(nodeUpdates);
                data.edges.update(edgeUpdates);

                document.getElementById('selectedLibs').textContent = 'Selected: ' + Array.from(selectedNodes).join(', ');
            }}

            // Custom click handling
            network.on('click', function(params) {{
                if (params.nodes.length > 0) {{
                    const clickedNode = params.nodes[0];
                    if (params.event.srcEvent.ctrlKey || params.event.srcEvent.metaKey) {{
                        // Ctrl+click: toggle selection
                        if (selectedNodes.has(clickedNode)) {{
                            selectedNodes.delete(clickedNode);
                        }} else {{
                            selectedNodes.add(clickedNode);
                        }}
                    }} else {{
                        // Normal click: reset selection to only this node
                        selectedNodes.clear();
                        selectedNodes.add(clickedNode);
                    }}
                    network.unselectAll();
                    updateHighlighting();
                }} else {{
                    // Click on empty space: clear selection
                    selectedNodes.clear();
                    network.unselectAll();
                    updateHighlighting();
                }}
            }});

            document.getElementById('clearSelection').addEventListener('click', function() {{
                selectedNodes.clear();
                network.unselectAll();
                updateHighlighting();
            }});
        }})();
    </script>
</div>'''

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(html_content)

    print(f"Interactive HTML file written to {output_path}")
