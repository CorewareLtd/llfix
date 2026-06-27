#!/bin/bash
rm -rf logs stores global_filename*
mkdir -p logs
mkdir -p stores
LD_LIBRARY_PATH=/usr/local/lib/ onload --profile=../onload_profile.cfg ./benchmark