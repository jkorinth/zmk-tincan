# Feature: Automated Releases

Introduce release-please with a minimal configuration for this repo.
Add the github workflows and automatically build the releases. Keep
a changelog using conventional commits.

## Prerequisites

- Must establish CONTRIBUTING.md to communicate:
  1. conventional commits only
  2. rebase + ff-only trunk-based development
  3. no `dev` branch, just `main` and `feat/...` branches.
  4. mandatory pull requests
  5. mandatory CI checks on every PR

When that is setup, release-please's PR branch and pull request
will automatically track the changes and version.
