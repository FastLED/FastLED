FROM ghcr.io/astral-sh/uv:0.8.0-python3.12-bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends clang llvm valgrind \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 1000 audit \
    && mkdir -p /build-volume /venv-volume /home/audit/.cache/uv \
    && chown -R audit:audit /build-volume /venv-volume /home/audit/.cache/uv

ENV UV_CACHE_DIR=/home/audit/.cache/uv

WORKDIR /repo

USER audit
