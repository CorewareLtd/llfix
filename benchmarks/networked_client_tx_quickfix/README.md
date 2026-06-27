In this benchmark, server and benchmark executable (client) ideally should be run from different hosts.

As for target and local (NIC to bind) IP addresses and local NIC name, update client.cfg for your environment.

**Prerequisites**

The build in this directory expects the required include directories and dynamic library files to be located under ../tests/deps.

To prepare this folder, please refer to the README.md file in that directory.

**RUNNING THE BENCHMARK**:

1. You need to build and run the server first. You can find it in "../networked_client_tx/server" directory :

```bash
cd ../networked_client_tx/server
make server
./server
```

2. Build and run the benchmark in this directory :

```bash
make release
sudo chmod +x *.sh
./run.sh
```

If you have a Solarflare NIC and if Onload is available :

```bash
./run_with_onload.sh
```
Note : Everytime you repeat the benchmark, press R to reset sequence numbers on the server app.