#include "lsp_server.h"
#include <math.h>

#define MAX_REQ_SIZE 500000

typedef struct {
    uint32_t requester;
    char *hash;
    char *lower;
    char *upper;
} request;

request* divide_req(request *req, int req_size) {
   req_size--;
   
   // create new request and copy request info from req
   request* ret_req = new request();
   ret_req->requester = req->requester;
   ret_req->hash = req->hash;
   int len = strlen(req->lower);
   
   ret_req->lower = new char[len];
   strcpy(ret_req->lower,req->lower);
   ret_req->upper = new char[len];
   strcpy(ret_req->upper,req->lower);

   // try to add the req_size to the ret_req->lower word
   int carried_one = 0;
   for(int pos = len-1; pos >= 0; pos--) {
       int pos_addition = req_size % 26;
       req_size /= 26;
       if(pos_addition > 0 || carried_one) {
           int cur_pos_val = ret_req->upper[pos] - 'a';
           int val = cur_pos_val + pos_addition + carried_one;
           ret_req->upper[pos] = 'a' + (val % 26);
           carried_one = val / 26;
       }
       if(req_size == 0 && carried_one == 0) {
           break;
       }
   }
   
   // if all of req_size did not get added to ret_req->lower or ret_req->lower == req->upper
   // then req is small enough and should not be divided
   if(req_size > 0 || carried_one > 0 || strcmp(ret_req->upper,req->upper) == 0) {
       delete[] ret_req->lower;
       delete[] ret_req->upper;
       delete ret_req;
       return NULL;
   }

   // set req->lower = ret_req->upper+1
   strcpy(req->lower,ret_req->upper);
   for(int pos = len-1; pos >= 0; pos--) {
       if(req->lower[pos] != 'z'){
           req->lower[pos]++;
           break;
       } else {
       req->lower[pos] = 'a';
       }
   }
   return ret_req;
};

int main(int argc, char* argv[]){
    // randomization seed requested by Dr. Stoleru
    srand(12345);
    
    if(argc < 2) {
        printf("Usage: ./server port\n");
        return -1;
    }

    //lsp_set_drop_rate(0.2); // 20% of packets dropped
    //lsp_set_epoch_lth(0.1); // 100 ms per epoch = fast resends on failure
    //lsp_set_epoch_cnt(20); // 20 epochs (2 seconds) with no response
    
    // create a server
    lsp_server *server = lsp_server_create(atoi(argv[1]));
    if(!server)
        return -1;

    // keeps track of available worker who are not currently wrokinrg
    std::queue<uint32_t> inactive_workers; 
    
    // keeps track of active requests sent to worker
    std::map<uint32_t, request*> active_requests;

    // keeps track of next request to be worked on
    std::queue<request*> next_req_queue;

    uint8_t payload[2048];
    uint32_t returned_id;
    int bytes_read;
    
    printf("Server started. Waiting for messages...\n");
    
    while(true){
        // wait for data from clients
        bytes_read = lsp_server_read(server,payload,&returned_id);
        
        // if bytes_read > 0 then there is data to be read
        // if bytes_read == 0 then a client disconnected
        if(bytes_read) {
            printf("[%d]: %s\n",returned_id, payload);
            if(payload[0] == 'j') { // worker joined the server
                inactive_workers.push(returned_id); // add to inactive_workers
            } else if(payload[0] == 'x') { // worker did not find correct password for request
                request *req = active_requests[returned_id]; 
                // request may have been deleted if password was already found or
                // the request client disconnected
                if (req != NULL) {
                    // delete request for active_requests
                    active_requests.erase(returned_id);

                    // find out if this is the last request chuck for password crack request
                    bool finished = true;
                    for(std::map<uint32_t, request*>::iterator it = active_requests.begin(); 
                        it != active_requests.end(); it++) {
                        request *act_req = it->second;
                        if(act_req != NULL && act_req->requester == req->requester) {
                            finished = false;
                            break;
                        }
                    }
                    if(finished) {
                        for(int k = next_req_queue.size(); k > 0; k--) {
                            request *next_req = next_req_queue.front();
                            next_req_queue.pop();
                            if(next_req->requester == req->requester) {
                                finished = false;
                            } 
                            next_req_queue.push(next_req);
                        }
                    }

                    // if this is the last request then forward password not found
                    // message to the request client and delete request data
                    if(finished) {
                        lsp_server_write(server,payload,bytes_read,req->requester);
                        delete[] req->hash;
                    }
                    delete[] req->lower;
                    delete[] req->upper;
                    delete req;
                }
                inactive_workers.push(returned_id);
            } else if(payload[0] == 'f') { // worker found the correct password
                request *req = active_requests[returned_id];
                
                // send correct password to request client
                lsp_server_write(server,payload,bytes_read,req->requester);
                
                active_requests.erase(returned_id);
                
                // delete all corresponding request data in next_req_queue
                for(int k = next_req_queue.size(); k > 0; k--) {
                    request *next_req = next_req_queue.front();
                    next_req_queue.pop();
                    if(next_req->requester == req->requester) {
                        delete[] next_req->lower;
                        delete[] next_req->upper;
                        delete next_req;
                    } else {
                        next_req_queue.push(next_req);
                    }
                }
                // delete all corresponding request data in active_requests map
                for(std::map<uint32_t, request*>::iterator it = active_requests.begin(); 
                    it != active_requests.end(); it++) {
                    request *act_req = it->second; 
                    if(act_req != NULL && act_req->requester == req->requester) {
                        delete[] it->second->lower;
                        delete[] it->second->upper;
                        delete it->second;
                        active_requests.erase(it);
                    }
                }
                delete[] req->hash;
                delete[] req->lower;
                delete[] req->upper;
                delete req;

                inactive_workers.push(returned_id);
            } else if(payload[0] == 'c') { // crack request received for request client
                // create new request and copy data to request struct
                request *req = new request();
                req->requester = returned_id;
                
                char *start_pt = strchr((char*)payload,' ')+1;
                char *end_pt = strchr(start_pt,' ');
                int len = end_pt-start_pt;
                req->hash = new char[len+1];
                memcpy(req->hash,start_pt,end_pt-start_pt);
                req->hash[end_pt-start_pt] = '\0';
                
                start_pt = end_pt+1;
                end_pt = strchr(start_pt,' ');
                len = end_pt-start_pt;
                req->lower = new char[len+1];
                memcpy(req->lower,start_pt,end_pt-start_pt);
                req->lower[end_pt-start_pt] = '\0';
                
                start_pt = end_pt+1;
                len = end_pt-start_pt;
                req->upper = new char[len+1];
                strcpy(req->upper,start_pt);
                
                next_req_queue.push(req);
            }
            
            // when there are inactive workers and work to be done
            // send out next crack request to next available worker
            while(inactive_workers.size() > 0 && next_req_queue.size() > 0) {
                // send work requests
                request *req = next_req_queue.front();
                next_req_queue.pop();
                request *next_req = divide_req(req,MAX_REQ_SIZE);
                if(next_req) { // request was bigger than MAX_REQ_SIZE so it 
                               // was divided into 2 requests
                    int payload_size = sprintf((char*)payload,"c %s %s %s",
                        next_req->hash,next_req->lower,next_req->upper);
                    printf("[%s]: %s -> %s, %s -> %s\n",req->hash,next_req->lower,
                        next_req->upper,req->lower,req->upper);
                    
                    lsp_server_write(server,payload,payload_size,inactive_workers.front());
                    active_requests[inactive_workers.front()] = next_req;
                    next_req_queue.push(req);
                } else { // request was small enough and was not divided into 2 chunks
                    int payload_size = sprintf((char*)payload,"c %s %s %s",
                        req->hash,req->lower,req->upper);
                    printf("r[%s]: %s -> %s\n",req->hash,req->lower,req->upper);
                    
                    lsp_server_write(server,payload,payload_size,inactive_workers.front());
                    active_requests[inactive_workers.front()] = req;
                }
                inactive_workers.pop();
            }
        } else { // client disconnected
            printf("Client %d disconnected\n",returned_id);
            request *req = active_requests[returned_id];
            
            if(req != NULL) { // client was an active worker
                // add incompleted crack request back to next_req_queue
                next_req_queue.push(req);
                active_requests.erase(returned_id);
            } else {
                // check to see if client was inactive worker
                // and delete worker from map if it was
                bool is_worker = false;
                for(int k = inactive_workers.size(); k > 0; k--) {
                    uint32_t worker_id = inactive_workers.front();
                    inactive_workers.pop();
                    if(worker_id == returned_id) {
                        is_worker = true;
                        break;
                    } else {
                        inactive_workers.push(worker_id);
                    }
                }

                // if not a worker, then client was a requester
                if(!is_worker) {
                    // delete all data associated with request client
                    bool hash_deleted = false;
                    for(int k = next_req_queue.size(); k > 0; k--) {
                        req = next_req_queue.front();
                        next_req_queue.pop();
                        if(req->requester == returned_id) {
                            if(!hash_deleted) {
                                delete[] req->hash;
                                hash_deleted = true;
                            }
                            delete[] req->lower;
                            delete[] req->upper;
                            delete req;
                        } else {
                            next_req_queue.push(req);
                        }
                    }
                    for(std::map<uint32_t, request*>::iterator it = active_requests.begin(); 
                        it != active_requests.end(); it++) {
                        req = it->second;
                        if(req != NULL && req->requester == returned_id) {
                            req = it->second;
                            if(!hash_deleted) {
                                delete[] req->hash;
                                hash_deleted = true;
                            }
                            delete[] req->lower;
                            delete[] req->upper;
                            delete req;
                            active_requests.erase(it);
                        }
                    }
                }
            }
        }
    }        
    return 0;
}
