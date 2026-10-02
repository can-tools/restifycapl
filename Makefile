# Makefile -- restifycapl (CAPL REST DLL).
# Targets: help (default), all, build-x86, build-x64, test, clean
# The MSVC environment comes from the active shell, or from the
# setup-stored RESTIFY_MSVC_<ARCH>_* variables when those are defined.

SHELL := cmd.exe
.SHELLFLAGS := /Q /C

.DEFAULT_GOAL := help

# ------------------------------------------------------------------------------
# Architecture selection -- the ONE place x86 vs x64 differs. Everything
# below this block (flags, source list, link libraries) is identical text
# for both architectures; only these variables change per ARCH.
# ------------------------------------------------------------------------------

ARCH ?= x64

ifeq ($(ARCH),x86)
  MACHINE  := X86
  LIBDIR   := lib/x86
  GTESTDIR := lib/gtest/x86
else ifeq ($(ARCH),x64)
  MACHINE  := X64
  LIBDIR   := lib/x64
  GTESTDIR := lib/gtest/x64
else
  $(error Unknown ARCH '$(ARCH)' -- must be x86 or x64)
endif

# Stored variables win over an activated shell, so ARCH alone picks the toolchain.
# Both PATH spellings are set because Windows exports the variable as Path.
ifdef RESTIFY_MSVC_$(MACHINE)_PATH
  RESTIFY_BASE_PATH := $(or $(PATH),$(Path))
  PATH    := $(RESTIFY_MSVC_$(MACHINE)_PATH);$(RESTIFY_BASE_PATH)
  Path    := $(PATH)
  INCLUDE := $(RESTIFY_MSVC_$(MACHINE)_INCLUDE)
  LIB     := $(RESTIFY_MSVC_$(MACHINE)_LIB)
  LIBPATH := $(RESTIFY_MSVC_$(MACHINE)_LIBPATH)
  export PATH Path INCLUDE LIB LIBPATH
endif

BUILDDIR := build/$(ARCH)
DLLNAME  := restifycapl-$(ARCH).dll
DLL      := $(BUILDDIR)/$(DLLNAME)

# ------------------------------------------------------------------------------
# Versioning -- see the msvc-build-conventions skill for the full spec. No
# version number is ever hand-edited: everything here derives from the Git
# tag (release builds, via VER_* overrides from the environment/CI) or from
# commit counts (local dev builds, computed below).
# ------------------------------------------------------------------------------

# Human-readable build identifier for StringFileInfo. Works even with zero
# tags in this repo's history: --tags picks up lightweight tags (not just
# annotated ones), --always falls back to the abbreviated commit hash if no
# tag exists at all, so this command never fails.
GIT_DESCRIBE := $(shell git describe --tags --always --dirty)

# Numeric placeholder for local dev builds: major.minor stays 0.0 (no tag
# context to draw a real major.minor from), and the revision field
# increments with the number of commits since the last tag -- or, if no tag
# exists yet anywhere in history (this repo's current state), since the
# first commit. That fallback is not a special case in the formula: "commits
# since the last tag" with no last tag naturally degenerates to "commits
# since the beginning".
LAST_TAG := $(shell git describe --tags --abbrev=0 2>NUL)
ifeq ($(strip $(LAST_TAG)),)
  VER_COMMIT_COUNT := $(shell git rev-list --count HEAD)
else
  VER_COMMIT_COUNT := $(shell git rev-list --count $(LAST_TAG)..HEAD)
endif

# Release builds override these four from the tag; local dev builds fall
# through to the placeholders. Never fed anything but small
# integers here -- FILEVERSION/PRODUCTVERSION are four 16-bit fields that
# wrap silently above 65535.
VER_MAJOR ?= 0
VER_MINOR ?= 0
VER_BUILD ?= 0
VER_REV   ?= $(VER_COMMIT_COUNT)

# ------------------------------------------------------------------------------
# Sources. $(wildcard ...) is re-evaluated on every `make` invocation (not
# cached across runs), so real sources are picked up automatically the
# moment they exist; nothing here needs editing when they do.
# ------------------------------------------------------------------------------

SRC_DIRS := src/core src/http src/registry src/mapping src/module
SRCS     := $(wildcard $(addsuffix /*.cpp,$(SRC_DIRS)))
OBJS     := $(patsubst src/%.cpp,$(BUILDDIR)/obj/%.obj,$(SRCS))

# -include, not include: silently does nothing on a fresh checkout where no
# .d files exist yet -- see GEN_DEPFILE below for how they're produced.
-include $(wildcard $(BUILDDIR)/obj/*/*.d)

INCLUDES := /I include /I include/vendor /I include/vendor/capl-dll-sdk /I src

# Required by curl.h when linking the static libcurl.lib: without it, curl.h
# declares every entry point __declspec(dllimport) and the link fails with
# LNK2019 on __imp_curl_*. Defined once here so the product and test compile
# paths below cannot drift.
CURL_DEFINES := /D CURL_STATICLIB

# /std:c++17, /EHsc and /MT are identical across architectures by design --
# see msvc-build-conventions. /MT is mandatory; never change to /MD here.
CXXFLAGS := /nologo /c /std:c++17 /EHsc /MT /W4 $(INCLUDES) $(CURL_DEFINES)

# Windows system libs required transitively by libcurl -- link all of them,
# always (see msvc-build-conventions). version.lib is also pulled in via
# exports.cpp's own #pragma comment; listed here too so the product's full
# external-import-lib set stays visible in one place. iphlpapi.lib resolves
# if_nametoindex, pulled in by this libcurl build's IPv6 scope-ID handling --
# missing it produces LNK2019 on __imp__if_nametoindex only once a real curl
# symbol is referenced, confirmed by a real link.
SYSLIBS := crypt32.lib bcrypt.lib secur32.lib ws2_32.lib normaliz.lib wldap32.lib advapi32.lib iphlpapi.lib version.lib
LIBS    := libcurl.lib zs.lib $(SYSLIBS)

VERSION_RC  := src/module/version.rc
VERSION_RES := $(BUILDDIR)/version.res

DEF_FILE := src/module/exports.def

# ------------------------------------------------------------------------------
# Header dependency tracking -- GEN_DEPFILE is called from every .obj pattern
# rule below (product and test) so an incremental build-x86/build-x64/test
# rebuilds whatever a changed header transitively touches.
# ------------------------------------------------------------------------------

# Editing trap: never chain more commands with `&` after an `if COND (...)`
# inside the for /f loop below. On this cmd.exe, once COND is false in a
# parenthesized FOR body, everything after that if -- not just its own
# action -- silently stops running. Use `cmd && (set VAR=1)` twice instead
# of `if not defined VAR (cmd && ...)`, and keep any trailing
# `if defined ...` as the last statement in the block. Filters cl.exe's
# /sourceDependencies JSON down to src/include headers only: system header
# paths contain spaces GNU Make 3.81 can't express as a prerequisite; a
# missing/stale .d is harmless since src/%.cpp is still a prerequisite.
# findstr, unlike cl.exe/rc.exe/mkdir/> below, cannot open a forward-slash
# path -- $(subst /,\,...) is needed only on the two findstr file arguments.
define GEN_DEPFILE
@setlocal EnableDelayedExpansion & set "PFX_SRC=%CD%\src\\" & set "PFX_INC=%CD%\include\\" & type nul > "$(@:.obj=.d)" & (for /f "usebackq tokens=* delims=" %%L in (`findstr /C:"\\\\" "$(subst /,\,$(@:.obj=.d.json))"`) do (set "LINE=%%L" & for /f "tokens=* delims= " %%T in ("!LINE!") do set "LINE=%%T" & set "LINE=!LINE:,=!" & set "LINE=!LINE:~1,-1!" & set "LINE=!LINE:\\=\!" & (echo !LINE!)>"$(@:.obj=.line)" & set "MATCH=" & findstr /B /I /C:"!PFX_SRC!" "$(subst /,\,$(@:.obj=.line))" >nul && (set "MATCH=1") & findstr /B /I /C:"!PFX_INC!" "$(subst /,\,$(@:.obj=.line))" >nul && (set "MATCH=1") & if defined MATCH (echo $@: !LINE!>>"$(@:.obj=.d)")) & del "$(subst /,\,$(@:.obj=.line))")
endef

# ------------------------------------------------------------------------------
# Phony targets
# ------------------------------------------------------------------------------

.PHONY: help all build-x86 build-x64 _build test clean

help:
	@echo Targets:
	@echo   build-x64   build build/x64/restifycapl-x64.dll
	@echo   build-x86   build build/x86/restifycapl-x86.dll
	@echo   test        build and run the tests, ARCH=x64 or ARCH=x86, default x64
	@echo   all         build-x86 and build-x64
	@echo   clean       remove build/
	@echo   help        show this list

all: build-x86 build-x64

# Thin wrappers: each just re-invokes make with ARCH set, so build-x86 and
# build-x64 share the exact same recipe (_build, and everything it depends
# on) below. A fresh recursive `make` re-evaluates the whole file with ARCH
# bound to the requested architecture, which is what makes the per-ARCH
# variable block above the single source of truth for both targets.
build-x86:
	@$(MAKE) --no-print-directory _build ARCH=x86

build-x64:
	@$(MAKE) --no-print-directory _build ARCH=x64

_build: $(DLL)
	@echo Built $(DLL) (ARCH=$(ARCH), version $(VER_MAJOR).$(VER_MINOR).$(VER_BUILD).$(VER_REV) / $(GIT_DESCRIBE))

$(DLL): $(OBJS) $(VERSION_RES) $(DEF_FILE)
	link.exe /nologo /DLL /MACHINE:$(MACHINE) /VERSION:$(VER_MAJOR).$(VER_MINOR) /DEF:$(DEF_FILE) /OUT:"$@" $(OBJS) $(VERSION_RES) /LIBPATH:$(LIBDIR) $(LIBS)

# NOTE (dry-run cosmetic quirk, confirmed harmless): `make -n` against this
# target (or anything that depends on it, e.g. `make -n build-x64`) prints
# one spurious extra line after the real recipe:
#   process_begin: CreateProcess(NULL, "", ...) failed.
# Root cause, isolated with toy Makefiles: on this GNU Make 3.81 Windows
# port, each top-level $(shell ...) call above (GIT_DESCRIBE, LAST_TAG,
# VER_COMMIT_COUNT) is evaluated at parse time regardless of -n; those whose
# command string needs a real cmd.exe (i.e. contains shell metacharacters
# such as I/O redirection) leave behind a stale job/process-slot that gets
# replayed -- once per such call -- as a bogus zero-argument CreateProcess
# the next time -n would start a recipe. Only LAST_TAG's `2>NUL` redirection
# (line ~73) qualifies here, which is why exactly one such line appears, and
# only under -n. It never appears in real (non-dry-run) runs, never affects
# the exit code (always 0), and disappears entirely if LAST_TAG's command is
# changed to not need a shell -- but that edit touches the versioning
# mechanism, which is out of scope for this fix; do not "fix" this by
# altering the $(shell ...) calls above without going through the
# versioning mechanism's owner. Left as-is intentionally -- do not
# re-investigate from scratch.
$(VERSION_RES): $(VERSION_RC)
	@if not exist "$(BUILDDIR)" mkdir "$(BUILDDIR)"
	rc.exe /nologo /D VER_MAJOR=$(VER_MAJOR) /D VER_MINOR=$(VER_MINOR) /D VER_BUILD=$(VER_BUILD) /D VER_REV=$(VER_REV) /D VER_STRING="\"$(GIT_DESCRIBE)\"" /I include /fo "$@" "$<"

$(BUILDDIR)/obj/%.obj: src/%.cpp
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	cl.exe $(CXXFLAGS) /sourceDependencies "$(@:.obj=.d.json)" /Fo"$@" "$<"
	$(GEN_DEPFILE)

# ------------------------------------------------------------------------------
# test -- builds and runs the GoogleTest suite outside CANoe. Only
# src/core, src/http, src/registry and src/mapping are compiled in --
# src/module is CAPL-specific export glue that tests never link against, per
# the project's dependency-direction rule (only src/module/ includes the
# CAPL SDK headers; logic must be reachable without the export glue).
#
# `make test` uses ARCH the same way the build targets do (default x64,
# override with `make test ARCH=x86`) but is not itself required to be a
# thin wrapper -- that hard requirement is specific to build-x86/build-x64.
# ------------------------------------------------------------------------------

TEST_SRC_DIRS    := src/core src/http src/registry src/mapping
TEST_LOGIC_SRCS  := $(wildcard $(addsuffix /*.cpp,$(TEST_SRC_DIRS)))
TEST_CASE_SRCS   := $(wildcard tests/core/*.cpp tests/http/*.cpp tests/mapping/*.cpp)

TEST_BUILDDIR    := build/test/$(ARCH)
TEST_LOGIC_OBJS  := $(patsubst src/%.cpp,$(TEST_BUILDDIR)/obj/src/%.obj,$(TEST_LOGIC_SRCS))
TEST_CASE_OBJS   := $(patsubst tests/%.cpp,$(TEST_BUILDDIR)/obj/tests/%.obj,$(TEST_CASE_SRCS))
TEST_OBJS        := $(TEST_LOGIC_OBJS) $(TEST_CASE_OBJS)

-include $(wildcard $(TEST_BUILDDIR)/obj/src/*/*.d $(TEST_BUILDDIR)/obj/tests/*/*.d)

# GoogleTest headers are architecture-agnostic and live alongside the other
# vendored third-party headers; only the .lib binaries are per-architecture
# (lib/gtest/x86, lib/gtest/x64 -- see msvc-build-conventions).
TEST_INCLUDES := /I include /I include/vendor /I src
TEST_CXXFLAGS := /nologo /c /std:c++17 /EHsc /MT /W4 $(TEST_INCLUDES) $(CURL_DEFINES)
TEST_LIBS     := gtest.lib gtest_main.lib $(LIBS)
TEST_EXE      := $(TEST_BUILDDIR)/restifycapl-tests.exe

test: $(TEST_EXE)
	"$(TEST_EXE)"

# /SUBSYSTEM:CONSOLE is required here (unlike the product DLL link above):
# link.exe infers the subsystem only from main/WinMain in .obj files given
# directly on the command line, never from symbols pulled in transitively
# from a .lib. main() here comes solely from gtest_main.lib, so without an
# explicit /SUBSYSTEM the linker can't infer one and fails with LNK1561
# ("entry point must be defined") -- confirmed by reproducing the failure
# and fix with a real link.exe invocation.
$(TEST_EXE): $(TEST_OBJS)
	link.exe /nologo /SUBSYSTEM:CONSOLE /MACHINE:$(MACHINE) /OUT:"$@" $(TEST_OBJS) /LIBPATH:$(GTESTDIR) /LIBPATH:$(LIBDIR) $(TEST_LIBS)

$(TEST_BUILDDIR)/obj/src/%.obj: src/%.cpp
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	cl.exe $(TEST_CXXFLAGS) /sourceDependencies "$(@:.obj=.d.json)" /Fo"$@" "$<"
	$(GEN_DEPFILE)

$(TEST_BUILDDIR)/obj/tests/%.obj: tests/%.cpp
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	cl.exe $(TEST_CXXFLAGS) /sourceDependencies "$(@:.obj=.d.json)" /Fo"$@" "$<"
	$(GEN_DEPFILE)

# ------------------------------------------------------------------------------
# clean -- removes ALL intermediates under build/ (both architectures' DLL
# output, .obj/.res intermediates, and the test build tree), not a selected
# few. Nothing outside build/ is ever a build intermediate by design.
# ------------------------------------------------------------------------------

.PHONY: clean
clean:
	@if exist build rmdir /s /q build
