@echo off

rmdir /s /q "build"
rmdir /s /q "clients"
rmdir /s /q "x64"
rmdir /s /q ".vs"
del *log.txt
del *messages.txt
del incoming.txt
del outgoing.txt
del *vcxproj.user
