
# 1. Overview

This repo add RPMI tests for rpmi framework

# 2. Running tests

build and run tests

```
# build the tests
$ make

# execute the tests
$ ./build/test/test_base.elf

Test Base Service Group
*******************************************

Executing RPMI_BASE service group tests :
-------------------------------------------------
RPMI_BASE_SRV_ENABLE_NOTIFICATION                        : Succeeded!
RPMI_BASE_SRV_PROBE_SERVICE_GROUP                        : Succeeded!
RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION                 : Succeeded!
RPMI_BASE_SRV_GET_SPEC_VERSION                           : Succeeded!
RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN                     : Succeeded!
RPMI_BASE_SRV_GET_PLATFORM_INFO                          : Succeeded!
RPMI_BASE_SRV_GET_ATTRIBUTES                             : Succeeded!
RPMI_BASE_SRV_SET_MSI                                    : Succeeded!

```

