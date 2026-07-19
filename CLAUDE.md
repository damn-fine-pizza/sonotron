# Sonotron — Claude Code Instructions

## Tool priority

Use tools in this order when appropriate:

1. `codebase-memory-mcp` for repository architecture and symbol relationships;
2. direct source inspection for exact implementation details;
3. `ast-grep` for syntax-aware C++ searches and transformations;
4. Bash commands, automatically optimized by RTK.

Use the smallest tool capable of answering the current question.

---

## Codebase Memory MCP

Use `codebase-memory-mcp` before broad repository exploration.

### Main purposes

Use it for:

* understanding repository architecture;
* locating classes, functions, modules, and concepts;
* finding callers and callees;
* tracing dependencies and execution paths;
* understanding relationships between components;
* identifying affected files and symbols;
* estimating the blast radius of a change;
* detecting changes relative to the indexed graph.

### Preferred workflow

1. Verify that the current repository is indexed.
2. Obtain the relevant architectural context.
3. Search for the requested concept or symbol.
4. Inspect callers, callees, dependencies, and paths.
5. Identify the smallest relevant set of files.
6. Read exact source code only where implementation details are required.
7. Verify critical information against the current source before editing.
8. Check affected symbols again after significant changes.

### Functions

Use the available MCP functions according to their purpose:

* `list_projects`: list indexed repositories;
* index-status functions: verify whether the repository is indexed and current;
* `index_repository`: index the current repository when necessary;
* `get_architecture`: obtain a high-level architectural overview;
* `search_graph`: search symbols, concepts, components, and relationships;
* `get_code_snippet`: retrieve focused code associated with graph results;
* caller/callee functions: inspect incoming and outgoing relationships;
* `trace_path`: trace relationships or execution paths between symbols;
* dependency functions: inspect dependencies and dependents;
* `detect_changes`: compare the current repository with the indexed graph;
* ADR functions: inspect or manage architectural decisions when relevant.

Function names may vary by installed version. Select the corresponding available tool by its description.

### Rules

* Use codebase-memory before broad `grep`, `rg`, `find`, `Glob`, or recursive file reads.
* Do not reconstruct architecture by reading the repository file by file.
* Do not repeat searches already answered by the graph.
* Treat graph results as navigation and architectural context, not final proof.
* Verify signatures, implementations, constants, templates, macros, and configuration against current files.
* Use direct reads for generated files, documentation, build files, configuration, complex templates, and code not represented accurately by the graph.
* Check index freshness when graph results conflict with the source.
* Reindex only when necessary.
* Do not request a complete architectural overview for every small task.

Recommended sequence:

```text
index status
→ graph search
→ symbol relationships
→ affected files
→ exact source inspection
→ implementation
→ validation
```

---

## ast-grep

`ast-grep` is installed as a CLI and Claude skill.

Use it when the query depends on C++ syntax or structure rather than literal text.

### Use it for

* finding function calls with a specific argument structure;
* locating classes derived from a particular base;
* finding declarations with specific modifiers;
* identifying missing `override`, `const`, or qualifiers;
* locating constructors or member initializers;
* finding deprecated or unsafe constructs;
* matching expressions inside a specific syntactic context;
* performing repetitive syntax-aware transformations.

### Rules

* Prefer ast-grep over text search for structural C++ queries.
* Prefer ordinary text search for strings, filenames, logs, comments, documentation, and configuration.
* Search and inspect matches before applying rewrites.
* Limit searches and transformations to relevant directories.
* Do not perform broad automatic rewrites without first reviewing representative matches.
* Inspect the resulting diff after every structural transformation.
* Run formatting, build, and relevant tests after rewrites.
* Do not use ast-grep as a substitute for architectural analysis.
* Use codebase-memory first when callers, dependencies, ownership, or blast radius matter.

---

## RTK

RTK is installed through a Claude Code `PreToolUse` hook for Bash commands.

Write normal shell commands. The hook automatically rewrites supported commands at execution time.

### Rules

* Do not manually prefix normal commands with `rtk`.
* Let the hook optimize supported search, file-reading, Git, GitHub, process, and system commands.
* Prefer narrow paths, focused patterns, specific targets, test filters, and limited output.
* Avoid dumping complete repositories, generated files, process lists, or large logs.
* Use codebase-memory instead of broad shell exploration when investigating architecture.
* Preserve compiler errors, failed assertions, stack traces, and command exit codes.
* Do not hide command failures with unconditional output redirection.
* If compressed output omits necessary information, narrow the command or inspect the corresponding RTK tee output.
* Do not disable RTK globally because one command needs fuller output.
* Use `rtk gain` to inspect runtime savings.
* Treat `rtk discover` as analysis of authored command history; it may not observe commands rewritten later by the hook.
* RTK output reduction is not evidence that a build, test, or change is correct.
* Current source code, real compiler output, and test results remain authoritative.

---

## Division of responsibility

```text
codebase-memory-mcp
    Architecture, symbols, dependencies, callers,
    callees, execution paths, and blast radius.

ast-grep
    Structural C++ searches and syntax-aware transformations.

RTK
    Compact execution of supported Bash commands and output.
```

Do not use all three tools mechanically. Choose them according to the information required by the task.

## Agent routing

Custom agents are available under `.claude/agents/`.

For every substantial task, determine whether one specialized agent owns the work. Delegate to that agent instead of performing the specialist work directly.

Do not invoke agents merely to add ceremony. Handle trivial lookups and very small, obvious edits directly.

### Primary routing

| Need                                                                                             | Agent                             |
| ------------------------------------------------------------------------------------------------ | --------------------------------- |
| Implement a feature, refactor, or fix product code                                               | `nazzareno-cpp-implementor`       |
| Own test strategy, write/run functional, integration, fuzz, sanitizer, golden, or coverage tests | `torquato-qa-lead`                |
| General C++ review: correctness, bugs, UB, ownership, concurrency, idioms                        | `fabrizio-bofh-cpp`               |
| Readability and performance-vs-clarity review                                                    | `clementi-craftsmanship-critic`   |
| Existing logical architecture, coupling, ABI, layering, dual-target seams                        | `corelli-architecture-critic`     |
| Repository layout, file placement, naming, moves, splits, and physical structure                 | `palladio-structure-steward`      |
| Whole-tree C++ modernization, allocation, performance, language-standard and toolchain survey    | `paganini-modernization-surveyor` |
| Judge a proposed direction or concept before implementation                                      | `prospero-reflection-critic`      |
| Analyze musical style corpus and generative feasibility                                          | `ottorino-style-analyst`          |
| Judge the built product’s musical value, originality, utility, and real use cases                | `puccini-product-critic`          |
| Evaluate the complete roadmap, sequencing, priorities, deferrals, and cuts                       | `verdi-roadmap-strategist`        |
| Improve requirements capture, acceptance criteria, definition-of-done, and QA process            | `guido-process-analyst`           |
| Correct, consolidate, translate, or verify documentation                                         | `saverio-doc-steward`             |
| Reconcile reported project status with the repository’s actual state                             | `vasari-status-steward`           |
| Design a new custom agent through requirements elicitation                                       | `epistaffo`                       |
| Explicitly wait for a stated number of minutes                                                   | `clessidra-timer`                 |

### Subordinate agents

Do not normally invoke subordinate agents directly. Route work through their owning lead.

| Subordinate                | Owner and permitted use                                                                           |
| -------------------------- | ------------------------------------------------------------------------------------------------- |
| `taddeo-cpp-apprentice`    | Nazzareno: exact, mechanical, bounded C++ work                                                    |
| `filippino-cpp-journeyman` | Nazzareno: bounded but substantial C++ implementation slice                                       |
| `cennino-ut-scribe`        | Torquato: precisely specified unit-test implementation                                            |
| `celestino-corpus-hand`    | Ottorino: exact corpus measurement commands with raw output only                                  |
| `benedetto-golden-hand`    | Authorized senior: mechanical regeneration of an explicit golden-file list with required sign-off |
| `figaro`                   | Exact mechanical task or read-only evidence gathering with no judgment                            |

The owning lead remains responsible for:

* defining the subordinate’s exact scope;
* assigning disjoint files and a worktree when parallel edits could collide;
* reviewing the returned work;
* independently verifying build and test claims;
* producing the final response to the user.

Never ask a subordinate to determine scope, architecture, expected behavior, test strategy, or product direction.

### Selection rules

Use exactly one primary owner when a task has one dominant purpose.

Examples:

* A bug fix belongs to Nazzareno, even when tests are required for validation.
* A request to design tests belongs to Torquato, even when a product bug may be exposed.
* A review request belongs to Fabrizio unless it explicitly targets architecture, physical layout, or craftsmanship.
* A proposed idea belongs to Prospero; an already-built architecture belongs to Corelli.
* Corpus behavior belongs to Ottorino; musician value and product fit belong to Puccini.
* One proposed direction belongs to Prospero; the entire roadmap belongs to Verdi.
* Broken acceptance and requirements workflow belongs to Guido; actual tests belong to Torquato.

When a task spans independent domains, delegate separate read-only analyses in parallel and synthesize their results. Do not launch multiple mutating agents against overlapping files.

### Delegation protocol

Before invoking an agent, provide a self-contained task containing:

1. objective;
2. exact scope and relevant paths;
3. expected behavior or question to answer;
4. acceptance criteria;
5. constraints and forbidden changes;
6. required verification;
7. expected report format;
8. worktree and file boundary when concurrency is involved.

Do not send vague instructions such as “investigate and fix everything.”

### Mandatory use

Delegate when:

* the request explicitly matches a specialist’s domain;
* implementation or analysis spans multiple files or components;
* architectural, testing, product, musical, or roadmap judgment is required;
* a substantial task can be decomposed into independent bounded slices.

Do not silently perform specialist work in the top-level agent when a matching custom agent exists.

### Reporting

In the final response, state:

* which agent or agents were used;
* why each was selected;
* what each returned;
* what was independently verified;
* any disagreement, rejection, or blocked condition.

The top-level agent owns routing and synthesis. Specialist agents own only their declared domain.
