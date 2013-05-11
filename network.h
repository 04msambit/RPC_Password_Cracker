// John Keech, UIN:819000713, CSCE 438 - HW2

#ifndef NETWORK_HANDLER
#define NETWORK_HANDLER

#include "datatypes.h"
#include <rpc/rpc.h>
#include<string>
#include<queue>


#define _DROP_RATE 0.0

// build a socket and try to connect
Connection* network_setup_server(int port);
Connection* network_make_connection(const char *host, int port);
bool network_send_connection_request(Connection *conn);
bool network_wait_for_connection(Connection *conn, double timeout); // wait for a connection for timeout seconds

// send and receive messages
bool network_send_message(Connection *conn, message *msg);
message* network_read_message(Connection *conn, double timeout, sockaddr_in *addr); // try to read a message with a timeout in seconds
//bool network_acknowledge(Connection *conn); // send an acknowledgement from the client for the previously received message
bool network_acknowledge_client(Connection*, CLIENT*);
bool network_acknowledge_server(Connection *conn);
// Marshal/unmarshal data using Google Protocol Buffers
message* network_build_message(int is, int seq, uint8_t *pld, int len);
uint8_t* network_marshal(message *msg, int *packedSize);
message* network_unmarshal(uint8_t *buf, int buf_len);

// configure the network drop rate
void network_set_drop_rate(double percent);

// use the drop rate to determine if we should send/receive this packet
bool network_should_drop();

// create a timeval structure
struct timeval network_get_timeval(double seconds);

//std::map<unsigned int,Connection*> global_clients;
//pthread_mutex_t map_lock;

void populate_global_clients(int,Connection*);
void empty_global_clients(int);

/*struct message
{
  uint32_t connid;
  uint32_t seqnum;
  std::string  payload;};*/




#endif
