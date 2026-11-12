# GakuMotion

- Use `build.ps1` for local Runtime, NativeAOT and synthetic-contract builds.
- Ordinary source edits and builds do not require a snapshot approval status.
- Retain dependency licenses and attribution notices. Keep source and parameter provenance accurate.
- Do not deploy, control the game, access real user motions/models/captures, publish a Release or change remote repositories unless the current user request authorizes that action.
- Keep user data and unrelated working changes intact. Use synthetic inputs for isolated tests.
- Record build/test results separately from game loading and visual acceptance.
- Make local Git checkpoints after validating the requested changes.

This repository has no automatic game-deployment step.

## Git commit and pull request conventions

- Use English Conventional Commits for all new commit messages and PR titles.
- Write the type, scope, subject, commit body and footers in English. Preserve original names or quoted source messages only when required for attribution.
- Format: `type(scope): imperative English summary`. Use `!` and a `BREAKING CHANGE:` footer for breaking changes.
- Allowed types: `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `build`, `ci`, `chore`, `style`, `revert`.
- Use a concise lowercase ASCII scope, such as `lighting`, `motion`, `stage`, `tests` or `workflow`; separate multiple words with hyphens.
- Keep the subject at most 72 characters, including the prefix. Use an imperative verb and omit the final period.
- Keep each commit focused on one logical change. Avoid vague subjects such as `update`, `fix bug` or `misc change`.
- Split substantial features into dedicated branches and PRs. State dependencies explicitly when PRs are stacked.
- Validate relevant changes before committing. For message-only rewrites, verify that every rewritten commit retains exactly the same source tree; do not report old test runs as new ones.
- Leave `main` and unrelated history unchanged. Rewrite published task branches only when requested, retain local backup refs and push with explicit `--force-with-lease` expectations.

Examples:

```text
feat(lighting): add custom color palettes
fix(stage): preserve pending restoration during actor replacement
test(motion): cover initial yaw persistence
docs(workflow): require English Conventional Commits
```
