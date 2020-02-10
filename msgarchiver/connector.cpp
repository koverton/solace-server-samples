#include "connector.hpp"
#include <iostream>


solClient_rxMsgCallback_returnCode_t
on_dir_msg ( solClient_opaqueSession_pt sess, solClient_opaqueMsg_pt msg, void *user_p )
{
	return SOLCLIENT_CALLBACK_OK;
}

void
on_evt ( solClient_opaqueSession_pt sess, solClient_session_eventCallbackInfo_pt evt, void *user_p )
{ }

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
	state_p->flowcb_( flow, msg_p, state_p->flowdata_ );
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
connect( connstate& state, const std::string& host, const std::string& vpn, const std::string& user, const std::string& pass) {
	const char	 *sprops[20];
	int			 pi = 0;
	sprops[pi++] = SOLCLIENT_SESSION_PROP_HOST;
	sprops[pi++] = host.c_str();
	sprops[pi++] = SOLCLIENT_SESSION_PROP_VPN_NAME;
	sprops[pi++] = vpn.c_str();
	sprops[pi++] = SOLCLIENT_SESSION_PROP_USERNAME;
	sprops[pi++] = user.c_str();
	sprops[pi++] = SOLCLIENT_SESSION_PROP_PASSWORD;
	sprops[pi++] = pass.c_str();
	sprops[pi++] = NULL;

	solClient_session_createFuncInfo_t sfninfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
	sfninfo.rxMsgInfo.callback_p = on_dir_msg;
	sfninfo.eventInfo.callback_p = on_evt;

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


void
sendmsg(connstate& state, solClient_opaqueMsg_pt msg_p) {
	if ( solClient_session_sendMsg( state.sess_, msg_p ) != SOLCLIENT_OK ) {
		std::cerr << "CRAP! sendMsg didn't work!" << std::endl;
	}
}

