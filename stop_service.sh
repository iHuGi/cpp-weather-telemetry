#!/bin/bash

# Stop and remove the weather-service container cleanly
echo "Stopping weather-service..."
docker rm -f weather-service 2>/dev/null

echo "Service stopped and container removed."