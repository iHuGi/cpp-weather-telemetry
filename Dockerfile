# ============================================================================
# STAGE 1: BUILDER
# ============================================================================
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    g++ \
    make \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libasio-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN mkdir -p bin && \
    g++ -std=c++20 -Wall -Wextra \
    telemetry_engine_classes.cpp \
    -o bin/telemetry_engine_classes \
    -lcurl -pthread


# ============================================================================
# STAGE 2: RUNTIME
# ============================================================================
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    libcurl4 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app/bin

COPY --from=builder /src/bin/telemetry_engine_classes .

EXPOSE 8080

ENTRYPOINT ["./telemetry_engine_classes"]

# Default arguments passed to the main() function.
# To alternate environments, override this CMD during 'docker run':
#   -> DEV MODE  (Default open CORS) : CMD []
#   -> PROD MODE (Restricted CORS)   : CMD ["--prod"]
CMD []