# Work folders

Each unit of work (feature, fix, investigation) gets its own subfolder
here, named with a short kebab-case slug:

```
docs/work/<slug>/
├── plans/     saved via plan-writer (using the save-plan skill)
├── reviews/   persisted code-reviewer output, if kept
└── notes/     anything else worth keeping
```

See `.claude/agents/plan-writer.md` and `.claude/skills/save-plan/`.
