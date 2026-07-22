# librpmi RPMI Specification Compliance

This document tracks librpmi's implementation status against the RISC-V Platform Management Interface (RPMI) specification.

## Compliance Matrix

### RPMI v1.0 (Ratified - Released 2025-07-16)

| Service Group ID | Service Group Name | Implementation | Tests | Notes |
|-----------------|-------------------|----------------|-------|-------|
| 0x0001 | BASE | ⏳ Partial | ⏳ Partial | Missing REQUEST_HANDLE_ERROR event |
| 0x0002 | SYSTEM_MSI | ✅ Complete | ✅ Complete | N/A |
| 0x0003 | SYSTEM_RESET | ✅ Complete | ✅ Complete | N/A |
| 0x0004 | SYSTEM_SUSPEND | ✅ Complete | ✅ Complete | N/A |
| 0x0005 | HART_STATE_MANAGEMENT | ✅ Complete | ✅ Complete | N/A |
| 0x0006 | CPPC | ✅ Complete | ✅ Complete | N/A |
| 0x0007 | VOLTAGE | ✅ Complete | ✅ Complete | N/A |
| 0x0008 | CLOCK | ✅ Complete | ✅ Complete | N/A |
| 0x0009 | DEVICE_POWER | ✅ Complete | ❌ N/A | N/A |
| 0x000A | PERFORMANCE | ⏳ Partial | ❌ N/A | Missing 3 notification events |
| 0x000B | MANAGEMENT_MODE | ✅ Complete | ✅ Complete | N/A |
| 0x000C | RAS_AGENT | ❌ N/A | ❌ N/A | Not implemented |
| 0x000D | REQUEST_FORWARD | ❌ N/A | ❌ N/A | Not implemented |

### RPMI v2.0 (Draft)

| Service Group ID | Service Group Name | Implementation | Tests | Notes |
|-----------------|-------------------|----------------|-------|-------|
| 0x000E | LOGGING | ⏳ Partial | ❌ N/A | Missing ENABLE_NOTIFICATION service |
| 0x000F | SYSTEM_IRQ | ❌ N/A | ❌ N/A | Not implemented |
| 0x0010 | TEE | ❌ N/A | ❌ N/A | Not implemented |

## Status Definitions

- **✅ Complete:** Fully implemented/tested
- **⏳ Partial:** Partially implemented/tested
- **❌ N/A:** Not applicable or not implemented

## Implementation Details

### Missing Notification Events

Some service groups define notification events in the RPMI specification that are not yet implemented:

- **BASE (0x0001)**: Missing `REQUEST_HANDLE_ERROR` event
- **PERFORMANCE (0x000A)**: Missing events:
  - `PERF_POWER_CHANGE`
  - `PERF_LIMIT_CHANGE`
  - `PERF_LEVEL_CHANGE`

All other service groups either have no events defined in the specification or have event notifications correctly returning `RPMI_ERR_NOTSUPP`.

## Notes

- Service group IDs and names match the official RPMI specification
- Test coverage refers to automated test suites in the `test/` directory
- Implementation status reflects code availability in the `lib/` directory
- "No events per spec" means the RPMI specification explicitly states the service group does not support notification events

---

For contribution guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md).
