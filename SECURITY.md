# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| latest  | Yes       |

## Reporting a Vulnerability

If you discover a security vulnerability, please report it privately:

1. **Do not** open a public issue
2. Email: sauloverissimo@gmail.com
3. Include: description, steps to reproduce, potential impact

You will receive a response within 48 hours. Confirmed vulnerabilities will be patched and disclosed responsibly.

## Scope

midi2cpp is a C++17 wrapper over the [midi2](https://github.com/sauloverissimo/midi2) C99 core. It carries no network, file, or OS access of its own; the library reads UMP words from a caller-wired transport, dispatches them to typed callbacks, and writes UMP words back. Security concerns are limited to:

- Buffer overflows in SysEx7 / SysEx8 reassembly or MIDI-CI message parsing
- Integer overflows in value scaling, length calculations, or per-slot bridge windows
- Malformed UMP or MIDI-CI input causing unexpected behavior
- Out-of-bounds reads when parsing variable-length fields (Profiles, PE data)
- Use-after-free in the `m2bridge` slot table when the platform tears down a slot mid-feed

## Mitigations

- The host test suite (7 executables under `tests/`) runs under AddressSanitizer and UndefinedBehaviorSanitizer locally and in CI
- The `test_midi2_bridge` suite specifically exercises the heap allocations inside `m2bridge::begin()` (50x construct/begin/destruct cycles) under ASan
- Input length is validated before accessing message fields
- Multi-byte field access uses explicit bounds checks
- The C99 core (midi2) is strictly zero-allocation, eliminating use-after-free and double-free classes there
- midi2cpp's `new` allocations live only inside `m2bridge::begin()` and are released in the destructor
