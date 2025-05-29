#!/bin/bash

echo "First request (should pass):"
curl -i http://localhost:4222/hello
echo -e "\n"

echo "Immediate second request (should get 429):"
curl -i http://localhost:4222/hello
echo -e "\n"

echo "Sleeping 150ms..."
sleep 0.15

echo "Third request (should pass):"
curl -i http://localhost:4222/hello
echo -e "\n"