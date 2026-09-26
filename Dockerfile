# syntax=docker/dockerfile:1
# Dockerfile
#
# Leta's only deployment artifact is this image (ADR-010): a multi-stage build where a full
# toolchain compiles `leta` and a minimal glibc runtime layer ships it. This is the M0-T7 skeleton —
# the shape FR-67 needs (non-root, declared volume, one entrypoint) with two blanks that later
# milestones fill:
#
#   HEALTHCHECK    needs GET /health (M1) and a probe that runs without a shell or curl, because
#                  the runtime layer has neither. Planned form: `leta --healthcheck` (M12, FR-67).
#   Runtime base   ADR-010 settles it at M12 by measuring NFR-01 latency and NFR-13 size against
#                  the Alpine/static alternative. Until then it is the ARG below, so the comparison
#                  is a --build-arg, not a rewrite.
#
# Build and run (README, "Container image"):
#
#   docker build -t leta:dev --build-arg GIT_COMMIT=$(git rev-parse --short=7 HEAD) .
#   docker run --rm leta:dev --version
#
# Multi-arch is `docker buildx build --platform linux/amd64,linux/arm64 …`. Nothing below is
# architecture-specific: each platform's build stage installs its native toolchain.

# Builder and runtime come from the same Debian release, so the glibc and libstdc++ the binary
# links against are the ones the runtime layer carries. Trixie's GCC 14 sits above the NFR-09
# floor that CI enforces (GCC 13, Clang 17): CI proves the floor, the image proves a newer
# toolchain. A runtime without a shell or a user database still works with this file — see USER.
ARG BUILD_IMAGE=debian:trixie
ARG RUNTIME_IMAGE=gcr.io/distroless/cc-debian13:nonroot

# ---- Stage 1: build ---------------------------------------------------------------------------
FROM ${BUILD_IMAGE} AS build

# ca-certificates: the configure step downloads the pinned dependency tarballs over https
# (cmake/LetaDependencies.cmake). binutils (strip) arrives with g++.
RUN apt-get update -q \
    && apt-get install -y -q --no-install-recommends \
        ca-certificates \
        cmake \
        g++ \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# The context has no .git (.dockerignore), so cmake/LetaBuildInfo.cmake would report "unknown".
# The caller passes the commit in; an empty ARG keeps "unknown", which is the honest answer for a
# build from a source tarball. Declared after COPY so that changing it never invalidates the
# toolchain layer.
ARG GIT_COMMIT=""

# Only the `leta` target: tests are CI's job (docs/05-quality-strategy.md §10), and building Catch2
# here would double the build for nothing that ships. The cache mount keeps the downloaded
# dependency tarballs across builds; CPM reads CPM_SOURCE_CACHE itself.
RUN --mount=type=cache,target=/root/.cache/CPM \
    export CPM_SOURCE_CACHE=/root/.cache/CPM \
    && cmake --preset release -DLETA_GIT_COMMIT_OVERRIDE="${GIT_COMMIT}" \
    && cmake --build --preset release --target leta \
    && strip --strip-unneeded build/release/bin/leta \
    && mkdir -p /out/data \
    && install -m 0755 build/release/bin/leta /out/leta

# ---- Stage 2: runtime -------------------------------------------------------------------------
FROM ${RUNTIME_IMAGE}

ARG GIT_COMMIT=""
LABEL org.opencontainers.image.title="leta" \
      org.opencontainers.image.description="Small, fast, self-hosted product search server" \
      org.opencontainers.image.source="https://github.com/Samir120/leta" \
      org.opencontainers.image.licenses="Apache-2.0" \
      org.opencontainers.image.revision="${GIT_COMMIT}"

# The binary stays root-owned and read-only to the runtime user; only the data directory is
# theirs. Copying an empty directory is how /data gets created and chowned on a base that has no
# mkdir or chown to run.
COPY --from=build /out/leta /usr/local/bin/leta
COPY --from=build --chown=65532:65532 /out/data /data

# FR-63 name (A10). Nothing reads it until M9; it documents the contract the volume relies on.
ENV LETA_DATA_DIR=/data

VOLUME ["/data"]
EXPOSE 7700

# Numeric on purpose (NFR-08): the runtime layer may carry no /etc/passwd, and a fallback base
# such as debian:trixie-slim has no `nonroot` user. 65532 is distroless's nonroot uid.
USER 65532:65532

ENTRYPOINT ["/usr/local/bin/leta"]
