CC = g++
TARGET = server request worker
CFILES = server_stub.h server_stub_clnt.c server_stub_xdr.c server_stub_svc.c
DEBUG = #-g
CFLAGS = -Wall -c $(DEBUG) 
LFLAGS = $(DEBUG)
LIBS =  -lpthread -lssl -lcrypto

all: $(CFILES)

server_stub.h:server_stub.x
	rpcgen -h server_stub.x -o server_stub.h

server_stub_clnt.c:server_stub.x
	rpcgen -l server_stub.x -o server_stub_clnt.c

server_stub_xdr.c:server_stub.x
	rpcgen -c server_stub.x -o server_stub_xdr.c

server_stub_svc.c:server_stub.x
	rpcgen -m server_stub.x -o server_stub_svc.c

all: $(TARGET)

request: lsp_client.o  network.o server_stub_clnt.o server_stub_xdr.o
	$(CC) $(LFLAGS) -o $@ $@.cpp $^ $(LIBS)

worker: lsp_client.o  network.o server_stub_clnt.o server_stub_xdr.o
	$(CC) $(LFLAGS) -o $@ $@.cpp $^ $(LIBS)

server: lsp_server.o network.o server_stub_clnt.o server_stub_xdr.o
	$(CC) $(LFLAGS) -o $@ $@.cpp $^ $(LIBS)

   
%.o: %.cpp
	$(CC) $(CFLAGS) $<

%.o: %.c
	$(CC) $(CFLAGS) $<

clean:
	rm -f *.o 
	rm -f $(TARGET)
	rm -f *~
	rm -f server_stub.h
	rm -f server_stub_clnt.c
	rm -f server_stub_xdr.c
	rm -f server_stub_svc.c
