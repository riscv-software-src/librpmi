# Contributing to librpmi

This project follows a lightweight Linux/OpenSBI-style workflow.

## Submit changes

1. Fork and create a branch from `main`.
2. Make one logical change per commit.
3. Use a subject prefix in commit title:
   - `lib:`, `include:`, `test:`, `docs:`, `debian:`, `build:`
4. Add `Signed-off-by:` to every commit (DCO).
5. Open a pull request against `riscv-software-src/librpmi:main`.
6. If you add a new service group, include basic tests for it in the same series.

Example:

```
lib: cpcc: fix frequency bounds check

Fixes: 1a2b3c4d5e6f ("lib: cpcc: add set perf path")
Signed-off-by: Your Name <you@example.com>
```

## Required checks before PR

- `make`
- `make check`
- `make LIBRPMI_TEST=y`

If documentation changes are included, also run:

- `make docs`

## Commit and trailer rules

- For bug fixes, add `Fixes: <12+ sha> ("subject")` when applicable.
- For externally reported issues, add `Closes: <public issue URL>`.
- Keep commit messages self-contained and explain user impact.

Recommended (when applicable): `Reviewed-by:`, `Tested-by:`, `Acked-by:`.

## DCO

By contributing, you certify your work under the Developer Certificate of
Origin 1.1 by adding `Signed-off-by:` to commits:

`Signed-off-by: Your Name <you@example.com>`
