#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "crow_all.h" // The web framework for C++

using namespace std;
using json = nlohmann::json;

// ---------------------------------------------------------
// ENV FILE PARSER
// ---------------------------------------------------------
/**
 * @brief Parses a local .env file to safely extract the target API key.
 * * Since C++ lacks a native dotenv library, this function acts as a lightweight
 * file reader. It scans the file line by line, identifies key-value pairs,
 * and sanitizes the output by removing whitespace and quotation marks.
 * * @return string The sanitized API key, or an empty string if the key is not found.
 */
string load_api_key_from_env() {
    ifstream file("../.env"); // The program opens the .env file in the current working directory
    string line;
    
    if (!file.is_open()) {
        cerr << "[!] File Error: The system could not locate the .env file!" << endl;
        return "";
    }

    // The loop reads the file line by line until the end of the document
    while (getline(file, line)) {
        // The program searches the current line for the assignment operator (=)
        size_t pos = line.find('=');
        
        // If 'pos' is NOT equal to string::npos, it means the '=' was successfully found
        if (pos != string::npos) {
            
            // The string is split into two parts based on the position of the '='
            // substr(0, pos) starts at index 0 and extracts 'pos' amount of characters (The Key)
            string key = line.substr(0, pos);
            
            // substr(pos + 1) starts one character after the '=' and grabs the rest of the line (The Value)
            string value = line.substr(pos + 1);

            // The program trims trailing spaces from the key to ensure an accurate match
            key.erase(key.find_last_not_of(" \t") + 1);

            // The system checks if the extracted key matches the required target
            if (key == "WEATHER_API") {
                
                // The program searches for the first and last characters in the value
                // that are NOT spaces (\t) or quotation marks (\")
                size_t first = value.find_first_not_of(" \t\"");
                size_t last = value.find_last_not_of(" \t\"");
                
                // If the value is not empty or composed entirely of junk characters...
                if (first != string::npos && last != string::npos) {
                    // The system extracts only the valid characters.
                    // 'last - first + 1' calculates the exact length of the clean string.
                    return value.substr(first, (last - first + 1));
                }
            }
        }
    }
    return ""; // Returns an empty string if the file finishes and the key was never found
}

// ---------------------------------------------------------
// LIBCURL CALLBACK FUNCTION
// ---------------------------------------------------------
/**
 * @brief Appends incoming data chunks from libcurl into a target string buffer.
 * * libcurl does not return the entire HTTP response at once. Instead, the library
 * triggers this callback function multiple times as data arrives over the network.
 * * @param contents Pointer to the delivered data chunk in system memory.
 * @param size Size of one data item (usually 1 byte).
 * @param nmemb Number of items (total bytes in this specific chunk).
 * @param s Pointer to the target std::string where the final JSON payload is stored.
 * @return size_t The exact number of bytes safely processed by the application.
 */
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* s) {
    size_t newLength = size * nmemb;
    
    try {
        // The program accesses the target string via pointer and appends the new chunk
        s->append((char*)contents, newLength);
        
        // The function returns the exact byte count back to libcurl to confirm successful storage
        return newLength;
        
    } catch(bad_alloc& e) {
        // If the system runs out of RAM, returning 0 triggers a safe libcurl abort
        return 0;
    }
}

int main() {
    cout << "--- WEATHER API SERVER BOOTING UP ---" << endl;
    string api_key = load_api_key_from_env();
    
    if (api_key.empty()) {
        cerr << "[!] CRITICAL: WEATHER_API missing." << endl;
        return 1;
    }

    // 1. Instantiate the web application instance
    crow::SimpleApp app;

    // 2. Create an API endpoint route
    CROW_ROUTE(app, "/api/weather")
    ([api_key](){
        
        // --- TARGET VECTOR CONTAINING PORTUGUESE CITIES ---
        vector<string> cities = {
            "Aveiro",
            "Beja",
            "Braga",
            "Braganca",
            "Castelo Branco",
            "Coimbra",
            "Evora",
            "Faro",
            "Funchal",
            "Guarda",
            "Leiria",
            "Lisbon",
            "Ponta Delgada",
            "Portalegre",
            "Porto",
            "Santarem",
            "Setubal",
            "Viana do Castelo",
            "Vila Real",
            "Viseu"
        };
        
        // Utilize Crow's native json list structure to handle the microservice response payload
        crow::json::wvalue::list response_list;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        CURL* curl = curl_easy_init();

        if(curl) {
            for(const auto& city : cities) {
                char* encoded_city = curl_easy_escape(curl, city.c_str(), city.length());
                string url = "http://api.openweathermap.org/data/2.5/weather?q=" + string(encoded_city) + "&appid=" + api_key + "&units=metric";
                curl_free(encoded_city);
                
                string response_string;
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

                CURLcode res = curl_easy_perform(curl);

                if(res == CURLE_OK) {
                    try {
                        auto data = json::parse(response_string);
                        if (data.contains("cod") && data["cod"] == 200) {
                            
                            // Map the incoming metrics into a JSON object format suitable for web delivery
                            crow::json::wvalue city_data;
                            city_data["city"] = string(data["name"]);
                            city_data["temp"] = double(data["main"]["temp"]);
                            city_data["condition"] = string(data["weather"][0]["main"]);
                            
                            response_list.push_back(move(city_data));
                        }
                    } catch (...) { /* Ignore parse errors to guarantee API server resilience */ }
                }
            }
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();

        // 3. Construct the HTTP response and append CORS headers to grant frontend access
        auto response = crow::response(crow::json::wvalue(response_list));
        response.add_header("Access-Control-Allow-Origin", "*");
        return response;
    });

    cout << "Server running on http://localhost:8080/api/weather" << endl;
    
    // 4. Fire up the application engine as a multithreaded server instance
    app.port(8080).multithreaded().run();
}