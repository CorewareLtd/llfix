# Order router example

This example demonstrates how to build a simple order router using llfix.

In this implementation, the order router class is based on llfix::FixServer, which also spawns separate threads to manage llfix::FixClient instances for outbound sessions.

The scripts folder contains tinyfix scripts that you can use to try this example. The message flow is as follows:


```text
CLIENT1 ------|                       ----> SERVER1
              |-->  ORDER ROUTER  ---
CLIENT2 ------|                       ----> SERVER2
```


STEPS TO RUN

1. As a prerequisite, you need to install tinyfix library : https://github.com/CorewareLtd/tinyfix

2. Start server1.py and server2.py

3. Build and start the order router

4. Finally start client1.py and client2.py

Both clients will send 10 new orders. Client1’s messages will be routed to Server1, while Client2’s messages will be routed to Server2.