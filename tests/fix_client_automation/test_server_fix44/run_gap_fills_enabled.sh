#!/bin/bash
sudo rm -rf store
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../../deps
./test_server ./test_server_gap_fills_enabled.cfg