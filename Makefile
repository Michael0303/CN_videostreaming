CC = gcc
CXX = g++
INCLUDE_OPENCV = `pkg-config --cflags --libs opencv`
LINK_PTHREAD = -lpthread

CLIENT = client.cpp
SERVER = server.cpp
CLI = client
SER = server


all: server client
  
server: $(SERVER)
	$(CXX) $(SERVER) -o $(SER) $(LINK_PTHREAD) $(INCLUDE_OPENCV)
client: $(CLIENT)
	$(CXX) $(CLIENT) -o $(CLI) $(LINK_PTHREAD) $(INCLUDE_OPENCV)


.PHONY: clean

clean:
	rm $(CLI) $(SER) 
