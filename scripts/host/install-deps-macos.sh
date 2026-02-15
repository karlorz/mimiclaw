#!/usr/bin/env bash
set -euo pipefail

brew update
HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK=1 brew install cmake pkg-config curl libwebsockets cjson
