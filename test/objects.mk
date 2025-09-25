#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Ventana Micro Systems Inc.
#

test-elfs-y += test_srvgrp_base

test_srvgrp_base-objs-y += test/test_log.o
test_srvgrp_base-objs-y += test/test_common.o

test-elfs-y += test_srvgrp_sysreset

test_srvgrp_sysreset-objs-y += test/test_log.o
test_srvgrp_sysreset-objs-y += test/test_common.o

test-elfs-y += test_srvgrp_hsm

test_srvgrp_hsm-objs-y += test/test_log.o
test_srvgrp_hsm-objs-y += test/test_common.o
