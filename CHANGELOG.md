# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Version headings correspond to Git tags on this repository; there are no
hand-invented version numbers.

## [Unreleased]

### Added

- `restifyGetVersion(char buffer[], dword bufferSize) : long` -- the
  project's first CAPL-exported operation (Stage 5, ABI proof). Writes the
  DLL's build version string into a caller-supplied buffer; returns 0 on
  success or a negative error code (see `src/module/exports.cpp`).
- `scripts/setup-dev-env.ps1`: development environment bootstrap script
  that provisions MSVC Build Tools, `make`, vcpkg-built libcurl (x86 and
  x64, static, SChannel), and the pinned `nlohmann/json` single header.
  Verified working end to end on a real machine (14 OK, 0 WARN, 0 FAIL).
