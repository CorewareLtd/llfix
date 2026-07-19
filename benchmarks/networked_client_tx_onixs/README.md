In this benchmark, server and benchmark executable (client) ideally should be run from different hosts.

As for target IP address, update benchmark.cpp for your environment.

**Prerequisites**

You need to obtain OnixS FIX engine. And then you need to copy the following directories to this folder:  'include' and 'lib' and 'license'


**RUNNING THE BENCHMARK**:

1. You need to build and run the server first. You can find it in "../networked_client_tx/server" directory :

```bash
cd ../networked_client_tx/server
make release
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