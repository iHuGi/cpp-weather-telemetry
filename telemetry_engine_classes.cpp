#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "crow_all.h"
#include <chrono>
#include <mutex>
#include <thread>
#include <ctime>

using namespace std;
using json = nlohmann::json;

// Centralized configuration for CORS origin validation in production
const string FRONTEND_URL = "https://weather.hugo.pt";

// ============================================================================
// 1. ENV LOADER CLASS
// ============================================================================
class EnvLoader {
public:
    // Parses the specified environment file and securely extracts the API key
    static string getApiKey(const string& path) {
        ifstream file(path);
        string line;

        if (!file.is_open()) return "";

        while (getline(file, line)) {
            size_t pos = line.find('=');
            if (pos != string::npos) {
                string key = line.substr(0, pos);
                string value = line.substr(pos + 1);

                // Trims whitespace from the key
                key.erase(key.find_last_not_of(" \t") + 1);

                if (key == "WEATHER_API") {
                    // Trims whitespace and quotes from the value
                    size_t first = value.find_first_not_of(" \t\"");
                    size_t last = value.find_last_not_of(" \t\"");
                    if (first != string::npos && last != string::npos) {
                        return value.substr(first, (last - first + 1));
                    }
                }
            }
        }
        return "";
    }
};

// ============================================================================
// 2. CACHE MANAGER CLASS (Thread-Safe Data Storage)
// ============================================================================
class WeatherCache {
private:
    crow::json::wvalue::list data;
    std::chrono::system_clock::time_point last_update;
    std::chrono::system_clock::time_point last_attempt;
    int daily_calls = 0;
    std::mutex mtx;

public:
    // Initializes the cache timestamps with a backward offset to ensure an immediate first run
    WeatherCache() {
        last_update = std::chrono::system_clock::now() - std::chrono::hours(2);
        last_attempt = std::chrono::system_clock::now() - std::chrono::hours(2);
    }

    // Updates the internal memory with new API data and tracks daily quota limits
    void updateCache(crow::json::wvalue::list&& new_data, int calls_made) {
        lock_guard<mutex> lock(mtx);

        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        auto last_time_t = std::chrono::system_clock::to_time_t(last_update);

        std::tm now_tm;
        std::tm last_tm;
        localtime_r(&now_time_t, &now_tm);
        localtime_r(&last_time_t, &last_tm);

        // Resets the daily quota counter at midnight
        if (now_tm.tm_mday != last_tm.tm_mday || now_tm.tm_mon != last_tm.tm_mon || now_tm.tm_year != last_tm.tm_year) {
            daily_calls = 0;
        }

        if (!new_data.empty()) {
            data = std::move(new_data);
            last_update = now;
        }

        daily_calls += calls_made;
    }

    // Records the exact timestamp of a frontend API interaction
    void recordAttempt() {
        lock_guard<mutex> lock(mtx);
        last_attempt = std::chrono::system_clock::now();
    }

    // Safely extracts a comprehensive snapshot of the system state for JSON delivery
    crow::json::wvalue getSnapshot() {
        lock_guard<mutex> lock(mtx);
        auto last_update_sec = std::chrono::duration_cast<std::chrono::seconds>(last_update.time_since_epoch()).count();
        auto last_attempt_sec = std::chrono::duration_cast<std::chrono::seconds>(last_attempt.time_since_epoch()).count();

        crow::json::wvalue response_obj;
        response_obj["calls"] = daily_calls;
        response_obj["last_update"] = last_update_sec;
        response_obj["last_attempt"] = last_attempt_sec;
        response_obj["data"] = crow::json::wvalue(data);

        return response_obj;
    }
};

// ============================================================================
// 3. SERVICE WORKER CLASS (Async API Fetcher)
// ============================================================================
class WeatherFetcher {
private:
    // Handles the incoming data stream from the CURL requests
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* s) {
        size_t total = size * nmemb;
        try { s->append((char*)contents, total); return total; }
        catch (...) { return 0; }
    }

public:
    // Operates as an infinite background loop, orchestrating parallel CURL requests to OpenWeather
    static void startBackgroundWorker(
        const string& api_key,
        const vector<string>& cities,
        WeatherCache& cache_ref) {

        while (true) {

            crow::json::wvalue::list new_data;
            CURLM* multi = curl_multi_init();
            vector<CURL*> handles;
            vector<string> responses(cities.size());
            int still_running = 0;

            for (size_t i = 0; i < cities.size(); i++) {
                CURL* c = curl_easy_init();
                if (!c) continue;
                handles.push_back(c);

                char* encoded = curl_easy_escape(c, cities[i].c_str(), cities[i].size());
                string url = "https://api.openweathermap.org/data/2.5/weather?q=" + string(encoded) + "&appid=" + api_key + "&units=metric";
                curl_free(encoded);

                curl_easy_setopt(c, CURLOPT_URL, url.c_str());
                curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(c, CURLOPT_WRITEDATA, &responses[i]);
                curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
                curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 5L);

                curl_multi_add_handle(multi, c);
            }

            curl_multi_perform(multi, &still_running);

            int max_loops = 1000;
            while (still_running && max_loops--) {
                int numfds;
                curl_multi_wait(multi, nullptr, 0, 1000, &numfds);
                curl_multi_perform(multi, &still_running);
            }

            for (auto c : handles) {
                curl_multi_remove_handle(multi, c);
                curl_easy_cleanup(c);
            }

            curl_multi_cleanup(multi);
            
            for (const auto& resp : responses) {
                try {
                    auto data = json::parse(resp);
                    if (data.contains("cod") && data["cod"] == 200) {
                        crow::json::wvalue city;
                        city["city"] = string(data["name"]);
                        city["temp"] = data["main"]["temp"].get<double>();
                        city["temp_max"] = data["main"]["temp_max"].get<double>();
                        city["temp_min"] = data["main"]["temp_min"].get<double>();
                        city["condition"] = string(data["weather"][0]["main"]);
                        city["lon"] = data["coord"]["lon"].get<double>();
                        city["lat"] = data["coord"]["lat"].get<double>();
                        new_data.push_back(move(city));
                    }
                } catch (...) {}
            }

            cache_ref.updateCache(move(new_data), cities.size());

            // Pauses the worker thread for 1 hour to respect API rate limits
            this_thread::sleep_for(chrono::hours(1));
        }
    }
};

// ============================================================================
// 4. API SERVER CLASS (Crow Wrapper)
// ============================================================================
class WebServer {
private:
    crow::SimpleApp app;
    WeatherCache& cache_ref;
    int port;
    string environment;

public:
    // Injects dependencies and initializes the web framework
    WebServer(WeatherCache& cache, int p, const string& env)
        : cache_ref(cache), port(p), environment(env) {

        setupRoutes();
    }

    void setupRoutes() {

        // Provides a lightweight health probe for container orchestration tools
        CROW_ROUTE(app, "/health")([this]() {
            crow::json::wvalue response;
            response["status"] = "healthy";
            response["environment"] = environment;
            return response;
        });

        // Main telemetry endpoint with dynamic CORS boundaries
        CROW_ROUTE(app, "/api/weather")([this]() {
            cache_ref.recordAttempt();
            crow::response res(cache_ref.getSnapshot());

            // Evaluates environment to dictate browser security boundaries
            if (environment != "prod") {
                res.add_header("Access-Control-Allow-Origin", "*");
            } else {
                res.add_header("Access-Control-Allow-Origin", FRONTEND_URL);
            }
            return res;
        });
    }

    // Launches the multithreaded server instance and displays runtime context
    void run() {
        if (environment == "prod") {
            cout << "\n[PRODUCTION MODE] Server starting tightly secured on port " << port << "..." << endl;
        } else {
            cout << "\n[DEVELOPMENT MODE] Server running open on http://localhost:" << port << "/api/weather" << endl;
        }
        app.port(port).multithreaded().run();
    }
};

// ============================================================================
// 5. ENTRY POINT
// ============================================================================
int main(int argc, char * argv[]) {

    const int port = 8080;
    const string path_env = ".env";
    string current_env = "dev";

    // Enforces strict runtime arguments. Aborts immediately if compromised
    if (argc > 1) {
        string arg = argv[1];

        if (arg == "--prod") {
            current_env = "prod";
        } else {
            cerr << "\n[!] CRITICAL ABORT: Invalid argument '" << arg << "'. Only --prod is allowed." << endl;
            return 1;
        }
    }

    string key = EnvLoader::getApiKey(path_env);

    // Fallback mechanism: attempts relative parent directory for local binary execution
    if (key.empty()) {
        key = EnvLoader::getApiKey("../.env");
    }
    
    // Triggers a critical system abort if no environment configurations are found
    if (key.empty()) {
        cerr << "\n[!] CRITICAL ABORT: API Key not found. Checked '.env' and '../.env'" << endl;
        return 1;
    }

    const vector<string> cities = {
        "Aveiro","Beja","Braga","Braganca","Castelo Branco",
        "Coimbra","Evora","Faro","Funchal","Guarda",
        "Leiria","Lisbon","Ponta Delgada","Portalegre","Porto",
        "Santarem","Setubal","Viana do Castelo","Vila Real","Viseu"
    };

    curl_global_init(CURL_GLOBAL_DEFAULT);
    WeatherCache cacheManager;

    thread worker(
        WeatherFetcher::startBackgroundWorker,
        key,
        cities,
        ref(cacheManager)
    );

    worker.detach();

    WebServer server(cacheManager, port, current_env);
    server.run();

    curl_global_cleanup();
    return 0;
}