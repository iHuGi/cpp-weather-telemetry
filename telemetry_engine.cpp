#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "crow_all.h"
#include <chrono>
#include <mutex>
#include <thread>
#include <ctime>

using namespace std;
using json = nlohmann::json;

/**
 * @struct WeatherCache
 * @brief Thread-safe in-memory cache for weather data.
 * * Stores the weather payload and management metadata.
 * Uses a mutex to ensure thread safety between the background refresh worker 
 * and the HTTP API frontend.
 */
struct WeatherCache {
    crow::json::wvalue::list data;
    
    /// @brief Timestamp of the last successful refresh.
    std::chrono::system_clock::time_point last_update = 
        std::chrono::system_clock::now() - std::chrono::hours(2);
    
    std::chrono::system_clock::time_point last_attempt = 
        std::chrono::system_clock::now() - std::chrono::hours(2);

    /// @brief Counter to track OpenWeather API daily request quota usage.
    int daily_calls = 0;

    /// @brief Mutex to prevent data races during concurrent read/write operations.
    std::mutex mtx;
} cache;

/**
 * @brief Loads the OpenWeather API key from a local .env file.
 * * Parses the file line-by-line, looking for the WEATHER_API key.
 * Sanitizes whitespace and quotes for secure string extraction.
 * * @return string The sanitized API key or empty string if not found.
 */
string load_api_key_from_env() {
    ifstream file("../.env");
    string line;
    if (!file.is_open()) return "";
    while (getline(file, line)) {
        size_t pos = line.find('=');
        if (pos == string::npos) continue;
        string key = line.substr(0, pos);
        string value = line.substr(pos + 1);
        key.erase(key.find_last_not_of(" \t") + 1);
        if (key == "WEATHER_API") {
            size_t first = value.find_first_not_of(" \t\"");
            size_t last  = value.find_last_not_of(" \t\"");
            if (first != string::npos && last != string::npos)
                return value.substr(first, last - first + 1);
        }
    }
    return "";
}

/**
 * @brief Callback for libcurl to append HTTP response chunks.
 * * @param contents Pointer to the delivered data chunk.
 * @param size Size of one data item.
 * @param nmemb Number of items.
 * @param s Target string buffer to store the payload.
 * @return size_t Number of bytes processed.
 */
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* s) {
    size_t total = size * nmemb;
    try { s->append((char*)contents, total); return total; } 
    catch (...) { return 0; }
}

/**
 * @brief Background worker thread that refreshes weather telemetry.
 * * Executes an infinite loop, performing parallel HTTP requests using libcurl's
 * multi-interface to minimize latency. Updates the global cache and sleeps.
 * * @param api_key The OpenWeather API key required for requests.
 */
void weather_refresh_worker(const string& api_key) {
    const vector<string> cities = {
        "Aveiro","Beja","Braga","Braganca","Castelo Branco",
        "Coimbra","Evora","Faro","Funchal","Guarda",
        "Leiria","Lisbon","Ponta Delgada","Portalegre","Porto",
        "Santarem","Setubal","Viana do Castelo","Vila Real","Viseu"
    };

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
                    city["condition"] = string(data["weather"][0]["main"]);
                    new_data.push_back(move(city));
                }
            } catch (...) {}
        }

        {
            lock_guard<mutex> lock(cache.mtx);

            // Reset daily API call counter at midnight to maintain accurate quota tracking
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            auto last_time_t = std::chrono::system_clock::to_time_t(cache.last_update);
            
            std::tm now_tm;
            std::tm last_tm;

            localtime_r(&now_time_t, &now_tm);
            localtime_r(&last_time_t, &last_tm);

            if (now_tm.tm_mday != last_tm.tm_mday || now_tm.tm_mon != last_tm.tm_mon || now_tm.tm_year != last_tm.tm_year) {
                cache.daily_calls = 0;
            }
            
            // Update cache only if valid data was received to prevent overwriting existing data with empty payloads
            if (!new_data.empty()) {
                cache.data = move(new_data);
                cache.last_update = chrono::system_clock::now();
            }
            cache.daily_calls += static_cast<int>(cities.size());
        }
        this_thread::sleep_for(chrono::hours(1));
    }
}

/**
 * @brief Entry point for the weather microservice.
 * * Initializes the environment, spawns the background refresh thread,
 * and boots the Crow web application instance.
 */
int main() {
    string api_key = load_api_key_from_env();
    if (api_key.empty()) { cerr << "CRITICAL: WEATHER_API missing." << endl; return 1; }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    thread(weather_refresh_worker, api_key).detach();

    crow::SimpleApp app;

    CROW_ROUTE(app, "/api/weather")([]() {
        lock_guard<mutex> lock(cache.mtx);

        cache.last_attempt = std::chrono::system_clock::now();
        
        auto last_update_sec = std::chrono::duration_cast<std::chrono::seconds>(
            cache.last_update.time_since_epoch()).count();

        auto last_attempt_sec = std::chrono::duration_cast<std::chrono::seconds>(
            cache.last_attempt.time_since_epoch()).count();
        
        crow::json::wvalue response_obj;
        response_obj["calls"] = cache.daily_calls; // Added here for front-end visibility
        response_obj["last_update"] = last_update_sec; // Added here for front-end visibility
        response_obj["last_attempt"] = last_attempt_sec; // Added here for front-end visibility
        response_obj["data"] = crow::json::wvalue(cache.data);
        
        crow::response res(response_obj);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    cout << "Server running on http://localhost:8080/api/weather" << endl;
    app.port(8080).multithreaded().run();
    curl_global_cleanup();
    return 0;
}