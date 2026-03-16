@echo off

rmdir /s /q "build"
rmdir /s /q "clients"
rmdir /s /q "x64"
rmdir /s /q ".vs"
del *.store
del *.user
del *.obj
del *.exp
del *.lib