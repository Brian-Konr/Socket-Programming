all: server client

server: server.cpp
	g++ -o server server.cpp -lssl -lcrypto -pthread

client: client.cpp
	g++ -o client client.cpp -lssl -lcrypto -pthread