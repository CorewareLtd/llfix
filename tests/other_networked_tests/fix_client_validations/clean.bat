@echo off

rmdir /s /q "build"
rmdir /s /q "messages_incoming"
rmdir /s /q "messages_outgoing"
rmdir /s /q "x64"
rmdir /s /q ".vs"
rmdir /s /q "__pycache__"
del *log.txt
del *.store
del *.user
