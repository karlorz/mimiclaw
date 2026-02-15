#!/usr/bin/env bash
set -euo pipefail

brew update
brew install cmake pkg-config curl libwebsockets cjson
