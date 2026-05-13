# librpmi Release Process

This is the initial lightweight release policy.

## Versioning and tags

- Use SemVer-like tags: `vMAJOR.MINOR.PATCH`
- Current baseline release: `v1.0.0`
- Minor feature release example: `v1.1.0`
- Patch release example: `v1.1.1`
- Optional pre-release tags: `v1.2.0-rc1`, `v1.2.0-rc2`

Tag rules:

- Maintainers must use signed, annotated tags (`git tag -s`).
- Never retag an existing version.
- Tag from the release commit on `main`.

## Cadence

- Planned release cadence: every 6 months.
- Patch releases: High Impact / Security fixes.
- Security or build-break regressions can trigger an immediate patch release.

## Simple release checklist

1. Freeze: stop non-critical feature merges.
2. Validate: run `make`, `make LIBRPMI_TEST=y`, `make check`, and `make docs`.
3. Create release commit (if needed) and tag `vX.Y.Z`.
4. Publish release notes with:
   - Highlights
   - Breaking/API-visible changes
   - Bug fixes
   - Contributor list

## Backport policy

- Bug fixes can be backported to the latest minor line.
- Prefer minimal-risk backports.
- Include `Fixes:` tag in backport commits where possible.
