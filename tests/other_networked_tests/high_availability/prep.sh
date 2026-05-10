#!/bin/bash
rm -rf "server1"
rm -rf "server2"
rm -rf "clients"
unzip -o "suite.zip" -d .
chmod +x ./server1/server1
chmod +x ./server2/server2
chmod +x ./clients/clients