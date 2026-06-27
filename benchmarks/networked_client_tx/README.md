In this benchmark, server and benchmark executable (client) ideally should be run from different hosts.
Note that in case of Solarflare TCPDirect, this becomes mandatory as TCPDirect doesn't support loopback.

As for target and local (NIC to bind) IP addresses and local NIC name, update config.cfg for your environment.

**RUNNING THE BENCHMARK**:

1. You need to build and run the server first:

```bash
cd server
make server
./server
```

2. Build the benchmark

```bash
cd ..
make release
```

Note: To enable Solarflare TCPDirect, you will need to enable #define LLFIX_ENABLE_TCPDIRECT line in benchmark.cpp or pass -DLLFIX_ENABLE_TCPDIRECT=ON to CMake. You will also need to configure IP and ports in both benchmark.cpp and server accordingly.

3. Run the benchmark

```bash
./benchmark
```

If you have a Solarflare NIC and if Onload is available :

```bash
onload --profile=../onload_profile.cfg ./benchmark
```
Note : Everytime you repeat the benchmark, press R to reset sequence numbers on the server app.