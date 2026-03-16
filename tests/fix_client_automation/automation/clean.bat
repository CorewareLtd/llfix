@echo off

rmdir /s /q "build"
rmdir /s /q "messages_incoming"
rmdir /s /q "messages_outgoing"
rmdir /s /q "x64"
rmdir /s /q ".vs"
del *.store
del *log.txt
del *outgoing.txt
del *incoming.txt
del *new_orders.txt
del *cancel_orders.txt
del *replace_orders.txt
del *outgoing_resend_requests.txt
