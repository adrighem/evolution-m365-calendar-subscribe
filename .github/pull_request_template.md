<!-- SPDX-License-Identifier: GPL-3.0-only -->

## Summary


## Test Plan

- [ ] `cmake -S . -B build`
- [ ] `cmake --build build`
- [ ] `ctest --test-dir build --output-on-failure`

## Checklist

- [ ] The change is scoped to Evolution M365 calendar subscription behavior.
- [ ] User-facing behavior or documentation is updated when needed.
- [ ] No credentials, tenant IDs, access tokens, or private calendar data are included.
