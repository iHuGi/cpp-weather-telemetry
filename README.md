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

Bro, o teu `README.md` já é profissional, mas **falta-lhe o toque de Midas**. Num repositório de engenharia, o Docker é a "cereja no topo do bolo". Se um recrutador ou outro engenheiro abrir isto, ele quer ver que tu sabes gerir ambientes isolados.

Aqui está exatamente como deves atualizar a secção **3. System Deployment** para incluir o Docker, mantendo o nível de autoridade técnica que já tens:

---

### Atualização sugerida para o `README.md`

Substitui a tua secção **3. System Deployment** por esta versão turbinada:

---

### 3. System Deployment
The system supports two deployment modes: **Native Compilation** for local development and **Containerized Deployment** for environment-agnostic execution.

#### Option A: Native Build
Uses the included `Makefile` to manage compilation and memory-safe binary generation.

```bash
# Compile the microservice
make

# Execute (Ensure .env is in the project root)
./bin/telemetry_engine

```

#### Option B: Containerized Deployment (Recommended)

Utilises **Docker** to encapsulate the environment, dependencies, and runtime, ensuring consistent performance across all platforms.

**1. Build the Image:**

```bash
docker build -t cpp-weather-api .

```

**2. Run the Container:**
Injects your local `.env` configuration at runtime via a volume mount:


```bash
Start the service:
Bash
./run_container.sh
# OR directly:
docker run -d -p 8080:8080 -v $(pwd)/.env:/app/.env --name weather-service cpp-weather-api

Stop the service:
Bash
./stop_service.sh
# OR directly:
docker rm -f weather-service 2>/dev/null

# **Engineering Note:** Before running the scripts for the first time, ensure they have execute permissions:
chmod +x run_container.sh stop_service.sh

```

> **Engineering Note:** In both modes, the application binds to port `8080`. For native execution, ensure the binary can locate the `../.env` file. For Docker, the container handles environment injection, making it the preferred method for deployment stability.

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