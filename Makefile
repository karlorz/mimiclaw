.PHONY: host-build host-smoke host-run host-ci firmware-ci ci-all

BUILD_HOST_DIR ?= build-host

host-build:
	./scripts/host/build.sh $(BUILD_HOST_DIR)

host-smoke: host-build
	BUILD_DIR=$(BUILD_HOST_DIR) ./scripts/host/smoke.sh

host-run: host-build
	./$(BUILD_HOST_DIR)/mimiclaw-host --config $$HOME/.mimiclaw/config.json

host-ci:
	BUILD_DIR=$(BUILD_HOST_DIR) ./scripts/host/ci.sh

firmware-ci:
	./scripts/firmware/ci.sh

ci-all: host-ci firmware-ci
