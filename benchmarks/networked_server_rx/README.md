**PREREQUISITE**:

Throughput calculation is done by using llfix deserialiser. Therefore you need to have that executable in that directory.
Existing executable is built on RHEL. If it doesn't work due to C++ runtime mismatch, you need to build it again and copy the executable to this folder.

To build it navigate to tools directory and do "make release".

**RUNNING THE BENCHMARK**:

1. Give the required permissions:

```bash
sudo chmod +x *.sh *.py deserialiser
```

2. Build and run the server

```bash
make release
sudo ./server
```

Note1: To enable FIX dictionary validations, enable #define LLFIX_ENABLE_DICTIONARY line in server.cpp or pass -DLLFIX_ENABLE_DICTIONARY=ON to CMake.

Note2: To enable multithreaded FIX server, enable #define ENABLE_SCALABLE_SERVER line in server.cpp or pass -DLLFIX_ENABLE_SCALABLE_SERVER=ON to CMake.

3. Navigate to clients directory and build the client app

```bash
cd clients
sudo chmod +x *.sh
make release
```

4. Run the clients app

```bash
sudo ./clients
```

5. When in the clients app, send orders. Example: Since there are 8 clients, if you send 150K orders that will be 1 million 35=D messages in total.


6. After all orders sent, you can quit from the server and run the py script to calculate throughput:

```bash
sudo python ./calculate_throughput.py
```

Note: When you want to repeat the test, you can invoke clean.sh in both server directory and clients directory.