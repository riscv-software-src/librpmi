#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Ventana Micro Systems Inc.
#

# Select Make Options:
MAKEFLAGS += -r --no-print-directory

# Readlink -f requires GNU readlink
ifeq ($(shell uname -s),Darwin)
READLINK ?= greadlink
else
READLINK ?= readlink
endif

# Find out source, build, and install directories
src_dir=$(CURDIR)
ifdef O
 build_dir=$(shell $(READLINK) -f $(O))
else
 build_dir=$(CURDIR)/build
endif
ifeq ($(build_dir),$(CURDIR))
$(error Build directory is same as source directory.)
endif
install_root_dir_default=$(CURDIR)/install
ifdef I
 install_root_dir=$(shell $(READLINK) -f $(I))
else
 install_root_dir=$(install_root_dir_default)
endif
ifeq ($(install_root_dir),$(CURDIR))
$(error Install root directory is same as source directory.)
endif
ifeq ($(install_root_dir),$(build_dir))
$(error Install root directory is same as build directory.)
endif
docs_dir=$(build_dir)/docs
install_docs_dir=$(install_root_dir)/docs
# Check if verbosity is ON for build process
CMD_PREFIX_DEFAULT := @
ifeq ($(V), 1)
	CMD_PREFIX :=
else
	CMD_PREFIX := $(CMD_PREFIX_DEFAULT)
endif

# Setup path of directories
export include_dir=$(src_dir)/include
export lib_dir=$(src_dir)/lib
export test_dir=$(src_dir)/test

ifeq ($(LLVM),1)
CC		=	clang
AR		=	llvm-ar
AS		=	llvm-as
LD		=	ld.lld
OBJCOPY		=	llvm-objcopy
else
# Setup compilation commands
ifdef CROSS_COMPILE
CC		=	$(CROSS_COMPILE)gcc
AR		=	$(CROSS_COMPILE)ar
AS		=	$(CROSS_COMPILE)as
LD		=	$(CROSS_COMPILE)ld
else
CC		?=	gcc
AR		?=	ar
AS		?=	as
LD		?=	ld
endif
endif
CPP		=	$(CC) -E

# Setup list of objects.mk files
lib-object-mks=$(shell if [ -d $(lib_dir)/ ]; then find $(lib_dir) -iname "objects.mk" | sort -r; fi)
test-object-mks=$(shell if [ -d $(test_dir)/ ]; then find $(test_dir) -iname "objects.mk" | sort -r; fi)

# Include all object.mk files
include $(lib-object-mks)
include $(test-object-mks)

# Setup list of objects
lib-objs-path-y=$(foreach obj,$(lib-objs-y),$(build_dir)/lib/$(obj))
test-elfs-path-y=$(foreach elf,$(test-elfs-y),$(build_dir)/test/$(elf).elf)
test-objs-path-y=$(foreach elf,$(test-elfs-y),$(build_dir)/test/$(elf).o)
test-objs-path-y+=$(foreach elf,$(test-elfs-y),$(foreach obj,$($(elf)-objs-y),$(build_dir)/$(obj)))

# Setup list of deps files for objects
deps-y=$(lib-objs-path-y:.o=.dep)

ifeq ($(LIBRPMI_TEST),y)
deps-y+=$(test-objs-path-y:.o=.dep)
endif

# Setup compilation commands flags
GENFLAGS	=	-Wall -Werror -g
GENFLAGS	+=	-I$(include_dir) -I$(lib_dir)

ifeq ($(LIBRPMI_DEBUG),y)
GENFLAGS 	+=	 -O0 -DDEBUG
else
GENFLAGS 	+=	 -O2
endif

EXTRA_CFLAGS	+= 	-Wsign-compare

CFLAGS		=	$(GENFLAGS)
CFLAGS		+=	$(EXTRA_CFLAGS)

CPPFLAGS	+=	$(GENFLAGS)

ARFLAGS		=	rcs

ELFFLAGS	+=	-static -L$(build_dir) -lrpmi
ELFFLAGS	+=	$(EXTRA_ELFFLAGS)

# Setup functions for compilation
define dynamic_flags
-I$(shell dirname $(2)) -D__OBJNAME__=$(subst -,_,$(shell basename $(1) .o))
endef
copy_file =  $(CMD_PREFIX)mkdir -p `dirname $(1)`; \
	     echo " COPY      $(subst $(build_dir)/,,$(1))"; \
	     cp -f $(2) $(1)
inst_file =  $(CMD_PREFIX)mkdir -p `dirname $(1)`; \
	     echo " INSTALL   $(subst $(install_root_dir)/,,$(1))"; \
	     cp -f $(2) $(1)
inst_file_list = $(CMD_PREFIX)if [ ! -z "$(4)" ]; then \
	     mkdir -p $(1)"/"$(3); \
	     for rel_file in $(4) ; do \
	     dest_file=$(1)"/"$(3)"/"`echo $$rel_file`; \
	     dest_dir=`dirname $$dest_file`; \
	     src_file=$(2)"/"`echo $$rel_file`; \
	     echo " INSTALL   "$(3)"/"`echo $$rel_file`; \
	     mkdir -p $$dest_dir; \
	     cp -f $$src_file $$dest_file; \
	     done \
	     fi
inst_dir =  $(CMD_PREFIX)mkdir -p $(1); \
	     echo " INSTALL   $(subst $(install_root_dir)/,,$(1))"; \
	     rm -rf $(1); \
	     cp -rf $(2) $(1)
compile_cpp = $(CMD_PREFIX)mkdir -p `dirname $(1)`; \
	     echo " CPP       $(subst $(build_dir)/,,$(1))"; \
	     $(CPP) $(CPPFLAGS) -x c $(2) | grep -v "\#" > $(1)
compile_cc_dep = $(CMD_PREFIX)mkdir -p `dirname $(1)`; \
	     echo " CC-DEP    $(subst $(build_dir)/,,$(1))"; \
	     printf %s `dirname $(1)`/  > $(1) && \
	     $(CC) $(CFLAGS) $(call dynamic_flags,$(1),$(2))   \
	       -MM $(2) >> $(1) || rm -f $(1)
compile_cc = $(CMD_PREFIX)mkdir -p `dirname $(1)`; \
	     echo " CC        $(subst $(build_dir)/,,$(1))"; \
	     $(CC) $(CFLAGS) $(call dynamic_flags,$(1),$(2)) -c $(2) -o $(1)
compile_elf = $(CMD_PREFIX)mkdir -p `dirname $(1)`; \
	     echo " ELF       $(subst $(build_dir)/,,$(1))"; \
	     $(CC) $(CFLAGS) $(2) $(3) $(4) $(ELFFLAGS) -o $(1)
compile_ar = $(CMD_PREFIX)mkdir -p `dirname $(1)`; \
	     echo " AR        $(subst $(build_dir)/,,$(1))"; \
	     $(AR) $(ARFLAGS) $(1) $(2)

blobs-y = $(build_dir)/librpmi.a
blobs-$(LIBRPMI_TEST) += $(test-elfs-path-y)

# Default rule "make" should always be first rule
.PHONY: all
all: printdetails $(blobs-y)

.PHONY: printdetails
printdetails:
	$(info )
	$(info ------------------ LIBRPMI BUILD INFO --------------------)
	$(info CC:        ($(CC)))
	$(info CPP:       ($(CPP)))
	$(info AS:        ($(AS)))
	$(info AR:        ($(AR)))
	$(info LD:        ($(LD)))
	$(info ARFLAGS:   ($(ARFLAGS)))
	$(info CFLAGS:    ($(CFLAGS)))
	$(info LDFLAGS:   ($(LDFLAGS)))
	$(info --------------------------------------------------)

# Preserve all intermediate files
.SECONDARY:

$(build_dir)/%.elf: $(build_dir)/%.o $(test-objs-path-y) $(build_dir)/librpmi.a
	$(call compile_elf,$@,$<,$(foreach obj,$($(*F)-objs-y),$(build_dir)/$(obj)),$($(*F)-cflags-y))

$(build_dir)/librpmi.a: $(lib-objs-path-y)
	$(call compile_ar,$@,$^)

$(build_dir)/%.dep: $(src_dir)/%.c
	$(call compile_cc_dep,$@,$<)

$(build_dir)/%.o: $(src_dir)/%.c
	$(call compile_cc,$@,$<)

# Dependency files should only be included after default Makefile rules
# They should not be included for any "xxxconfig" or "xxxclean" rule
all-deps-1 = $(if $(findstring config,$(MAKECMDGOALS)),,$(deps-y))
all-deps-2 = $(if $(findstring clean,$(MAKECMDGOALS)),,$(all-deps-1))
-include $(all-deps-2)

# Rule for "make install"
.PHONY: install
install: $(build_dir)/librpmi.a $(src_dir)/COPYING.BSD
	$(call inst_dir,$(install_root_dir)/include,$(include_dir))
	$(call inst_file,$(install_root_dir)/lib/librpmi.a,$(build_dir)/librpmi.a)
	$(call inst_file,$(install_root_dir)/COPYING.BSD,$(src_dir)/COPYING.BSD)

# Rule for "make clean"
.PHONY: clean
clean:
	$(CMD_PREFIX)mkdir -p $(build_dir)
	$(if $(V), @echo " RM        $(build_dir)/*.o")
	$(CMD_PREFIX)find $(build_dir) -type f -name "*.o" -exec rm -rf {} +
	$(if $(V), @echo " RM        $(build_dir)/*.a")
	$(CMD_PREFIX)find $(build_dir) -type f -name "*.a" -exec rm -rf {} +

# Rule for "make distclean"
.PHONY: distclean
distclean: clean
	$(CMD_PREFIX)mkdir -p $(build_dir)
	$(if $(V), @echo " RM        $(build_dir)/*.dep")
	$(CMD_PREFIX)find $(build_dir) -type f -name "*.dep" -exec rm -rf {} +
ifeq ($(build_dir),$(CURDIR)/build)
	$(if $(V), @echo " RM        $(build_dir)")
	$(CMD_PREFIX)rm -rf $(build_dir)
endif
ifeq ($(install_root_dir),$(install_root_dir_default))
	$(if $(V), @echo " RM        $(install_root_dir_default)")
	$(CMD_PREFIX)rm -rf $(install_root_dir_default)
endif

.PHONY: docs
docs:
	$(CMD_PREFIX)mkdir -p $(docs_dir)
	$(CMD_PREFIX)cat docs/doxygen/Doxyfile | sed -e "s#@@SRC_DIR@@#.#" -e "s#@@BUILD_DIR@@#$(build_dir)#" > $(docs_dir)/Doxyfile
	$(CMD_PREFIX)doxygen $(docs_dir)/Doxyfile
	$(CMD_PREFIX)make -C $(docs_dir)/latex

# Rule for make cleandocs
.PHONY: cleandocs
cleandocs:
	$(CMD_PREFIX)rm -rf $(build_dir)/docs

.PHONY: FORCE
FORCE:
