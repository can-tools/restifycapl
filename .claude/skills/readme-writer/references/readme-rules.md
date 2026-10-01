# README.md Authoring Rules (generic, version 2)

Sources: GitHub Docs (About READMEs), Standard Readme (spec), Make a README, awesome-readme (including its Architecture Examples section).

## 1. Purpose

A README answers these questions (GitHub Docs):
1. What does the project do?
2. Why is it useful?
3. How do I get started?
4. Where can I get help?
5. Who maintains and develops it?

It should also state the license and the project status.

## 2. File and Location

- Name: `README.md`.
- Location: repository root. GitHub also recognizes `.github/` and `docs/`. If several README files exist, GitHub shows the first one in this order: `.github/`, root, `docs/`.
- Size limit: GitHub truncates content above 500 KiB.
- Language versions: `README.md` is English; other languages use `README.pl.md` and similar (BCP 47 tag). If there is only one file, it may be in any language.
- The title should match the repository, directory, or package name. If it differs, explain why.

## 3. Core Principles

- Write for someone seeing the project for the first time.
- Layer the content: a short description at the top, then a quick start, then details. The reader gets more depth as they scroll.
- Completeness matters more than brevity. If the README grows too long, move details to separate documents (wiki, `docs/`, `ARCHITECTURE.md`) and link to them instead of cutting information.
- The README contains what is needed to start using and contributing to the project. Everything else belongs in separate documentation.
- Do not invent anything. Every command, path, variable, and dependency must exist in the repository. Mark gaps as `TODO`.
- Verify all instructions on a clean environment.
- No dead links. Code examples in the README are checked like the rest of the code (lint, ideally a test in CI).
- A human writes or approves the description. A generator provides a skeleton, not the final content.
- Update the README in the same PR as the code change.

## 4. Structure and Order

The order follows Standard Readme, extended with sections from Make a README.

| # | Section | Status | Notes |
|---|---------|--------|-------|
| 1 | Title | Required | Matches repo/package name |
| 2 | Banner / logo | Optional | Directly below the title; local file in the repo |
| 3 | Badges | Optional | Only meaningful ones: CI, version, license, coverage |
| 4 | Short description | Required | One line, under 120 characters, identical to the GitHub repo description |
| 5 | Project status | Conditional | At the top if the project is paused, archived, or experimental |
| 6 | Long description / Features | Recommended | A few paragraphs or 3–6 bullets |
| 7 | Visuals | Recommended for UI/CLI | Screenshot, GIF, diagram |
| 8 | Table of contents | Required from ~100 lines | Covers all `##` headings |
| 9 | Background | Optional | Motivation, alternatives, how it differs |
| 10 | Security | Optional | When relevant to users |
| 11 | Install (Requirements, Dependencies) | Required | Code block; manual requirements and dependencies in a subsection |
| 12 | Usage | Required | Smallest working example plus expected output |
| 13 | Additional sections | Optional | Configuration, testing, project structure, architecture |
| 14 | API | Optional | Or a link to generated documentation |
| 15 | Roadmap | Optional | Planned directions |
| 16 | Support | Recommended | Issues, chat, email |
| 17 | Maintainers / Credits | Optional | Responsible people, acknowledgements |
| 18 | Contributing | Required | Where to ask, whether PRs are accepted, requirements; link to `CONTRIBUTING.md` |
| 19 | License | Required | Last section; SPDX identifier, owner, link to `LICENSE` |

## 5. Section Details

### Install
- A code block with commands. Assume the reader is a beginner and give concrete steps.
- If it only works in a specific context (language version, OS), add a Requirements subsection.
- Add an Updating subsection if users may have several versions.

### Usage
- Put the smallest example in the README; longer ones go in separate files or `examples/`.
- Show the expected output.
- For a CLI: an example invocation. For a library: import and usage.
- If two version lines exist (for example v1 and v2), state which one applies at the start.

### Configuration (if applicable)
A table: name, description, default value, required?

### Testing / Development
How to run tests and the linter, and what is needed (for example external services). Hints for people changing the code.

### Architecture
A short description in the README (diagram, main flows). For larger projects, use a separate `ARCHITECTURE.md` that contains:
- a high-level diagram,
- a code map (directories and key modules),
- invariants and design decisions,
- descriptions of the main flows (startup, lifecycle).

### Background
Motivation, conceptual dependencies, alternatives, and differences from them. A small comparison table works well.

### Contributing
Whether you accept PRs, how to report bugs, requirements (for example sign-off), and a link to the Code of Conduct.

## 6. Formatting (GitHub Flavored Markdown)

- One `#` (title), then `##`, `###` without skipping levels.
- Code blocks with a language tag; commands without a `$` prefix.
- Relative links (`docs/CONTRIBUTING.md`). GitHub adapts them to the branch, and they work in clones. Keep link text on a single line.
- Tables for configuration and comparisons.
- Diagrams in Mermaid (rendered by GitHub).
- Collapsible sections (`<details>`) for long examples or installation variants.
- Admonitions (`> [!NOTE]`) only for genuinely important remarks.
- Images with alt text, stored in the repo (for example `docs/img/`).
- Emoji and decoration are a matter of style. They do not replace content.
- GitHub generates a table of contents itself (Outline menu). A manual one is useful but does not replace good headings.

## 7. Companion Files

| File | Purpose |
|------|---------|
| `LICENSE` | Terms of use |
| `CONTRIBUTING.md` | Contribution rules; GitHub links it when creating issues/PRs |
| `CODE_OF_CONDUCT.md` | Collaboration rules |
| `SECURITY.md` | Vulnerability reporting |
| `CHANGELOG.md` | Change history |
| `ARCHITECTURE.md` | Code map, decisions, invariants |
| Issue/PR templates | Better reports |

## 8. Checklist

- [ ] Description is one line (<120 characters) and matches the repo description.
- [ ] After the first two sentences it is clear what the project is and who it is for.
- [ ] Install and Usage verified on a clean environment, with expected output.
- [ ] No invented commands, paths, or variables.
- [ ] All links work; internal ones are relative.
- [ ] Code blocks have a language tag; examples are tested.
- [ ] Contributing, Support, and License are present; License is last.
- [ ] Project status is stated if the project is not active.
- [ ] No secrets, API keys, or local paths.
- [ ] No leftover `TODO` markers.
- [ ] Long content moved to separate documents and linked.
- [ ] Only badges that carry information.

## 9. Anti-patterns

- Instructions that only work on the author's machine.
- A wall of text with no headings or examples.
- Copying CHANGELOG or CONTRIBUTING into the README.
- Outdated badges and dead links.
- Marketing language instead of specifics; an unverified generated description.
- No status information for a project that is not maintained.
- A screenshot where a copyable command is needed.
