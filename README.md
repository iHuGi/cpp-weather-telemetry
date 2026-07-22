# C++ Weather Telemetry API

A high-performance microservice architecture designed to ingest real-time atmospheric data, process the telemetry stream, and serve it via a local REST API. This system demonstrates the power of C++20 for network-bound tasks, utilising **libcurl** for non-blocking network I/O and the **Crow** framework for high-concurrency web delivery.

---

### 1. Architectural Stack

* **Language:** C++20
* **Network I/O:** `libcurl` (Asynchronous/Multiplexed HTTP via `curl_multi`)
* **Web Framework:** `Crow` (Multithreaded REST API)
* **Data Parsing:** `nlohmann/json`
* **Concurrency:** `std::thread`, `std::mutex`, `std::lock_guard`
* **Infrastructure:** Docker Compose / Linux / WSL2

---

### 2. Key Features

* **Thread-Safe In-Memory Cache:** Decouples the background data ingestion from the API response layer, ensuring zero-latency reads for incoming HTTP requests.
* **API Rate Limiting Protection:** The backend worker sleeps between execution cycles, maintaining a strict daily quota (tracked dynamically) while serving cached telemetry instantly.
* **Observability Metrics:** Exposes system health data alongside the weather payload, including `last_update` (last successful background fetch) and `last_attempt` (last API request heartbeat).

---

### 3. System Deployment

The system supports multiple deployment modes, ranging from local compilation to declarative container orchestration.

#### Option A: Native Build

Uses the included `Makefile` to manage compilation and memory-safe binary generation.

```bash
# Compile the microservice
make

# Execute (Ensure .env is in the project root or parent directory)
./bin/telemetry_engine_classes

```

#### Option B: Containerized Deployment via Shell Scripts

Utilises a **Multi-Stage Docker Build** to encapsulate the environment, keeping the final production image lightweight and secure.

```bash
# Build the Image
docker build -t cpp-weather-api .

# Start the service (requires execution permissions: chmod +x run_container.sh)
./legacy/run_container.sh

# Stop the service
./legacy/stop_service.sh

```

#### Option C: Docker Compose Deployment (HIGHLY RECOMMENDED 💥)

The modern, declarative Infrastructure-as-Code (IaC) approach. Manages the build process, environment variable injection, and port mapping seamlessly.

```bash
# Build the image and start the service in the background
docker compose up -d --build

# View real-time container logs
docker compose logs -f

# Stop and gracefully remove the container and network
docker compose down

```

> **Engineering Note:** In all modes, the application binds to port `8080`. The system defaults to `DEV` mode (open CORS). To run in production mode with strict CORS boundaries, pass the `--prod` argument to the binary, or uncomment the `command: ["--prod"]` directive in the `docker-compose.yml`.

---

### 4. API Specification

The system provides a single, high-throughput endpoint for telemetry consumption, plus a health probe.

* **Telemetry Endpoint:** `/api/weather`
* **Health Endpoint:** `/health`
* **Method:** `GET`
* **Format:** `JSON`
* **CORS:** Dynamic (Open `*` in development; restricted to frontend URL when using `--prod`).

**Payload Example:**

```json
{
    "calls": 20,
    "last_attempt": 1718744500,
    "last_update": 1718740900,
    "data": [
        {
            "city": "Lisbon",
            "temp": 28.64,
            "temp_max": 29.10,
            "temp_min": 27.85,
            "condition": "Clear",
            "lat": 38.7167,
            "lon": -9.1333
        }
    ]
}

```

---

### 5. Frontend Integration

The project includes a lightweight, dark-mode dashboard (`dashboard.html` / JS) designed for low-latency visual telemetry.

**Deployment:**

1. Ensure the C++ backend is active.
2. Open the `dashboard.html` file directly in your preferred browser, or host it locally via a development server (e.g., VS Code Live Server).
3. The UI will automatically fetch the payload and parse the UNIX timestamps into local timezones.

---

### 6. Repository Hygiene

* **Environment Variables:** All secrets and API keys are strictly excluded via `.gitignore`. Ensure a `.env` file exists with the `WEATHER_API` key defined.
* **Binary Isolation:** All compiled objects and executables are siloed in the `bin/` directory.
* **Infrastructure as Code:** Docker Compose handles container orchestration, while legacy deployment shell scripts are cleanly archived in the `legacy/` directory for reference.