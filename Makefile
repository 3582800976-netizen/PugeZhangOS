# This file selects an experiment; all kernel sources live inside that lab.
LAB ?=
CPUS ?= 3
CHECK_CPUS ?= 1,2,3,8

ifeq ($(strip $(LAB)),)
.DEFAULT_GOAL := help
else
ifeq ($(filter $(LAB),1 2 3),)
$(error Only Lab 1-3 are implemented; see lab4/README.md to lab9/README.md for future plans)
endif
.DEFAULT_GOAL := all
endif

.PHONY: help all qemu qemu-gdb check clean
help:
	@echo "Architecture: ARCHITECTURE.md; roadmap: docs/ROADMAP.md"
	@echo "Lab 1-3 are implemented; Lab 4-9 contain plans, pending implementation."
	@echo "Each implemented lab has independent sources, a Makefile and a README."
	@echo "Build:   make -C lab1"
	@echo "Run:     make -C lab1 qemu CPUS=3"
	@echo "         make -C lab2 qemu CPUS=3"
	@echo "         make -C lab3 qemu CPUS=3"
	@echo "Check:   make -C lab1 check"
	@echo "All:     make check"
	@echo "Compatibility: make qemu LAB=1 CPUS=3"

all qemu qemu-gdb:
	$(if $(strip $(LAB)),,$(error Select a lab: make -C lab1 $@, or make $@ LAB=1))
	$(MAKE) -C lab$(LAB) $@

check:
	python3 scripts/check.py --lab $(if $(strip $(LAB)),$(LAB),all) --cpus "$(CHECK_CPUS)"

clean:
	$(MAKE) -C lab1 clean
	$(MAKE) -C lab2 clean
	$(MAKE) -C lab3 clean
