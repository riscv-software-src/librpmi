# librpmi Maintainers

This file uses a reduced Linux-style format:

- `M:` maintainer
- `R:` reviewer
- `S:` status (`Maintained`, `Odd Fixes`, `Orphan`)
- `F:` file patterns

## LIBRPMI CORE

- `M:` Anup Patel <anup@brainfault.org>
- `M:` Rahul Pathak <rahul@summations.net>
- `M:` Ranbir Singh <saini.ranbirs@outlook.com>
- `M:` Subrahmanya Lingappa <subbukl@gmail.com>
- `S:` Maintained
- `F:` `include/`
- `F:` `lib/`
- `F:` `Makefile`

## TESTS

- `M:` Anup Patel <anup@brainfault.org>
- `M:` Rahul Pathak <rahul@summations.net>
- `M:` Ranbir Singh <saini.ranbirs@outlook.com>
- `M:` Subrahmanya Lingappa <subbukl@gmail.com>
- `S:` Maintained
- `F:` `test/`

## DOCUMENTATION

- `M:` Anup Patel <anup@brainfault.org>
- `M:` Rahul Pathak <rahul@summations.net>
- `M:` Ranbir Singh <saini.ranbirs@outlook.com>
- `M:` Subrahmanya Lingappa <subbukl@gmail.com>
- `S:` Maintained
- `F:` `README.md`
- `F:` `CONTRIBUTING.md`
- `F:` `RELEASE.md`
- `F:` `docs/`

## Maintainer policy

- A maintainer should not self-merge their own pull request.
- At least one reviewer or maintainer approval is required before merge.
- Maintainers must create signed and annotated release tags (`git tag -s`).
- New subsystems should add a dedicated section in this file.
