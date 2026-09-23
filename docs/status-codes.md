# Status codes

Archival rationale for `src/core/status.h`'s `Status` enum -- the single
shared error/result type used across `src/core/` and consumed from
`src/module/exports.cpp`.

## Why one shared enum, owned by its own file

`json-path.h`, `type-conversion.h` and `buffer-copy.h` all need the same
result type. Hosting it inside any one of those headers would create an
arbitrary include dependency between otherwise-independent modules, so it
lives in its own file instead, included by all three (and by
`src/module/exports.cpp`).

## Why 0/-1/-2/-3 are absorbed, not renumbered

`Ok = 0`, `InvalidArgument = -1`, `BufferTooSmall = -2` and
`VersionResourceUnavailable = -3` are pre-existing, already-shipped values
from `restifyGetVersion` (`src/module/exports.cpp`), predating this enum.
They were absorbed into `Status` rather than given new numbers, because
CAPL scripts already depend on these exact values at runtime
(`capl-export-contract`) -- renumbering any of them would silently break
every script built against the DLL's first exported operation.

`VersionResourceUnavailable` is declared here so the numbering space has a
single owner, even though it is unreachable from anything in `src/core/`
itself -- only `src/module/exports.cpp` can produce it (the Win32
version-resource read that yields it has no place in CANoe-unaware code).

## Reserved ranges

- `-4..-9`: future module-local glue codes, currently empty.

Each range has exactly one owning layer, so a future addition never has to
guess where the next free number is.
