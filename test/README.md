
# 1. Overview

This repo add RPMI tests for rpmi framework

# 2. Running tests

build and run tests

```shell
# build the tests
$ make LIBRPMI_TEST=y

# execute the tests
$ make check
...
Test Base Service Group

Executing Base Service Group Default test scenario :
-------------------------------------------------
TEST: RPMI_BASE_SRV_ENABLE_NOTIFICATION                          : Succeeded!
TEST: RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION                   : Succeeded!
TEST: RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN                       : Succeeded!
TEST: RPMI_BASE_SRV_GET_SPEC_VERSION                             : Succeeded!
TEST: RPMI_BASE_SRV_GET_PLATFORM_INFO                            : Succeeded!
TEST: RPMI_BASE_SRV_PROBE_SERVICE_GROUP                          : Succeeded!
TEST: RPMI_BASE_SRV_GET_ATTRIBUTES                               : Succeeded!
Test System Reset Service Group

Executing System Reset Service Group test scenario :
-------------------------------------------------
TEST: ENABLE NOTIFICATION TEST (notifications not supported)     : Succeeded!
TEST: NOTIFICATION STATUS TEST (notifications not supported)     : Succeeded!
TEST: GET ATTRIBUTES TEST (for supported reset type)             : Succeeded!
TEST: GET ATTRIBUTES TEST (for unsupported reset type)           : Succeeded!
platform callback: cold reset
TEST: SYSTEM RESET (with supported reset type)                   : Succeeded!
Test Hart State Management Service Group

Executing System HSM Service Group test scenario :
-------------------------------------------------
TEST: ENABLE NOTIFICATION TEST (notifications not supported)     : Succeeded!
TEST: NOTIFICATION STATUS TEST (notifications not supported)     : Succeeded!
TEST: GET HART LIST                                              : Succeeded!
TEST: GET HART STATE (valid hart id)                             : Succeeded!
TEST: GET HART STATE (invalid hart id)                           : Succeeded!
TEST: HART START (valid hart id, hart already started)           : Succeeded!
TEST: HART START (invalid hart id)                               : Succeeded!
TEST: HART STOP (hart in start state)                            : Succeeded!
TEST: HART STOP (hart already stopped)                           : Succeeded!
TEST: HART Suspend (not supported)                               : Succeeded!
```
