#!/bin/bash

# Number of clients to simulate
NUM_CLIENTS=10

# Server address and port
SERVER_ADDR="localhost"
SERVER_PORT=12345

#!/bin/bash

files=("a_chars.txt" "b_chars.txt" "c_chars.txt" "d_chars.txt" "e_chars.txt" "f_chars.txt" "g_chars.txt" "h_chars.txt" "i_chars.txt" "j_chars.txt")

for i in "${!files[@]}"; do
  echo "Starting client $((i+1)) using file ${files[$i]}"
  nc localhost 12345 < "${files[$i]}" &
  sleep 0.5  # Add a delay of 0.5 seconds between clients
done

wait  # Wait for all background clients to finish
echo "All clients finished"
