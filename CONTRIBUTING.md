# Contributing

Thanks for improving Evolution M365 Calendar Subscribe.

## Development Setup

Install the build dependencies from [INSTALL.md](INSTALL.md), then build and test:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

UI tests use GTK. If you are running in a headless environment, use:

```bash
xvfb-run -a ctest --test-dir build --output-on-failure
```

## Pull Requests

- Keep changes focused on one bug fix or feature.
- Add or update tests when behavior changes.
- Update README or INSTALL when setup, usage, or troubleshooting changes.
- Do not include credentials, tenant IDs, access tokens, or private calendar data.
- Use conventional commit prefixes where possible, such as `fix:`, `feat:`,
  `docs:`, `test:`, or `ci:`.

External PRs are reviewed as design input. Maintainers may reimplement changes to
keep the codebase consistent and maintainable.

## License

By contributing, you agree that your contribution is licensed under GPLv3.
