#!/bin/bash
sudo rm -rf logs stores global_filename*
mkdir -p logs
mkdir -p stores
sudo -E LD_LIBRARY_PATH=/usr/local/lib/ ./benchmark