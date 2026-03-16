#/bin.sh
sudo chmod +x *.py
cd automation
sudo chmod +x deserialiser
make release
cd ..
cd test_server_fix42
sudo chmod +x *.sh
make release
cd ..
cd test_server_fix43
sudo chmod +x *.sh
make release
cd ..
cd test_server_fix44
sudo chmod +x *.sh
make release
cd ..
cd test_server_fix50
sudo chmod +x *.sh
make release
cd ..