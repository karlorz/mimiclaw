.PHONY: host-build host-smoke host-run host-boot-live host-debug-live-macos host-log-live host-scrub-live host-ci host-ci-live-anthropic host-ci-live-openai host-ci-live-all firmware-ci ci-all

BUILD_HOST_DIR ?= build-host
HOST_LIVE_STATE_ROOT ?= $(CURDIR)/.tmp/mimiclaw-host-live

host-build:
	./scripts/host/build.sh $(BUILD_HOST_DIR)

host-smoke: host-build
	BUILD_DIR=$(BUILD_HOST_DIR) ./scripts/host/smoke.sh

host-run: host-build
	./$(BUILD_HOST_DIR)/mimiclaw-host --config $$HOME/.mimiclaw/config.json

host-boot-live: host-build
	BUILD_DIR=$(BUILD_HOST_DIR) ./scripts/host/boot-live.sh

host-debug-live-macos: host-build
	BUILD_DIR=$(BUILD_HOST_DIR) ./scripts/host/debug-live-macos.sh

host-log-live:
	tail -f ./.tmp/mimiclaw-host-live/host.log

host-scrub-live: host-build
	./$(BUILD_HOST_DIR)/mimiclaw-host --state-root "$(HOST_LIVE_STATE_ROOT)" --scrub-sessions

host-ci:
	BUILD_DIR=$(BUILD_HOST_DIR) ./scripts/host/ci.sh

host-ci-live-anthropic:
	BUILD_DIR=$(BUILD_HOST_DIR) LIVE_PROVIDER=anthropic ./scripts/host/ci.sh --live-provider anthropic

host-ci-live-openai:
	BUILD_DIR=$(BUILD_HOST_DIR) LIVE_PROVIDER=openai ./scripts/host/ci.sh --live-provider openai

host-ci-live-all:
	$(MAKE) host-ci-live-anthropic
	$(MAKE) host-ci-live-openai

firmware-ci:
	./scripts/firmware/ci.sh

ci-all: host-ci firmware-ci
