struct message
{
  uint32_t connid;
  uint32_t seqnum;
  string  payload<1500>; 
  int payload_length;
};

program RPC_PROG {
	version SERVER_VERS {
		int SERVER_RECV_MSSG(message)  = 1;	/* procedure number = 1 */
                message SERVER_SEND_MSSG(uint32_t connid)  = 2;   /* procedure number = 2 */
	} = 1;
} = 0x33389897;	/* replace 88888 with some random numbers */
