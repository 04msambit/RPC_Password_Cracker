// John Keech, UIN:819000713, CSCE 438 - HW2

// This file creates the Live Sequence Protocol (LSP)
// API that can be used by both clients and servers.

#ifndef LSP_API
#define LSP_API

#include "datatypes.h"
#include "network.h"

// Global Parameters. For both server and clients.
#define _EPOCH_LTH 2.0
#define _EPOCH_CNT 5

// Set length of epoch (in seconds)
void lsp_set_epoch_lth(double lth);

// Set number of epochs before timing out
void lsp_set_epoch_cnt(int cnt);

// Set fraction of packets that get dropped along each connection
void lsp_set_drop_rate(double rate);


#endif
