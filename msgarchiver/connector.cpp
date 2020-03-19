#include "connector.hpp"
#include "sol_props.hpp"
#include <iostream>


solClient_rxMsgCallback_returnCode_t
on_dir_msg ( solClient_opaqueSession_pt sess, solClient_opaqueMsg_pt msg, void *user_p )
{
	return SOLCLIENT_CALLBACK_OK;
}

void
on_evt ( solClient_opaqueSession_pt sess, solClient_session_eventCallbackInfo_pt evt, void *user_p )
{ 
	connstate* state_p = (connstate*) user_p;
	msgcorrobj_pt corr  = (msgcorrobj_pt) evt->correlation_p;
	if ( state_p == NULL ) std::cout << "\tWARNING: Userdata on acknowledgment event was NULL" << std::endl;
	if ( ( evt->sessionEvent ) == SOLCLIENT_SESSION_EVENT_ACKNOWLEDGEMENT ) {
		// std::cout << "\tINFO: Acknowledgement received!" << std::endl;
		if ( corr != NULL ) {
			corr->isAcked = true;
			corr->isAccepted = true;
		}
	}
	else if ( ( evt->sessionEvent ) == SOLCLIENT_SESSION_EVENT_REJECTED_MSG_ERROR ) {
		std::cout << "\tERR: Negative ACK received!" << std::endl;
		if ( corr != NULL ) {
			corr->isAcked = true;
			corr->isAccepted = false;
		}
	}

}

void
on_flow_evt ( solClient_opaqueFlow_pt flow, solClient_flow_eventCallbackInfo_pt evt, void *user_p )
{ }


/***
 * Callback for Solace message events. The callback from the user-data passes control back to the application.
 ***/
solClient_rxMsgCallback_returnCode_t
on_msg ( solClient_opaqueFlow_pt flow, solClient_opaqueMsg_pt msg_p, void *user_p )
{
	connstate* state_p = (connstate*) user_p;
	if ( true != state_p->flowcb_( flow, msg_p, state_p->flowdata_ ) ) {
		// TODO: Log event strongly
		return SOLCLIENT_CALLBACK_OK; // there is no fail return code; just don't ack
	}
	solClient_msgId_t msgId;
	if ( solClient_msg_getMsgId ( msg_p, &msgId ) == SOLCLIENT_OK ) {
		solClient_flow_sendAck ( flow, msgId );
	}
	return SOLCLIENT_CALLBACK_OK;
}


connstate init()
{
	connstate state = { 0, 0, 0, 0 };
	solClient_context_createFuncInfo_t cfninfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;
	solClient_initialize ( SOLCLIENT_LOG_DEFAULT_FILTER, NULL );
	solClient_context_create ( SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
				&state.ctx_, &cfninfo, sizeof(cfninfo) );
	return state;
}

void 
connect( connstate& state, const std::string& propsfile ) {
	const char** sprops = read_props( propsfile.c_str() );

	solClient_session_createFuncInfo_t sfninfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
	sfninfo.rxMsgInfo.callback_p = on_dir_msg;
	sfninfo.rxMsgInfo.user_p = &state;
	sfninfo.eventInfo.callback_p = on_evt;
	sfninfo.eventInfo.user_p = &state;

	solClient_session_create ( sprops, state.ctx_, &state.sess_, &sfninfo, sizeof(sfninfo) );
	solClient_session_connect ( state.sess_ );
}

void
disconnnect(connstate& state) {
	solClient_session_disconnect ( state.sess_ );
	solClient_cleanup (  );
}



// TODO: Would much prefer to template this to the specific user_p type, 
//       but that requires templating on_flow_msg to match it
void
openqueue(connstate& state, const std::string& qname, on_flow_msg msgcallback, void* user_p) {
	const char	 *fprops[20];
	int			 pi = 0;
	fprops[pi++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
	fprops[pi++] = SOLCLIENT_PROP_DISABLE_VAL;
	fprops[pi++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
	fprops[pi++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;
	fprops[pi++] = SOLCLIENT_FLOW_PROP_ACKMODE;
	fprops[pi++] = SOLCLIENT_FLOW_PROP_ACKMODE_CLIENT;
	fprops[pi++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
	fprops[pi++] = qname.c_str();
	fprops[pi++] = NULL;

	state.flowcb_  = msgcallback;
	state.flowdata_= user_p;

	solClient_flow_createFuncInfo_t ffninfo = SOLCLIENT_FLOW_CREATEFUNC_INITIALIZER;
	ffninfo.rxMsgInfo.callback_p = on_msg;
	ffninfo.rxMsgInfo.user_p     = &state;
	ffninfo.eventInfo.callback_p = on_flow_evt;
	ffninfo.eventInfo.user_p     = &state;

	solClient_session_createFlow ( fprops, state.sess_, &state.flow_, &ffninfo, sizeof(ffninfo) );
}

void
closequeue(connstate& state) {
	solClient_flow_destroy ( &state.flow_ );
}

static int loop = 1;
bool
setcorr(connstate& state, solClient_opaqueMsg_pt msg_p) {
	state.msgPool_p = ( msgcorrobj_pt ) malloc ( sizeof ( msgcorrobj ) );
	/* Store the message information in message memory array. */
	state.msgPool_p->next_p = NULL;
	state.msgPool_p->msgId = loop++;
	state.msgPool_p->msg_p = msg_p;
	state.msgPool_p->isAcked = false;
	state.msgPool_p->isAccepted = false;
	if ( state.msgPoolTail_p != NULL )
	    state.msgPoolTail_p->next_p = state.msgPool_p;
	if ( state.msgPoolHead_p == NULL )
	    state.msgPoolHead_p = state.msgPool_p;
	state.msgPoolTail_p = state.msgPool_p;
	/*
	 * For correlation to take effect, it must be set on the message prior to
	 * calling send. Note: the size parameter is ignored in the API.
	 */
	int rc = SOLCLIENT_OK;
	if ( ( rc = solClient_msg_setCorrelationTagPtr ( msg_p, state.msgPool_p, sizeof (*state.msgPool_p) ) ) != SOLCLIENT_OK ) {
		std::cerr << rc << "solClient_msg_setCorrelationTag()"  << std::endl;
		return false;
	}
	return true;
}

void
sendmsg(connstate& state, solClient_opaqueMsg_pt msg_p) {
	solClient_msg_setDeliveryMode ( msg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT );
	if ( !setcorr( state, msg_p ) ) {
		std::cerr << "CRAP! setting correlation object didn't work!" << std::endl;
	}
	if ( solClient_session_sendMsg( state.sess_, msg_p ) != SOLCLIENT_OK ) {
		std::cerr << "CRAP! sendMsg didn't work!" << std::endl;
	}
}

void
cleanmsgstore(connstate& state) {
	while ( ( state.msgPoolHead_p != NULL ) && state.msgPoolHead_p->isAcked ) {
		//std::cout <<  "\tFreeing memory for message " << state.msgPoolHead_p->msgId 
		//	<< ", Result: Acked (" << state.msgPoolHead_p->isAcked
		//	<< "), Accepted (" << state.msgPoolHead_p->isAccepted
		//	<< ")" << std::endl;
		state.msgPool_p = state.msgPoolHead_p;
		if ( ( state.msgPoolHead_p = state.msgPoolHead_p->next_p ) == NULL ) {
			state.msgPoolTail_p = NULL;
		}
		solClient_msg_free ( &( state.msgPool_p->msg_p ) );
		free ( state.msgPool_p );
	}
}

