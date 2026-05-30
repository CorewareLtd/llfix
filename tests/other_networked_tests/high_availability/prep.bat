@echo off
rmdir /s /q "server1"
rmdir /s /q "server2"
rmdir /s /q "clients"
del log*.txt
powershell -command "Expand-Archive -Path 'suite.zip' -DestinationPath '.' -Force"