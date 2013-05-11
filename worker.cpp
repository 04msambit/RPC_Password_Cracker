// John Keech, UIN 819000713, CSCE 438 HW2

#include "lsp_client.h"
#include <openssl/sha.h>

void getNext(char *pass, int len){
    // given a current password, compute the next one to try and
    // store it in place
    // Ex. aaaa -> aaab; aaaz -> aaba;
    
    for(int pos = len-1; pos >= 0; pos--){
        if(pass[pos] != 'z'){
            pass[pos]++;
            break;
        } else {
            pass[pos] = 'a';
        }
    }
}

char* crack(char *hash, char *start, char *end){
    // attempts to crack the password with brute force
    // returns NULL if no match

    printf("attempting to crack hash %s (%s-%s)\n",hash,start,end);
    char *cur = start;
    int len = strlen(cur);
    unsigned char tmp[20];
    char computedHash[41];
    while(true){
        // hash the current one and compare it        
        SHA1((const unsigned char*)cur,len,tmp);
        for(int i = 0; i < 20; i++){
            sprintf(&(computedHash[i*2]),"%02x",tmp[i]);
        }
        //printf("%s: %s\n",cur,computedHash);
        if(memcmp(hash,&computedHash,40) == 0){
            // found a match
            return cur;
        }
        if(memcmp(cur,end,len) == 0) {
            // we just tried the last one, so return
            return NULL;
        }
        getNext(cur,len);
    }
}

int main(int argc, char* argv[]){
    // randomization seed requested by Dr. Stoleru
    srand(12345);
    
    if(argc < 2) {
        printf("Usage: ./worker host:port\n");
        return -1;
    }

    if(argc == 4) {
        // manual command line testing for debugging
        // expects to be called as: ./worker hash start end
        
        // copy the 'start' sequence at the beginning because
        // it is overwritten during the cracking process and we
        // want to print it out at the end
        char *start = new char[strlen(argv[2])];
        memcpy(start,argv[2],strlen(argv[2]));
        start[strlen(argv[2])] = '\0';
        char *pass = crack(argv[1],argv[2],argv[3]);
        if(pass)
            printf("cracked hash %s (%s-%s): %s\n",argv[1],start,argv[3],pass);
        else
            printf("failed to crack hash %s (%s-%s)\n",argv[1],start,argv[3]);
        delete start;
        return 0;
    }
    
    // parse the port from the input args
    char* portstr = strstr(argv[1],":");
    int port;
    if(portstr != NULL) {
        port = atoi(portstr+1);
    } else {
        printf("Usage: ./worker host:port\n");
        return -1;
    }
    //portstr[0] = NULL;
    portstr[0]=0;
    
    char* hash = argv[2];
    int len = atoi(argv[3]);
    
    //lsp_set_drop_rate(0.2); // 20% of packets dropped
    //lsp_set_epoch_lth(0.1); // 100 ms per epoch = fast resends on failure
    //lsp_set_epoch_cnt(20); // 20 epochs (2 seconds) with no response
    
    lsp_client *client = lsp_client_create(argv[1], port);
    if(!client){
        // the connection to the server could not be made
        printf("The connection to the server failed. Exiting...\n");
        return -1;
    }
    
    printf("The connection to the server has been established\n");
    
    // send join message to the server
    char buffer[1024];
    sprintf(buffer,"j");
    lsp_client_write(client,(uint8_t*)buffer,2);

    // wait for request chunks and then processes them
    while(int bytes_read = lsp_client_read(client,(uint8_t*)buffer)){
        // we have received a crack request. let's parse it
        if(buffer[0] == 'c'){
            char *hash = strtok(buffer+2," ");
            int bufLen = 0;

            // verify hash is valid
            if(strlen(hash) != 40) {
                printf("Invalid hash: %s\n",hash);
                bufLen = sprintf(buffer,"x"); // send "not found" back to server
                lsp_client_write(client,(uint8_t*)buffer,bufLen+1);
                continue;
            }

            // grab start and end sequences
            char *start = strtok(NULL, " ");
            char *end = strtok(NULL, " ");
            
            char *pass = crack(hash,start,end);

            // build response message
            if(pass)
                bufLen = sprintf(buffer,"f %s",pass);
            else 
                bufLen = sprintf(buffer, "x");

            // send the response back to the server
            lsp_client_write(client,(uint8_t*)buffer,bufLen+1);
        } else {
            printf("Unknown message format: %s\n",buffer); 
        }
    }
    // the connection to the server was lost
    lsp_client_close(client);    
    return 0;
}
