# ============================================================================
# STAGE 1: BUILDER (Heavyweight environment with compilers)
# ============================================================================
FROM ubuntu:24.04 AS builder

# Install the C++ compiler, curl, json, and ASIO networking libraries
RUN apt-get update && apt-get install -y \
    g++ \
    make \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libasio-dev \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /src

# Copy all source code and headers (cpp, crow_all.h, json.hpp)
COPY . .

# Create the bin directory and compile the OOP code
RUN mkdir -p bin && \
    g++ -std=c++20 -Wall -Wextra telemetry_engine_classes.cpp -o bin/telemetry_engine_classes -lcurl -pthread

# ============================================================================
# STAGE 2: RUNTIME (Lightweight production image)
# ============================================================================
FROM ubuntu:24.04

# Install ONLY the curl runtime library (no compilers here)
RUN apt-get update && apt-get install -y libcurl4 && rm -rf /var/lib/apt/lists/*

# Configure the working directory for the binary (vital for the "../.env" relative path to work)
WORKDIR /app/bin

# Copy ONLY the compiled binary from Stage 1 to this final image
COPY --from=builder /src/bin/telemetry_engine_classes .

# Expose port 8080 to the outside world
EXPOSE 8080

# Startup command for the microservice
CMD ["./telemetry_engine_classes"]