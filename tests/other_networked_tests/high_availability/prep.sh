#!/bin/bash
rm -rf "server1"
rm -rf "server2"
rm -rf "clients"
rm -rf  log*.txt
unzip -o "suite.zip" -d .
chmod +x ./server1/server1
chmod +x ./server2/server2
chmod +x ./clients/clients
chmod +x ./ha_manager
chmod +x ./ha_syncer