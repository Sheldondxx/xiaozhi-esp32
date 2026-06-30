# Agent Guidance

This project uses a graphify knowledge graph to make the codebase queryable.

## graphify

A pre-built knowledge graph lives in `graphify-out/`:

- `graphify-out/graph.json` — raw graph data
- `graphify-out/GRAPH_REPORT.md` — audit report with god nodes and community structure
- `graphify-out/graph.html` — interactive visualization

When answering questions about this codebase, **use the graph first**:

```bash
graphify query "<question>"
graphify path "<concept A>" "<concept B>"
graphify explain "<concept>"
```

Only fall back to raw file browsing or grep if the graph does not surface enough context.

After modifying source files, keep the graph current with:

```bash
graphify update .
```

This is AST-only and incurs no API cost.

## Project Context

See `CLAUDE.md` for the full project overview, build system, architecture, and contribution guidelines.
