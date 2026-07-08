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

test-elfs-y += test_srvgrp_clock

test_srvgrp_clock-objs-y += test/test_log.o
test_srvgrp_clock-objs-y += test/test_common.o

test-elfs-y += test_srvgrp_sysmsi

test_srvgrp_sysmsi-objs-y += test/test_log.o
test_srvgrp_sysmsi-objs-y += test/test_common.o

test-elfs-y += test_srvgrp_cppc

test_srvgrp_cppc-objs-y += test/test_log.o
test_srvgrp_cppc-objs-y += test/test_common.o

test-elfs-y += test_srvgrp_voltage

test_srvgrp_voltage-objs-y += test/test_log.o
test_srvgrp_voltage-objs-y += test/test_common.o

test-elfs-y += test_srvgrp_mm

test_srvgrp_mm-objs-y += test/test_log.o
test_srvgrp_mm-objs-y += test/test_common.o

test-elfs-y += test_srvgrp_syssusp

test_srvgrp_syssusp-objs-y += test/test_log.o
test_srvgrp_syssusp-objs-y += test/test_common.o

test-elfs-y += test_srvgrp_ras

test_srvgrp_ras-objs-y += test/test_log.o
test_srvgrp_ras-objs-y += test/test_common.o
