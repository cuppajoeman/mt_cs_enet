in order to use this clone the repo then run
```
git submodule init
git submodule update
```

then to run the server you can do 
```
cd server
cmake -S . -B build
cmake --build build
cd build
./multithreaded_enet_server
```

then to run the client you can do
```
cd client
cmake -S . -B build
cmake --build build
cd build
./multithreaded_enet_client
```
you can run more than one client at once.
