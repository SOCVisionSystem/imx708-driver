# Contributing to imx708-driver

Thanks for your interest! This is a Linux kernel driver for the Sony IMX708
camera sensor. We welcome bug reports, documentation, test cases, and code.

## Quick Start

```bash
git clone https://github.com/soccentric-vision-system/imx708.git
cd imx708/imx708-driver
make
make test
```

## Coding Style

- Follow [Linux kernel coding style](https://www.kernel.org/doc/html/v4.10/process/coding-style.html)
- 8-character tabs, 100-column limit
- SPDX header on every file: `// SPDX-License-Identifier: GPL-2.0-only`
- Doxygen-style comments on every function
- Use `devm_*` managed resources
- Run `checkpatch.pl --strict` before submitting

## Pull Request Process

1. Fork the repo, create a feature branch
2. Make your changes, add tests
3. Run `make test` to verify
4. Submit a PR with a clear description

## Good First Issues

- Bug reports and documentation improvements
- Additional V4L2 control implementations
- Test case additions
- Device tree overlays for new boards

## License

GPL-2.0-only. By contributing, you agree to license your contributions
under the same license.
