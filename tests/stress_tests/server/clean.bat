@echo off

rmdir /s /q "build"
rmdir /s /q "clients"
rmdir /s /q "x64"
rmdir /s /q ".vs"
del *log.txt
del *.ilk
del *.obj
del *.pdb
del *.user
del *.exp
del *.obj
del *.lib