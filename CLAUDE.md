# CLAUDE.md

## Repository relationship

This repository (`lisaEsme/Watchy`) is a fork of the upstream project `sqfmi/Watchy`.

## Never push to upstream

**Never push, open pull requests, or send any changes from this fork to the upstream repository `sqfmi/Watchy`** (or any other repository it was forked from). This applies to commits, branches, tags, pull requests, issues, and comments.

- All pushes go to `origin` (`lisaEsme/Watchy`) only.
- Pulling or fetching *from* upstream to keep the fork updated is fine.
- If an `upstream` remote is added for syncing, disable pushing to it:

  ```bash
  git remote set-url --push upstream DISABLED-do-not-push-to-upstream
  ```

- When creating a pull request, the base repository must be `lisaEsme/Watchy`, never `sqfmi/Watchy`.
