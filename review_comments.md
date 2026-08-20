# Draft Review Comments

PR: riscv-software-src/librpmi #114 - lib,test: align logging service and add coverage

Draft-only until human approval.

## pr114-test-test_srvgrp_logging-c-1-process-maintainers-7f3a

- Severity: LOW
- Location: test/test_srvgrp_logging.c:1
- Comment:

```text
checkpatch reports "added, moved or deleted file(s), does MAINTAINERS need
updating?" for the new test/test_srvgrp_logging.c file. Please confirm
whether this repository expects new service-group tests to be reflected in
MAINTAINERS. If not, this can be treated as an intentional checkpatch false
positive.

Rule: LNX-SUBMIT-STYLECHECK
Source: https://www.kernel.org/doc/html/latest/process/submitting-patches.html#style-check-your-changes
```

## Open Questions

- Is the MAINTAINERS warning applicable for new test/test_srvgrp_*.c files in this repository?
