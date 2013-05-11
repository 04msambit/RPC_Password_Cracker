#include "lsp_client.h"
#include<stdlib.h>
double epoch_delay = _EPOCH_LTH; // number of seconds between epochs
unsigned int num_epochs = _EPOCH_CNT; // number of epochs that are allowed to pass before a connection is terminated

/*
 *
 *
 *				LSP RELATED FUNCTIONS
 *
 *
 */  
 
// Set length of epoch (in seconds)
void lsp_set_epoch_lth(double lth){
    if(lth > 0)
        epoch_delay = lth;
}

// Set number of epochs before timing out
void lsp_set_epoch_cnt(int cnt){
    if(cnt > 0)
        num_epochs = cnt;
}

// Set fraction of packets that get dropped along each connection
void lsp_set_drop_rate(double rate){
    network_set_drop_rate(rate);
}


/*
 *
 *
 *				CLIENT RELATED FUNCTIONS
 *
 *
 */  

lsp_client* lsp_client_create(const char* dest, int port){
    
    lsp_client *client = new lsp_client();
    pthread_mutex_init(&(client->mutex),NULL);
   
    client->connection = new Connection();
    client->connection->lastSentSeq = 0;
    client->connection->lastReceivedSeq = 0;
    client->connection->lastReceivedAck = 0;
    client->connection->epochsSinceLastMessage = 0;
    client->clnt = clnt_create(dest,RPC_PROG,SERVER_VERS,"udp");
    // if(client->clnt==NULL)
    // printf("RPC failed!!!!");


    
    // kickoff new epoch timer
    int res;
    //if(network_send_connection_request(client->connection) && network_wait_for_connection(client->connection, epoch_delay * num_epochs))
 
    message *mssg = network_build_message(0,0,NULL,0);
 
    //printf("\nThe connection create message formed is connid is  %d seqnum is %d  payload is %s\n",mssg->connid,mssg->seqnum,mssg->payload);
    
    //int *val;
    bool_t *val;
    val = server_recv_mssg_1(mssg,client->clnt);
    if(val==NULL)
        clnt_perror(client->clnt,"Client Call failed in client create");
  
     
    if(val)
        {
            if(*val!=-1){
                client->connection->id = *val;
                client->connection->status = CONNECTED;
                // printf("The connection id returned is %d\n",*val);
               
            }
        }
    




    if((res = pthread_create(&(client->epochThread), NULL, ClientEpochThread, (void*)client)) != 0){
        printf("Error: Failed to start the epoch thread: %d\n",res);
        lsp_client_close(client);
        return NULL;
    }

    

   

    pthread_mutex_lock(&(client->mutex));
        
    // connection succeeded, build lsp_client struct        
    //  client->connection->port = port;
    //  client->connection->host = dest;
        
    //client->connection->status = CONNECTED;
        
    // kick off ReadThread to catch incoming messages
    //int res;
    if((res = pthread_create(&(client->readThread), NULL, ClientReadThread, (void*)client)) != 0){
        printf("Error: Failed to start the read thread: %d\n",res);
        lsp_client_close(client);
        return NULL;
    }
    if((res = pthread_create(&(client->writeThread), NULL, ClientWriteThread, (void*)client)) != 0){
        printf("Error: Failed to start the write thread: %d\n",res);
        lsp_client_close(client);
        return NULL;
    }
        
    pthread_mutex_unlock(&(client->mutex));
    return client;
    
    
}

int lsp_client_read(lsp_client* a_client, uint8_t* pld){
    // block until a message arrives or the client becomes disconnected
    while(true)
        {
            pthread_mutex_lock(&(a_client->mutex));
            Status s = a_client->connection->status;
            message *msg = NULL;
            if(s == CONNECTED) 
                {
                    // try to pop a message off of the inbox queue
                    if(a_client->inbox.size() > 0){
                        msg = a_client->inbox.front();
                        a_client->inbox.pop();
                    }
                }
            pthread_mutex_unlock(&(a_client->mutex));
            if(s == DISCONNECTED)
                break;
           
            // we got a message, so return it
            if(msg)
                {
                    std::string payload = msg->payload;
                    delete msg;
                    memcpy(pld,payload.c_str(),payload.length()+1);
                    //printf("\nThe payload is %d\n",payload.length());
                    return payload.length();
                }
        
            // still connected, but no message has arrived...
            // sleep for a bit
            usleep(10000); // 10 ms = 10,0000 microseconds
        }
    if(DEBUG) printf("Client was disconnected. Read returning NULL\n");
    return 0; // NULL, no bytes read (client disconnected)
}

bool lsp_client_write(lsp_client* a_client, uint8_t* pld, int lth){
    // queues up a message to be written by the Write Thread
    
    if(pld == NULL || lth == 0)
        return false; // don't send bad messages
    
    pthread_mutex_lock(&(a_client->mutex));
    a_client->connection->lastSentSeq++;
    if(DEBUG) printf("Client queueing msg %d for write\n",a_client->connection->lastSentSeq);
    
    // build the message
    //LSPMessage *msg = network_build_message(a_client->connection->id,a_client->connection->lastSentSeq,pld,lth);
    
    message *msg = network_build_message(a_client->connection->id,a_client->connection->lastSentSeq,pld,lth);

    // queue it up
    a_client->connection->outbox.push(msg);
    pthread_mutex_unlock(&(a_client->mutex));
    
    return true;
}

bool lsp_client_close(lsp_client* a_client){
    // returns true if the connected was closed,
    // false if it was already previously closed
    
    if(DEBUG) printf("Shutting down the client\n");
    
    pthread_mutex_lock(&(a_client->mutex));
    bool alreadyClosed = (a_client->connection && a_client->connection->status == DISCONNECTED);
    if(a_client->connection)
        a_client->connection->status = DISCONNECTED;
    pthread_mutex_unlock(&(a_client->mutex));
    
    cleanup_client(a_client);
    return !alreadyClosed;
}

/* Internal Methods */

void* ClientEpochThread(void *params){
    lsp_client *client = (lsp_client*)params;
    
    while(true){
        usleep(epoch_delay * 1000000); // convert seconds to microseconds
        if(DEBUG) printf("Client epoch handler waking up \n");
        
        // epoch is happening; send required messages
        pthread_mutex_lock(&(client->mutex));
        if(client->connection->status == DISCONNECTED)
            break;
        
        if(client->connection->status == CONNECT_SENT)
            {
                // connect sent already, but not yet acknowledged
                //if(DEBUG) printf("Client resending connection request\n");
                //network_send_connection_request(client->connection);
            
                message *mssg = network_build_message(0,0,NULL,0);

                //printf("\nThe connection create message formed is connid is  %d seqnum is %d  payload is %s\n",mssg->connid,mssg->seqnum,mssg->payload);
    
                int *vall;
                vall = server_recv_mssg_1(mssg,client->clnt);
                if(vall==NULL)
                    clnt_perror(client->clnt,"Client Call failed");
                // printf("RPC failed\n");  

                if(vall)
                    {
                        if(*vall!=-1){
                            client->connection->id = *vall;
                            client->connection->status = CONNECTED;
                            // printf("The connection id returned is %d\n",*vall);
                            //num_epochs = 5;
                        }
                    }
               
        


            } else if(client->connection->status == CONNECTED)
            {
                // send ACK for most recent message
                //if(DEBUG) printf("Client acknowledging last received message: %d\n",client->connection->lastReceivedSeq);
                //printf("\nWe will Acknowledge the Packet\n");
                network_acknowledge_client(client->connection,client->clnt);
            
                // resend the first message in the outbox, if any
                if(client->connection->outbox.size() > 0) 
                    {
                        if(DEBUG) printf("Client resending msg %d\n",client->connection->outbox.front()->seqnum);
                        network_send_message(client->connection,client->connection->outbox.front());
                    }
            } else {
            if(DEBUG) printf("Unexpected client status: %d\n",client->connection->status);
        }
        
        if(++(client->connection->epochsSinceLastMessage) >= num_epochs){
            // oops, we haven't heard from the server in a while;
            // mark the connection as disconnected
            if(DEBUG) printf("Too many epochs have passed since we heard from the server... disconnecting\n");
            client->connection->status = DISCONNECTED;
            break;
        }
        pthread_mutex_unlock(&(client->mutex));
    }
    pthread_mutex_unlock(&(client->mutex));
    if(DEBUG) printf("Epoch Thread exiting\n");
    return NULL;
}

void* ClientReadThread(void *params){
    lsp_client *client = (lsp_client*)params;
    
    // continuously poll for new messages and process them;
    // Exit when the client is disconnected
    while(true){
        pthread_mutex_lock(&(client->mutex));
        Status state = client->connection->status;
        pthread_mutex_unlock(&(client->mutex));
        
        if(state == DISCONNECTED)
            break;
        
        // attempt to read
        sockaddr_in addr;
        //   message *msg = network_read_message(client->connection, 0.5,&addr);
       	message *recv_msg =NULL;
        //printf("The connection id of client is %d\n", client->connection->id);
        recv_msg= server_send_mssg_1(&client->connection->id,client->clnt);
        if(recv_msg==NULL)
            {	 
                //clnt_perror(client->clnt,"Client Call failed in read thread");
                usleep(40);
                //return NULL;
            }
        message *msg = NULL;

        //printf("\nReceiving message at client side\n");
        
        if(recv_msg)
            {
                msg = (message*)malloc(sizeof(struct message));
                msg->connid =  recv_msg->connid;
                msg->seqnum = recv_msg->seqnum;
                
                msg->payload = (char *)malloc(recv_msg->payload_length+1);
               
                memset(msg->payload,0,recv_msg->payload_length+1);
                msg->payload_length = recv_msg->payload_length;
                memcpy(msg->payload,recv_msg->payload,msg->payload_length);
               
            }

             
        if(msg) 
            {
            
                if(msg->connid == client->connection->id)
                    {
                
                        pthread_mutex_lock(&(client->mutex));
                
                        // reset counter for epochs since we have received a message
                        client->connection->epochsSinceLastMessage = 0;
                
                        if(msg->payload_length == 0)
                            {
                                // we received an ACK
                                //if(DEBUG) printf("Client received an ACK for msg %d\n",msg->seqnum);
                                //printf("\nClient received an ACK for msg with sequence number %d\n",msg->seqnum);
                                if(msg->seqnum == (client->connection->lastReceivedAck + 1)){
                                    // this sequence number is next in line, even if it overflows
                                    client->connection->lastReceivedAck = msg->seqnum;
                                }
                                if(client->connection->outbox.size() > 0 && msg->seqnum == client->connection->outbox.front()->seqnum) {
                                    delete client->connection->outbox.front();
                                    client->connection->outbox.pop();
                                }
                            } else {
                            // data packet
                            if(DEBUG) printf("Client received msg %d\n",msg->seqnum);
                            //printf("\nClient received a msg with seqnum %d\n",msg->seqnum);
                            if(msg->seqnum == (client->connection->lastReceivedSeq + 1)){
                                // next in the list
                                client->connection->lastReceivedSeq++;
                                client->inbox.push(msg);
                        
                                // send ack for this message
                                network_acknowledge_client(client->connection,client->clnt);
                            }
                        }
                
                        pthread_mutex_unlock(&(client->mutex));
                    }
            }
    }
    if(DEBUG) printf("Read Thread exiting\n");
    return NULL;
}

// this write thread will ensure that messages can be sent/received faster than only
// on epoch boundaries. It will continuously poll for messages that are eligible to
// bet sent for the first time, and then send them out.
void* ClientWriteThread(void *params){
    
    lsp_client *client = (lsp_client*)params;
    
    // continuously poll for new messages to send;
    // Exit when the client is disconnected
    
    unsigned int lastSent = 0;
    
    while(true)
        {
            pthread_mutex_lock(&(client->mutex));
            Status state = client->connection->status;
        
            if(state == DISCONNECTED)
                break;
            
            unsigned int nextToSend = client->connection->lastReceivedAck + 1;
            if(nextToSend > lastSent)
                {
                    // we have received an ack for the last message, and we haven't sent the
                    // next one out yet, so if it exists, let's send it now
                    if(client->connection->outbox.size() > 0) 
                        {
                            
   
                            //printf("\n The sequence number is %d\n",client->connection->outbox.front()->seqnum);
                
                            bool_t* value=server_recv_mssg_1(client->connection->outbox.front(),client->clnt);
                            //printf("The return value of server_recv_mssg_1 is %d \n",*value);
                
                            lastSent = client->connection->outbox.front()->seqnum;
                        }                
                }
            pthread_mutex_unlock(&(client->mutex));
            usleep(5000); // 5ms
        }
    pthread_mutex_unlock(&(client->mutex));
    return NULL;
}

void cleanup_client(lsp_client *client){
    // wait for threads to close
    void *status;
    if(client->readThread)
        pthread_join(client->readThread,&status);
    if(client->writeThread)
        pthread_join(client->writeThread,&status);
    if(client->epochThread)
        pthread_join(client->epochThread,&status);
    
    // cleanup the memory and connection
    pthread_mutex_destroy(&(client->mutex));
    cleanup_connection(client->connection);
    delete client;
}

void cleanup_connection(Connection *s){
    if(!s)
        return;

    // close the file descriptor and free memory
    /*if(s->fd != -1)
      close(s->fd);
      delete s->addr;*/
    delete s;
}
    
