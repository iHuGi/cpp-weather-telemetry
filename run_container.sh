#!/bin/bash

# ==============================================================================
# DEPLOYMENT SCRIPT: weather-service
# ==============================================================================
# Usage: ./run_container.sh [arguments]
# Example: ./run_container.sh --prod
# ==============================================================================

# Define the restart policy. 
# Keep it commented for local development (to see crashes clearly).
# Uncomment for AWS/Production deployment to ensure high availability.

# RESTART_POLICY="--restart unless-stopped"

echo "Starting weather-service..."

# Cleanup: Force remove any existing container with the same name to avoid conflicts
docker rm -f weather-service 2>/dev/null || true

# Start the container in detached mode
# The "$@" passes any arguments (e.g., --prod) directly to the C++ binary
docker run -d \
  --name weather-service \
  -p 8080:8080 \
  -v "$(pwd)/.env:/app/.env" \
  $RESTART_POLICY \
  cpp-weather-api "$@"

# Wait for 3 seconds to allow the C++ service time to validate arguments and initialize
sleep 3

# Health Check: Verify if the container is still running after the startup sequence
if [ "$(docker inspect -f '{{.State.Running}}' weather-service 2>/dev/null)" = "true" ]; then
  echo "SUCCESS: weather-service is running."
else
  # If the container exited (e.g., due to invalid arguments), show logs and exit with an error
  echo "ERROR: weather-service crashed during startup. Logs:"
  docker logs weather-service
  
  # Remove the crashed container to maintain a clean environment
  docker rm -f weather-service
  exit 1
fi