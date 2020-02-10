#ifndef CONNECTOR_HPP
#define CONNECTOR_HPP
#include <string>
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"

// typedef struct connstate connstate;

typedef void (*on_flow_msg) ( solClient_opaqueFlow_pt, solClient_opaqueMsg_pt, void* );

typedef struct connstate 
{
	solClient_opaqueContext_pt ctx_;
	solClient_opaqueSession_pt sess_;
	solClient_opaqueFlow_pt    flow_;
	on_flow_msg                flowcb_;
	void*                      flowdata_;
} connstate;

/** 
 * Initialize the session for the Solace eventbroker.
 **/
connstate init ();

/**
 * Connect the Solace session.
 **/
void 
connect( connstate& state, const std::string& host, const std::string& vpn, const std::string& user, const std::string& pass);

void disconnnect(connstate& state);

/**
 * Bind to the Solace queue to consume messages via the user's callback function.
 **/
void
openqueue(connstate& state, const std::string& qname, on_flow_msg msgcallback, void* user_p);

void closequeue(connstate& state);

/**
 * Send a message on the Solace Session stored in the connstate parameter.
 **/
void sendmsg(connstate& state, solClient_opaqueMsg_pt msg_p);

#endif // CONNECTOR_HPP
