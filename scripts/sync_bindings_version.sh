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

# ---  Soft schema-version cross-reference check  ---
#
# Not an auto-sync: just a nudge. Warns (does not fail) if the schema version
# currently in version.txt isn't mentioned anywhere in either binding
# manifest, so a human can decide whether the binding's "targets schema
# version NN" description needs updating -- without forcing every schema
# bump to also bump the binding version, or vice versa.
schema_number="${SCHEMA_VERSION#schema-}"
schema_number="${schema_number%.*}"

if ! grep -q "schema version ${schema_number}" "${PYPROJECT}"; then
  echo "warning: ${PYPROJECT} does not mention 'schema version ${schema_number}' (version.txt says ${SCHEMA_VERSION}) -- update its description if the Python bindings now target a different schema version" >&2
fi

if ! grep -q "schema version ${schema_number}" "${PACKAGE_JSON}"; then
  echo "warning: ${PACKAGE_JSON} does not mention 'schema version ${schema_number}' (version.txt says ${SCHEMA_VERSION}) -- update its description if the JS bindings now target a different schema version" >&2
fi
