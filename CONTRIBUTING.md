# Contributing to DSO-TRON

Thanks for your interest in improving DSO-TRON!

## Building and testing

There is no separate unit-test suite yet. CI (`.github/workflows/ci.yml`)
checks that the kernel builds cleanly (`-Wall -Wextra -Werror`) with:

```bash
sudo apt-get update && sudo apt-get install -y lld
make all
```

The result is `build/dso-tron.bin`. Please make sure `make all` succeeds
without warnings before opening a pull request.

To smoke-test the kernel in QEMU (needs `grub2-common`, `xorriso` and
`qemu-system-x86` in addition to the above), use the Makefile targets:

```bash
make iso    # builds dso-tron.iso
make run    # boots it with qemu-system-x86_64 -nographic -serial mon:stdio
make clean  # removes build/, iso/ and dso-tron.iso
```

Check that the serial output matches the demo output in the README
(`T1:N T2:N` pairs with no gaps).

## Reporting a bug

Open an issue at <https://github.com/kostyk348/DSO-TRON/issues> and include:

- the DSO-TRON version or commit hash (`git rev-parse HEAD`);
- your host OS/distribution and architecture;
- `gcc --version`, `ld.lld --version` and, if relevant, `qemu-system-x86_64 --version`;
- the exact commands you ran and the full build or serial output;
- what you expected to happen and what happened instead.

## Reporting a security problem

Please do **not** open a public issue for security problems. Report them
privately via a GitHub security advisory:
<https://github.com/kostyk348/DSO-TRON/security/advisories/new>

## Commit messages

Write a short imperative summary line (≤ 72 characters, e.g. `Fix FIFO wrap-around in dso_fifo_pop`), optionally followed by a blank line and a longer explanation.
