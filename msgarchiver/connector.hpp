#ifndef CONNECTOR_HPP
#define CONNECTOR_HPP
#include <string>
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"

// typedef struct connstate connstate;

typedef bool (*on_flow_msg) ( solClient_opaqueFlow_pt, solClient_opaqueMsg_pt, void* );

typedef struct msgcorrobj
{
	struct msgcorrobj *next_p;
	int	     msgId;       /**< The message ID. */
	solClient_opaqueMsg_pt msg_p; /**< The message pointer. */
	bool	    isAcked;      /**< A flag indicating if the message has been acknowledged by the appliance (either success or rejection). */
	bool	    isAccepted;   /**< A flag indicating if the message is accepted or rejected by the appliance when acknowledged.*/
} msgcorrobj, *msgcorrobj_pt;     /**< A pointer to ::msgcorrobj structure of information. */

typedef struct connstate 
{
	solClient_opaqueContext_pt ctx_;
	solClient_opaqueSession_pt sess_;
	solClient_opaqueFlow_pt    flow_;
	on_flow_msg                flowcb_;
	void*                      flowdata_;
	msgcorrobj_pt              msgPool_p;
	msgcorrobj_pt              msgPoolHead_p;
	msgcorrobj_pt              msgPoolTail_p;
} connstate;

/** 
 * Initialize the session for the Solace eventbroker.
 **/
connstate init ();

/**
 * Connect the Solace session.
 **/
void 
connect( connstate& state, const std::string& propsfile );

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

void cleanmsgstore(connstate& state);

#endif // CONNECTOR_HPP
