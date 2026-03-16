1. Navigate to tools/high_availability_manager. Build ha manager and copy its executable to this folder.

2. Navigate to tools/high_availability_syncer. Build ha sycner and copy its executable to this folder.

3. On Linux

```bash
sudo chmod +x prep.sh
./prep.sh
```

If on Windows, double click prep.bat

4. Start server1/server1 executable

5. Start server2/server2 executable

6. Start clients1/clients1 executable

7. Start clients2/clients2 executable

8. Start ha syncer for server1 :

```bash
./ha_syncer ./ha_syncer_primary.cfg
```

9. Start ha syncer for server2 :

```bash
./ha_syncer ./ha_syncer_secondary.cfg
```

10. Start ha manager :

```bash
./ha_manager ./ha_manager.cfg
```