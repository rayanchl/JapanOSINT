# JapanOSINT native server. Not built by CI; see docs/ENVIRONMENT.md for every knob.
#   docker build -t japanosint .
#   docker run -p 4000:4000 -v japanosint-data:/data --env-file .env japanosint
#
# The binary bakes the checkout path in at compile time (-DJO_REPO_ROOT, used for
# schema.sql, grammars/, data/), so both stages use the SAME path, /app.

# ---- build ----------------------------------------------------------------
FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
# The package list is the one .github/workflows/ci.yml installs (minus ffmpeg,
# which is a runtime tool, and plus the compiler toolchain a CI runner ships).
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential pkg-config python3 ca-certificates \
      libcurl4-openssl-dev libssl-dev libmecab-dev mecab zlib1g-dev \
      mecab-ipadic-utf8 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY native/ native/
COPY grammars/ grammars/
WORKDIR /app/native
RUN make -j"$(nproc)" && rm -rf obj

# ---- runtime --------------------------------------------------------------
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates libcurl4 libssl3t64 libmecab2 mecab-ipadic-utf8 zlib1g ffmpeg \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --home-dir /data --shell /usr/sbin/nologin jo \
    && mkdir -p /data /app/native \
    && chown jo:jo /data
COPY --from=build /app/native/bin /app/native/bin
COPY --from=build /app/native/core/schema.sql /app/native/core/schema.sql
COPY --from=build /app/native/data /app/native/data
COPY --from=build /app/grammars /app/grammars
# Evidence, uploads and the breach corpus resolve under <repo root>/data.
RUN ln -s /data /app/data
ENV JO_DB=/data/japanmap.db \
    PORT=4000 \
    JO_BIND=0.0.0.0
# Binding 0.0.0.0 is for use behind a TLS-terminating proxy: the server has no TLS.
VOLUME /data
EXPOSE 4000
USER jo
WORKDIR /app
HEALTHCHECK --interval=30s --timeout=5s --start-period=120s \
  CMD ["/bin/sh", "-c", "exec 3<>/dev/tcp/127.0.0.1/${PORT:-4000} && printf 'GET /api/health HTTP/1.0\\r\\n\\r\\n' >&3 && grep -q ' 200 ' <&3"]
ENTRYPOINT ["/app/native/bin/japanosint"]
CMD ["--serve"]
