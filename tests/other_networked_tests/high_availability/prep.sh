#!/bin/bash
rm -rf "server1"
rm -rf "server2"
rm -rf "clients1"
rm -rf "clients2"
unzip -o "suite.zip" -d .
chmod +x ./server1/server1
chmod +x ./server2/server2
chmod +x ./clients1/clients1
chmod +x ./clients2/clients2