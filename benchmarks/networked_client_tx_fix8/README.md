In this benchmark, server and benchmark executable (client) ideally should be run from different hosts.

As for target IP address, update client.xml for your environment.

Note that generated Fix8 sources are included in this directory. Therefore f8c step is not required to run this benchmark unless you change FIX50.xml/FIXT11.xml.

**Prerequisites**

You need to build and install fix8 and its dependency Poco.

Installing Poco on RHEL: 

```bash
sudo dnf install -y poco-devel
```

Installing FIX8:
```bash
git clone https://github.com/fix8/fix8.git
cd fix8
sudo chmod +x bootstrap
./bootstrap
CXXFLAGS="-O3 -DNDEBUG" CFLAGS="-O3 -DNDEBUG" ./configure --prefix=/usr/local
make -j"$(nproc)"
sudo make install
```

**RUNNING THE BENCHMARK**:

1. You need to build and run the server first. You can find it in "../networked_client_tx/server" directory :

```bash
cd ../networked_client_tx/server
make release
sudo ./server
```

2. Build and run the benchmark in this directory :

```bash
make release
sudo chmod +x *.sh
sudo ./run.sh
```

If you have a Solarflare NIC and if Onload is available :

```bash
sudo ./run_with_onload.sh
```
Note : Everytime you repeat the benchmark, press R to reset sequence numbers on the server app.