#!/usr/bin/env bash
# Usage: fetch-pr-releases.sh <output-file> < pull-request-numbers
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: fetch-pr-releases.sh <output-file>" >&2
  exit 2
fi

releases="$1"
arg_file="$RUNNER_TEMP/pr-releases-arg.json"
pull="$RUNNER_TEMP/release-pull.json"
commit="$RUNNER_TEMP/release-commit.json"
entry="$RUNNER_TEMP/release-entry.json"
errfile="$RUNNER_TEMP/release-fetch.err"

printf '{}' > "$releases"
while read -r n; do
  [ -n "$n" ] || continue
  code=0; gh api "repos/$GITHUB_REPOSITORY/pulls/$n" > "$pull" 2> "$errfile" || code=$?
  if [ "$code" -ne 0 ]; then
    if grep -q 'HTTP 404' "$errfile"; then
      printf '{"found":false}' > "$entry"
    else
      echo "::error::could not read pull request #$n (exit $code): $(tr '\r\n' '  ' < "$errfile")"
      exit 1
    fi
  else
    head_sha=$(jq -r '.head.sha' "$pull")
    if [[ ! "$head_sha" =~ ^[0-9a-f]{40}$ ]]; then
      echo "::error::unexpected head commit of pull request #$n: $head_sha"
      exit 1
    fi
    code=0; gh api "repos/$GITHUB_REPOSITORY/commits/$head_sha" > "$commit" 2> "$errfile" || code=$?
    if [ "$code" -eq 0 ]; then
      jq -c --slurpfile c "$commit" '{found: true, merged: .merged, mergeCommitSha: .merge_commit_sha, baseRef: .base.ref, baseRepo: .base.repo.full_name, headRef: .head.ref, headRepo: (.head.repo.full_name // null), headSha: .head.sha, headParents: [$c[0].parents[].sha], headTree: $c[0].commit.tree.sha}' "$pull" > "$entry"
    elif grep -q 'HTTP 404\|HTTP 422' "$errfile"; then
      jq -c '{found: true, merged: .merged, mergeCommitSha: .merge_commit_sha, baseRef: .base.ref, baseRepo: .base.repo.full_name, headRef: .head.ref, headRepo: (.head.repo.full_name // null), headSha: .head.sha, headParents: null, headTree: null}' "$pull" > "$entry"
    else
      echo "::error::could not read the head commit $head_sha of pull request #$n (exit $code): $(tr '\r\n' '  ' < "$errfile")"
      exit 1
    fi
  fi
  merged=$(jq -c --arg n "$n" --slurpfile e "$entry" '. + {($n): $e[0]}' "$releases")
  printf '%s' "$merged" > "$arg_file"
  mv "$arg_file" "$releases"
done
