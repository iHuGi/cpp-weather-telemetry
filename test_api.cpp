#define CATCH_CONFIG_MAIN
#define UNIT_TEST // Flag to ignore main in classes file
#include "catch.hpp"
#include "telemetry_engine_classes.cpp" // Imports classes to be used

TEST_CASE("Validate JSON payload and API call counter", "[cache]") {
    WeatherCache cache;

    // Mock data simulating OpenWeather response
    crow::json::wvalue::list mock_data;
    crow::json::wvalue mock_city;
    mock_city["city"] = "Lisbon";
    mock_city["temp"] = 25.5;
    mock_data.push_back(std::move(mock_city));

    SECTION("Ensure JSON retains city data and increments API calls") {
        // Inject mock data into the cache (simulating 1 call)
        cache.updateCache(std::move(mock_data), 1);
        
        // Retrieves the pre-serialized cache snapshot as a string payload
        std::string snapshot = cache.getCachedJson();

        // Parses the snapshot string back into a JSON object to validate structure and contents for unit testing
        auto parsed_json = json::parse(snapshot);

        // VALIDATIONS (The test only passes if these are perfectly true)
        REQUIRE(parsed_json["calls"].get<int>() == 1);
        REQUIRE(parsed_json["data"][0]["city"].get<std::string>() == "Lisbon");
        REQUIRE(parsed_json["data"][0]["temp"].get<double>() == 25.5);
    }
}