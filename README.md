RPC_Password_Cracker
====================

This Application can crack passwords consisting of alphabets [ a-z ] . It has "Server , Client and Workers" where the clients issue crack requests to Server. The server assigns the requests to available workers. At a given point of time multiple clients can issue requests. The server assigns these requests to workers such that there is load balancing among the workers. All the communication was carried out using RPC. We used "rpcgen" compiler to generate the code between stubs.
