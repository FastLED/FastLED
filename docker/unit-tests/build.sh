#!/bin/bash
# Build and run FastLED unit tests in Docker
#
# Usage:
#   ./build.sh          # Build the image
#   ./build.sh run      # Build and run tests
#   ./build.sh shell    # Build and open interactive shell

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
IMAGE_NAME="fastled-unit-tests"

cd "$PROJECT_ROOT"

build_image() {
    echo "Building Docker image: $IMAGE_NAME"
    docker build -f docker/unit-tests/Dockerfile -t "$IMAGE_NAME" .
}

run_tests() {
    echo "Running unit tests in Docker..."
    docker run --rm -v "$PROJECT_ROOT:/fastled" "$IMAGE_NAME" \
        bash -c "cd /fastled && uv run test.py --cpp"
}

open_shell() {
    echo "Opening interactive shell..."
    docker run --rm -it -v "$PROJECT_ROOT:/fastled" "$IMAGE_NAME" /bin/bash
}

case "${1:-build}" in
    build)
        build_image
        ;;
    run)
        build_image
        run_tests
        ;;
    shell)
        build_image
        open_shell
        ;;
    *)
        echo "Usage: $0 {build|run|shell}"
        exit 1
        ;;
esac
