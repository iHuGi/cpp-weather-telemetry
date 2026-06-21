#!/bin/bash

# Stop and remove the existing container without errors if it's already stopped
docker rm -f weather-service 2>/dev/null

# Start the new version of the container
echo "Starting weather-service..."
docker run -d -p 8080:8080 -v $(pwd)/.env:/app/.env --name weather-service cpp-weather-api

echo "weather-service started with success!"