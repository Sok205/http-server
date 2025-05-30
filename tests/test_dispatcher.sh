#!/bin/zsh

echo "Starting the dispatcher test: "

for i in {1..10}; do
    curl -s http://localhost:4222/hello &
done
wait
echo "Test complteted"