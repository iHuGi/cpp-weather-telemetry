```markdown
# C++ Weather Telemetry API

A high-performance microservice architecture designed to ingest real-time atmospheric data, process the telemetry stream, and serve it via a local REST API. This system demonstrates the power of C++20 for network-bound tasks, utilising **libcurl** for non-blocking network I/O and the **Crow** framework for high-concurrency web delivery.

---

### 1. Architectural Stack
* **Language:** C++20
* **Network I/O:** `libcurl` (Asynchronous/Multiplexed HTTP via `curl_multi`)
* **Web Framework:** `Crow` (Multithreaded REST API)
* **Data Parsing:** `nlohmann/json`
* **Concurrency:** `std::thread`, `std::mutex`, `std::lock_guard`
* **Infrastructure:** Linux/WSL2 (Target Runtime)

---

### 2. Key Features
* **Thread-Safe In-Memory Cache:** Decouples the background data ingestion from the API response layer, ensuring zero-latency reads for incoming HTTP requests.
* **API Rate Limiting Protection:** The backend worker sleeps between execution cycles, maintaining a strict daily quota (tracked dynamically) while serving cached telemetry instantly.
* **Observability Metrics:** Exposes system health data alongside the weather payload, including `last_update` (last successful background fetch) and `last_attempt` (last API request heartbeat).

---

### 3. System Deployment
To initiate the telemetry engine, ensure the development environment is configured with the required compiler dependencies.

**Build Process:**
The system uses an automated `Makefile` to manage compilation, linking, and memory-safe binary generation.

```bash
# Clean previous binaries
make clean

# Compile the microservice
make

```

**Execution:**
Execute the compiled binary from the project root to ensure the application correctly locates the `.env` configuration file.

```bash
./bin/telemetry_engine

```

> **Engineering Note:** The application is architected as a multithreaded server, binding to `localhost:8080`. Ensure the port is not in use by other background processes.

---

### 4. API Specification

The system provides a single, high-throughput endpoint for telemetry consumption.

* **Endpoint:** `/api/weather`
* **Method:** `GET`
* **Format:** `JSON`
* **CORS:** Enabled (`Access-Control-Allow-Origin: *`)

**Payload Example:**

```json
{
    "calls": 20,
    "last_attempt": 1718744500,
    "last_update": 1718740900,
    "data": [
        {
            "city": "Lisbon",
            "condition": "Clear",
            "temp": 28.64
        }
    ]
}

```

---

### 5. Frontend Integration

The project includes a lightweight, dark-mode dashboard (HTML/JS) designed for low-latency visual telemetry.

**Deployment:**

1. Ensure the C++ backend is active (`./bin/telemetry_engine`).
2. Open the `dashboard.html` file directly in your preferred browser, or host it locally via a development server (e.g., VSCode Live Server).
3. Click **"Validate Temperatures"** to trigger an asynchronous fetch request to the backend. The UI will automatically parse the UNIX timestamps into local timezones.

---

### 6. Repository Hygiene

* **Environment Variables:** All secrets and API keys are strictly excluded via `.gitignore`. Ensure a `.env` file exists in the root directory with the `WEATHER_API` key defined.
* **Binary Isolation:** All compiled objects and executables are siloed in the `bin/` directory.
* **Configuration:** The project utilises a manual build pipeline for optimal transparency into the compilation and linking process.

```

Faz o *commit*, dá o *push* e vai festejar que o projeto está fechado com chave de ouro! 🥩⚙️🚀

```