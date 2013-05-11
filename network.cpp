// John Keech, UIN:819000713, CSCE 438 - HW2

#include "network.h"
#include "server_stub.h"
#include  <queue>
#include  <map>
#include <list>
#include "lsp.h"
#include "server_stub_svc.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>



#define RPC_SLEEP _EPOCH_LTH/3

double drop_rate = _DROP_RATE;
static int conid=-1;

struct rpc_message
{
    struct message* message;
    struct sockaddr_in addr_in;   
};

std::queue<struct rpc_message> global_queue;

pthread_mutex_t input_lock;
pthread_mutex_t output_lock;


struct global_output_list
{
    struct message* message;
   
    struct global_output_list* next;
};

struct global_output_list* HEAD=NULL;

std::map<unsigned int,Connection*> global_clients;
pthread_mutex_t map_lock;




void populate_global_clients(int id,Connection* conn)
{
  
    pthread_mutex_lock(&(map_lock));
    global_clients.insert(std::pair<int,Connection*>(conn->id,conn));
    pthread_mutex_unlock(&(map_lock));

   
}


void empty_global_clients(int id_value)
{
    pthread_mutex_lock(&(map_lock));
    global_clients.erase(id_value);
    pthread_mutex_unlock(&(map_lock));

}



static void
rpc_prog_1(struct svc_req *rqstp, register SVCXPRT *transp)
{
	union {
		message server_recv_mssg_1_arg;
		uint32_t server_send_mssg_1_arg;
	} argument;
	char *result;
	xdrproc_t _xdr_argument, _xdr_result;
	char *(*local)(char *, struct svc_req *);

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply (transp, (xdrproc_t) xdr_void, (char *)NULL);
		return;

	case SERVER_RECV_MSSG:
		_xdr_argument = (xdrproc_t) xdr_message;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) server_recv_mssg_1_svc;
		break;

	case SERVER_SEND_MSSG:
		_xdr_argument = (xdrproc_t) xdr_uint32_t;
		_xdr_result = (xdrproc_t) xdr_message;
		local = (char *(*)(char *, struct svc_req *)) server_send_mssg_1_svc;
		break;

	default:
		svcerr_noproc (transp);
		return;
	}
	memset ((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
		svcerr_decode (transp);
		return;
	}
	result = (*local)((char *)&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, (xdrproc_t) _xdr_result, result)) {
		svcerr_systemerr (transp);
	}
	if (!svc_freeargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
		fprintf (stderr, "%s", "unable to free arguments");
		exit (1);
	}
	return;
}



#define PACKET_SIZE 2048

void* register_service(void*)
{
	register SVCXPRT *transp;

	pmap_unset (RPC_PROG, SERVER_VERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		fprintf (stderr, "%s", "cannot create udp service.");
		exit(1);
	}
        else
        {
          printf("\nUDP service registered\n");
        }
	if (!svc_register(transp, RPC_PROG, SERVER_VERS, rpc_prog_1, IPPROTO_UDP)) {
		fprintf (stderr, "%s", "unable to register (RPC_PROG, SERVER_VERS, udp).");
		exit(1);
	}
        else
        {
               printf("\nService Registered\n");
        }    

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		fprintf (stderr, "%s", "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, RPC_PROG, SERVER_VERS, rpc_prog_1, IPPROTO_TCP)) {
		fprintf (stderr, "%s", "unable to register (RPC_PROG, SERVER_VERS, tcp).");
		exit(1);
	}

	svc_run ();
	fprintf (stderr, "%s", "svc_run returned");
	exit (1);
	/* NOTREACHED */
}






Connection* network_setup_server(int port){
    Connection *conn = new Connection();
    
   
    
    // setup local address to bind on
    conn->addr = new sockaddr_in();
    memset(conn->addr,0,sizeof(sockaddr_in));
    conn->addr->sin_family = AF_INET;
    conn->addr->sin_port = htons(port);
    conn->addr->sin_addr.s_addr = htonl(INADDR_ANY);
    
   
    
    return conn;
}

Connection* network_make_connection(const char *host, int port){
    // creates a socket to the specified host and port
    
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if(sd < 0) 
        {
            printf("Error creating socket: %d\n",sd);
            return NULL;
        }
    
    // fill in the sockaddr_in structure
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    char server[256];
    
    if(host)
        strcpy(server,host);
    else
        strcpy(server,"127.0.0.1");
        
    memset(&addr,0,addrlen);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(server);
    
    // not an IP, so try to look it up as a hostname
    if(addr.sin_addr.s_addr == (unsigned long)INADDR_NONE){
        struct hostent *hostp = gethostbyname(server);
        if(!hostp){
            printf("could not find host %s\n",server);
            return NULL;
        }
        memcpy(&addr.sin_addr, hostp->h_addr, sizeof(addr.sin_addr));
    }
    
   
    Connection *c = new Connection();
    //c->fd = sd;
    //c->addr = new sockaddr_in();
    //memcpy(c->addr,&addr,addrlen);
    return c;   
}

// send a connection request
bool network_send_connection_request(Connection *conn){
    message *msg = network_build_message(0,0,NULL,0);    
    if(network_send_message(conn,msg)) {
        conn->status = CONNECT_SENT;
        return true;
    } else {
        return false;
    }
}

// look for an acknowledgement of a connection request, and retrieve the
// connection ID from the response
bool network_wait_for_connection(Connection *conn, double timeout){
    sockaddr_in addr;
    message *msg = network_read_message(conn, timeout,&addr);
    if (msg && msg->seqnum == 0){
        conn->id = msg->connid;
        if(DEBUG) printf("[%d] Connected\n",conn->id); 
        return true;
    } else {
        if(DEBUG) printf("Timed out waiting for connection from server after %.2f seconds\n",timeout);
        return false;
    }
    return true;
}

// acknowledge the last received message
/*bool network_acknowledge(Connection *conn){
  message *msg = network_build_message(conn->id,conn->lastReceivedSeq,NULL,0);
  return network_send_message(conn,msg);
  }*/
bool network_acknowledge_server(Connection *conn){
    
    //printf("\n Acknowledgement from server for connection id %d\n",conn->id);
     
    message *msg = network_build_message(conn->id,conn->lastReceivedSeq,NULL,0);
    return network_send_message(conn,msg);
}

bool network_acknowledge_client(Connection *conn,CLIENT *client)
{
    message *msg = network_build_message(conn->id,conn->lastReceivedSeq,NULL,0);
    int* ret=  server_recv_mssg_1(msg,client);
    return true;   
}



bool network_send_message(Connection *conn, message *msg)
{
   
    
    //printf("In network_send_message waiting for output lock\n");
    pthread_mutex_lock(&(output_lock));
    //printf("In network_send_message acquired output lock\n");

    struct global_output_list* node = (global_output_list*)malloc(sizeof(struct global_output_list));

    node->message = (message*)malloc(sizeof(struct message));

    node->message->connid = msg->connid;
    node->message->seqnum = msg->seqnum;    
    node->message->payload_length = msg->payload_length;

    node->message->payload = (char*)malloc(msg->payload_length+1);
    memset(node->message->payload,0,msg->payload_length+1);
    memcpy(node->message->payload,msg->payload,msg->payload_length);

    
        
    

    node->next = NULL;

   

    if(HEAD == NULL)
        {
            HEAD = node;
        }
    else
        {
            struct global_output_list* iterator;
            iterator = HEAD;
            while(iterator->next)
                {
                    iterator = iterator->next;
                }
     
            iterator->next = node;

        }
  
    pthread_mutex_unlock(&(output_lock));
    //printf("In network_send_message released output lock\n"); 



    //global_output_queue.push(msg);

    return true;
}

// RPC function

int* server_recv_mssg_1_svc(message* data_message,struct svc_req * svc)
{
    // We will populate the global queue
    //printf ("\n Inside server receive message\n");
    //printf ("\n The connection id is %d and sequence number is %d and packet is %s\n",data_message->connid,data_message->seqnum,data_message->payload);
    //Making new message and copying the message received from RPC call

    struct rpc_message message_rpc;
    message_rpc.message = (message*)malloc(sizeof(struct message));


    //message_rpc.message *packet = (message*)malloc(sizeof (struct message)); 
    message_rpc.message->connid = data_message->connid;
    message_rpc.message->seqnum = data_message->seqnum;
    message_rpc.message->payload_length = data_message->payload_length;
    message_rpc.message->payload = (char*)malloc(data_message->payload_length+1);
    memset(message_rpc.message->payload,0,data_message->payload_length+1);
    memcpy(message_rpc.message->payload,data_message->payload,data_message->payload_length);

   
    message_rpc.addr_in = *(svc_getcaller(svc->rq_xprt));
     

    pthread_mutex_lock(&(input_lock));
      
    global_queue.push(message_rpc);
    pthread_mutex_unlock(&(input_lock));
    
    // We will monitor the output queue now to have the message
   

    struct global_output_list* iterator=HEAD;
    struct global_output_list * tempit;
    tempit = NULL;
    bool flag = data_message->connid==0;
    //bool flag = true;
    conid = data_message->connid; 
    //   conid = 4;
    //conid = -1;
    while(flag)
        {
            //printf("In server_recv_mssg waiting for output lock in while loop\n"); 
            pthread_mutex_lock(&(output_lock));
            //printf("In server_recv_mssg acquired output lock in while loop\n"); 
    
            iterator = HEAD;
            tempit  = NULL;
            while(iterator)
                {
                   
                    //if(iterator->message->seqnum==0)
                    //printf("\n The global_client size is %d\n",global_clients.size());
                    pthread_mutex_lock(&(map_lock));
                    for(std::map<unsigned int,Connection*>::iterator it=global_clients.begin();
                        it != global_clients.end();++it)
                        {
                            Connection *connection = it->second;
                            
                            //printf("%d %d\n",message_rpc.addr_in.sin_port,connection->port);
                            if(message_rpc.addr_in.sin_port == connection->port)
                                {
                                    conid = (it->first);
                                }
                        }
                    pthread_mutex_unlock(&(map_lock));
                       
                    //printf("\nThe conid got from connections is %d\n",conid);

                    //if(iterator->message->seqnum == data_message->seqnum)
                    if(iterator->message->seqnum == 0 && iterator->message->connid == conid)
                                                                 
                        {      
                            //conid = iterator->message->connid;
                            if(tempit){
                                tempit->next = iterator->next;
                            }		
                            else{
                                HEAD = iterator->next;
                            } 
                            free(iterator);
                            flag = false;
                            //  pthread_mutex_unlock(&(output_lock));
                            // printf("In server_recv_mssg released output lock in while loop\n"); 
                           
                            break;
                        } 
                       
	 
                    tempit=iterator;
                    iterator=iterator->next;
                }	
            pthread_mutex_unlock(&(output_lock));
            //printf("In server_recv_mssg released output lock\n"); 
           
            //sleep(2);
            //sleep(RPC_SLEEP);
           

        }

    //static bool_t  val = true;
    //return &val;
	
    //printf("\nWe are sending back connenction id --- %d\n",conid);
     
    return &conid;
}

message * server_send_mssg_1_svc(uint32_t *conid,struct svc_req*)
{
    // We will pop a message from the global output queue and display it 
    //printf("\n We are inside server_send_message\n");   

   
    //printf("In server_send_mssg_1 waiting for output_lock by thread %ld\n",(long int)syscall(224));
    pthread_mutex_lock(&(output_lock));
    //printf("In server_send_mssg_1 got output_lock by thread %ld\n",(long int)syscall(224));

    struct global_output_list* iterator;
    struct global_output_list* temp;
   
    if(HEAD)
        iterator = HEAD;
    // pthread_mutex_unlock(&(output_lock)); 
    else
        {
            pthread_mutex_unlock(&(output_lock));
            //printf("In server_send_mssg_1 released  output_lock by thread %ld\n",(long int)syscall(224));
            return NULL;
        
        
        }
   
    temp = NULL;
    bool flag = true;
    static message msg = {0,0,NULL,0};
    if(!msg.payload){
        msg.payload = (char *) malloc(1500);
    }
    memset(msg.payload,0,1500);
   

    //printf("\nThe connection id received from client is  %d\n",*conid);

    while(iterator && flag)
        {
            if(iterator->message->connid == *conid)
                {
                    if(iterator->message->seqnum !=0){
                        msg.connid = *conid;
                        msg.seqnum = iterator->message->seqnum;
                        msg.payload_length = iterator->message->payload_length;       		  
                        memcpy(msg.payload,iterator->message->payload,msg.payload_length);
                        flag = false;
                        if(temp)
                            temp->next = iterator->next;
                        else
                            HEAD = iterator->next;
                        free(iterator); 
                        break; 		
                    }
                }
            temp = iterator;
            iterator = iterator->next;
      
        }
  
   

    //message* msg = global_output_queue.front();
  
    //global_output_queue.pop();
            
    pthread_mutex_unlock(&(output_lock));
    //printf("In server_send_mssg_1 released  output_lock by thread %ld\n",(long int)syscall(224));

    return &msg;
}


message* network_read_message(Connection *conn, double timeout, sockaddr_in *addr){
   
    
    timeval t = network_get_timeval(timeout);
    while(true){
       
        
        if(network_should_drop())
            continue; // drop the packet and continue reading
        
        // Keep monitoring the global queue

        bool flag=true;
        rpc_message msg;
            
        while(flag)
            {
                //printf("In network_read_message waiting for input_lock by thread %ld\n",(long int)syscall(224));

                pthread_mutex_lock(&(input_lock));
                //printf("In network_read_message got input_lock by thread %ld\n",(long int)syscall(224));

                if(global_queue.size() > 0)
                    {
                        //printf("\nThe size of input queue is %d\n",global_queue.size());
                        msg = global_queue.front();
                        global_queue.pop();
                        //printf("\n msg->connid %d msg->seqnum %d  msg->payload %s\n",msg.message->connid,msg.message->seqnum,msg.message->payload);
                        flag = false;         
                    }
                pthread_mutex_unlock(&(input_lock));
                //printf("In network_read_message released input_lock by thread %ld\n",(long int)syscall(224));

            }

        *addr = msg.addr_in;
            
            

            
        return msg.message;
    }
   
}

message* network_build_message(int id, int seq, uint8_t *pld, int len)
{
    // create the LSPMessage data structure and fill it in
    
    message *msg = new message();
    
   

    msg->connid = id;
    msg->seqnum = seq;
    
    msg->payload = (char*)malloc(len+1);
    memset(msg->payload,0,len+1);
    memcpy(msg->payload,(char*)pld,len);
    msg->payload_length = len;    
    return msg;
}



// configure the network drop rate
void network_set_drop_rate(double percent){
    if(percent >= 0 && percent <= 1)
        drop_rate = percent;
}

// use the drop rate to determine if we should send/receive this packet
bool network_should_drop(){
    return (rand() / (double)RAND_MAX) < drop_rate;
}

struct timeval network_get_timeval(double seconds)
{
    // create the timeval structure needed for the timeout in the select call for reading from a socket
    timeval t;
    t.tv_sec = (long)seconds;
    t.tv_usec = (seconds - (long)seconds) * 1000000; // remainder in s to us
    return t;
}

