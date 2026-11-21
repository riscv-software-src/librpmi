## libRPMI - RPMI Protocol Implementation

![librpmi logo](docs/assets/librpmi-logo.png)

## Introduction
The libRPMI is an implementation of [RISC-V Platform Management Interface](https://github.com/riscv-non-isa/riscv-rpmi).

The libRPMI implements RPMI shared memory transport, RPMI message protocol and
various Service groups and Services as defined in the RPMI specification.

<img src="docs/assets/librpmi-arch.png" alt="librpmi high level architecture" width="600"/>

The libRPMI can be used by - 
1. RISC-V platform vendors to implement RPMI services in their
platform microcontroller firmware.

2. System-level partitions to implement RPMI services running as
separate OpenSBI domain

3. Hypervisors/emulators/simulators to emulate RPMI services for the Guest/VM

### Features

- **RPMI Transport Layer**
  - Shared memory-based transport implementation
  - Support for multiple concurrent RPMI transport instances
  - Per-transport service group configuration (Base group always enabled)

- **RPMI Service Groups**
  - Implements a subset of RPMI service groups; see [COMPLIANCE.md](COMPLIANCE.md) for per-service status
  - See [COMPLIANCE.md](COMPLIANCE.md) for complete implementation and test coverage status

- **Platform Integration**
  - Clean HAL (Hardware Abstraction Layer) interface for platform-specific operations
  - Minimal platform dependencies for easy porting

- **Testing & Documentation**
  - Extensible test framework for service group validation
  - Comprehensive API documentation (HTML and PDF via Doxygen)

## Development
The libRPMI supports GNU Make and comes with a simple Makefile that generates
`librpmi.a`, shared library objects (`librpmi.so.*`), and test applications
under the `build` directory.

### librpmi.a and shared library objects
```
// default without debug logs and tests, compiler optimizations are on
make

// Enable debug logs and build tests, compiler optimizations are off
make LIBRPMI_TEST=y LIBRPMI_DEBUG=y

// Cross compilation
make CROSS_COMPILE=<compiler prefix>
```
By default, `make` generates:
- `build/librpmi.a`
- `build/librpmi.so.<major>.<minor>.<patch>`
- symlinks `build/librpmi.so.<soversion>` and `build/librpmi.so`

The platform vendors may also integrate libRPMI sources directly into the
platform microcontroller firmware and extend firmware build system to
build the libRPMI sources rather than using `librpmi.a`.

### Porting
The libRPMI does not use the C library and calls the environment functions
defined in `include/librpmi_env.h` instead. Most of these functions have a
simple default implementation in the header itself and the platform firmware
must provide the remaining functions such as -
1. `rpmi_env_zalloc()` and `rpmi_env_free()` for memory allocation
2. `rpmi_env_writel()` and `rpmi_env_readl()` for MMIO
3. `rpmi_env_printf()` for debug logs

The test framework under `test` directory implements these functions using
the standard C library and can be used as a reference.

## Usage
The platform firmware creates a shared memory transport, a RPMI context and
the service groups it wants to support. The Base service group is part of the
RPMI context and is always available.
```c
// shared memory used for the RPMI queues
shmem = rpmi_shmem_create("rpmi_shmem", shmem_base, shmem_size,
                          &rpmi_shmem_simple_ops, NULL);

// shared memory transport with slot size and queue sizes
trans = rpmi_transport_shmem_create("rpmi_trans", slot_size,
                                    a2p_req_queue_size, p2a_req_queue_size,
                                    shmem);

// RPMI context for M-mode
cntx = rpmi_context_create("rpmi_cntx", trans, RPMI_SRVGRP_ID_MAX_COUNT,
                           RPMI_PRIVILEGE_M_MODE, 0, NULL);

// create service groups and add to the RPMI context
grp = rpmi_service_group_sysreset_create(sysreset_type_count, sysreset_types,
                                         &sysreset_ops, NULL);
rpmi_context_add_group(cntx, grp);

// process the requests from application processor and service group events
rpmi_context_process_a2p_request(cntx);
rpmi_context_process_all_events(cntx);
```
Refer: [librpmi.h](include/librpmi.h) for the complete API and the tests under
`test` directory for more examples.

## Packaging
The repository can build Debian and RPM packages for systems that want to
consume libRPMI as an installed library instead of integrating the source tree
or manually copying build artifacts. These packages provide a repeatable way to
install the runtime library, development files, and source content needed by
downstream projects such as emulators, hypervisors, firmware build flows, and
distribution packaging pipelines.

Debian packages can be built with:
```shell
make deb-pkg
```

This stages the Debian packaging from `packaging/debian` and writes the package
outputs under `build/deb`. The generated packages are:
- `librpmi0`: runtime shared library (`librpmi.so.*`).
- `librpmi-dev`: headers, static library, linker symlink, and `pkg-config`
  metadata for building applications against librpmi.
- `librpmi-src`: source content installed under `/usr/src/librpmi` for users
  that need to inspect or rebuild the library sources.

Install the generated Debian packages with:
```shell
sudo dpkg -i build/deb/librpmi0_*.deb \
             build/deb/librpmi-dev_*.deb \
             build/deb/librpmi-src_*.deb
```

RPM packages can be built with:
```shell
make rpm-pkg
```

This uses `packaging/rpm/librpmi.spec` and writes the RPM build tree under
`build/rpmbuild`. The generated RPM packages are:
- `librpmi`: runtime shared library (`librpmi.so.*`).
- `librpmi-devel`: headers, static library, linker symlink, and `pkg-config`
  metadata for building applications against librpmi.
- `librpmi-source`: source content installed under `/usr/src/librpmi`.

Install the generated RPM packages with the system package manager, for example:
```shell
sudo dnf install build/rpmbuild/RPMS/*/*.rpm
```

## Documentation
The libRPMI supports doxygen which can generate both html and pdf
documentation under `build/docs` directory.
```
make docs
```

This generates pdf file `build/docs/latex/refman.pdf` and html documentation
at `build/docs/html`.

## Test
Build test binaries -
```shell
make LIBRPMI_TEST=y
```
Run tests -
```shell
make check
```
Refer: [README in test folder](test/README.md)

The tests are also run by GitHub actions for every pull request.

## Project process

- [Contributing guide](CONTRIBUTING.md)
- [Maintainers](MAINTAINERS.md)
- [Release process](RELEASE.md)

## License

The libRPMI is provided under [2-Clause BSD License](COPYING.BSD)
