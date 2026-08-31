#!/usr/bin/env bash
# Propagates bindings_version.txt (the binding-package version, semver,
# independent of the XML schema version in version.txt) into
# python/pyproject.toml and js/package.json.
#
# Deliberately does NOT touch version.txt or derive bindings_version.txt from
# it: the schema version has external meaning to scenario authors and is not
# coupled to binding release cadence (a schema bump doesn't necessarily
# warrant a new binding release, and vice versa). See the "# Version" section
# of the root README.md for the full rationale.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BINDINGS_VERSION="$(tr -d '[:space:]' <"${REPO_DIR}/bindings_version.txt")"
SCHEMA_VERSION="$(tr -d '[:space:]' <"${REPO_DIR}/version.txt")"

PYPROJECT="${REPO_DIR}/python/pyproject.toml"
PACKAGE_JSON="${REPO_DIR}/js/package.json"

echo "> Syncing bindings_version.txt (${BINDINGS_VERSION}) into pyproject.toml and package.json"

# BSD sed (macOS) requires -i '' ; GNU sed requires -i (no argument). Detect
# which one we've got rather than hardcoding a platform.
sed_inplace() {
  if sed --version >/dev/null 2>&1; then
    sed -i "$@" # GNU sed
  else
    sed -i '' "$@" # BSD sed
  fi
}

sed_inplace -E "s/^version = \"[^\"]*\"/version = \"${BINDINGS_VERSION}\"/" "${PYPROJECT}"
sed_inplace -E "s/\"version\": \"[^\"]*\"/\"version\": \"${BINDINGS_VERSION}\"/" "${PACKAGE_JSON}"

echo "> python/pyproject.toml and js/package.json now at version ${BINDINGS_VERSION}"

# ---  Schema-version cross-reference  ---
#
# Rewrites the "schema version NN" number already present in each binding
# manifest's description field to match version.txt, without touching the
# rest of the description text or coupling the binding package version
# (above) to the schema version.
schema_number="${SCHEMA_VERSION#schema-}"
schema_number="${schema_number%.*}"

sed_inplace -E "s/schema version [0-9]+/schema version ${schema_number}/" "${PYPROJECT}"
sed_inplace -E "s/schema version [0-9]+/schema version ${schema_number}/" "${PACKAGE_JSON}"

echo "> python/pyproject.toml and js/package.json now reference schema version ${schema_number}"
