# Contributing

## Commit messages

All commits must follow [Conventional Commits](https://www.conventionalcommits.org/).
`release-please` parses commit types (`feat`, `fix`, ...), breaking-change
markers (`!` / `BREAKING CHANGE:` footer) and scopes to decide the next
version and to write the changelog — non-conventional commits are silently
excluded from the changelog and version bump.

## Branching

`main` is the only long-lived branch; there is no `dev` branch. Work happens
on short-lived `feat/<name>` branches cut from `main`. Before opening a pull
request, rebase your branch onto the latest `main`. History on `main` must
stay linear — merges are fast-forward only (enforced by branch protection),
so rebase instead of merging `main` into your branch.

## Pull requests and CI

All changes land via pull request; direct pushes to `main` are blocked.
Every PR must pass CI (the `lint` check) before it can be merged.

## Releases

`release-please` watches commits on `main`. Whenever a conventional commit
lands, it opens or updates a `chore(release): ...` pull request with the
next version and changelog entry. Merging that PR tags the repo and
publishes a GitHub Release — there's nothing else to do manually.
