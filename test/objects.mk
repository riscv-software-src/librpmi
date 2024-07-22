#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Ventana Micro Systems Inc.
#

test-elfs-y += test_base

test_base-objs-y += test/test_log.o
test_base-objs-y += test/test_common.o

test-elfs-y += test_sysreset

test_sysreset-objs-y += test/test_log.o
test_sysreset-objs-y += test/test_common.o
