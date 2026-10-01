---
name: readme-writer
description: Creates or improves README.md files for GitHub repositories by following the project's own README rules. Use when the user asks to write, generate, review, refactor, or audit a README.md, or to prepare a repository for publication on GitHub.
---

# README Writer

The single source of truth for README rules is `references/readme-rules.md` in this skill's directory. This file only describes the workflow. Do not copy or paraphrase the rules from memory: they change, so always read the file.

## Workflow

1. **Load the rules.** Read `references/readme-rules.md` in full before doing anything else. If the file is missing or unreadable, stop and tell the user. Do not fall back to generic knowledge.
2. **Inspect the repository.** Collect facts only from the repo: language and runtime versions, dependency manifests (`pyproject.toml`, `package.json`, `requirements*.txt`, etc.), entry points, scripts, Dockerfiles, CI workflows, test setup, environment variables, license, existing docs, and any existing `README.md`.
3. **Choose the mode.**
   - No README exists: create one.
   - README exists: audit it against the rules first, list the gaps, then propose changes. Preserve accurate content and the author's voice.
4. **Draft.** Follow the section order and required/optional statuses in the rules file. Include optional sections only when the repo has material for them. Write in English unless the user asks otherwise.
5. **Verify.** Check every command, path, variable, and link in the draft against the repository. Mark anything you cannot confirm as `TODO` and list these items for the user. Never invent commands, badges, URLs, or features.
6. **Run the checklist.** Go through the checklist and anti-patterns in the rules file and report which items pass, fail, or need user input.

## Output

- For a new README: the complete `README.md` file.
- For an existing README: a short list of gaps found, then the complete revised file with explicit changes.
- Always end with a list of `TODO` items and assumptions that need the user's confirmation.

## Constraints

- Do not put secrets, tokens, or local absolute paths in the README.
- Do not duplicate content that belongs in companion files (`CONTRIBUTING.md`, `CHANGELOG.md`, `SECURITY.md`, `ARCHITECTURE.md`); link to them and suggest creating them if missing.
- If the rules file conflicts with the user's explicit instruction in the current conversation, follow the user and note the deviation.
