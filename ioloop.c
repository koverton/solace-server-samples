#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"

struct state {
	int  count_;
	char connected_;
};

solClient_rxMsgCallback_returnCode_t
on_msg ( solClient_opaqueSession_pt sess_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
	struct state *sstate = (struct state*) user_p;
	solClient_log(SOLCLIENT_LOG_NOTICE, "Received message %d:", ++(sstate->count_) );
	solClient_msg_dump ( msg_p, NULL, 0 );
	return SOLCLIENT_CALLBACK_OK;
}

void
on_evt ( solClient_opaqueSession_pt sess_p, solClient_session_eventCallbackInfo_pt info_p, void *user_p )
{
	struct state *sstate = (struct state*) user_p;
	switch ( info_p->sessionEvent ) {
		/* it is best practice to wait for this before using the bus */
		case SOLCLIENT_SESSION_EVENT_UP_NOTICE:
		case SOLCLIENT_SESSION_EVENT_RECONNECTED_NOTICE:
			solClient_log(SOLCLIENT_LOG_NOTICE, "CONNECTED EVENT");
			sstate->connected_ = 1;
			break;
		/* any behavior that needs to change in this event, implement here */
		case SOLCLIENT_SESSION_EVENT_RECONNECTING_NOTICE:
			solClient_log(SOLCLIENT_LOG_NOTICE, "TRYING TO RECONNECT");
			sstate->connected_ = 0;
			break;

		/* these are all just good news */
		case SOLCLIENT_SESSION_EVENT_ACKNOWLEDGEMENT:
			/* event-ack is the appliance ACKing a published message
			 * which should be important for publishers to account for */
			solClient_log(SOLCLIENT_LOG_NOTICE, "MSG-ACK EVENT");
			break;
		case SOLCLIENT_SESSION_EVENT_TE_UNSUBSCRIBE_OK:
		case SOLCLIENT_SESSION_EVENT_CAN_SEND:
		case SOLCLIENT_SESSION_EVENT_PROVISION_OK:
		case SOLCLIENT_SESSION_EVENT_SUBSCRIPTION_OK:
			break;

		/* it is best practice to use this -- the solace connection could not be restored */
		case SOLCLIENT_SESSION_EVENT_DOWN_ERROR:
		case SOLCLIENT_SESSION_EVENT_CONNECT_FAILED_ERROR:
			solClient_log(SOLCLIENT_LOG_ERROR, "DISCONNECT EVENT");
			sstate->connected_ = 0;
			break;
		/* it is best practice to log these at ERROR level */
		case SOLCLIENT_SESSION_EVENT_REJECTED_MSG_ERROR:
		case SOLCLIENT_SESSION_EVENT_SUBSCRIPTION_ERROR:
		case SOLCLIENT_SESSION_EVENT_RX_MSG_TOO_BIG_ERROR:
		case SOLCLIENT_SESSION_EVENT_TE_UNSUBSCRIBE_ERROR:
		case SOLCLIENT_SESSION_EVENT_PROVISION_ERROR:
			solClient_log(SOLCLIENT_LOG_ERROR, "MSG/SUBSCRIPTION FAILURE EVENT");
			break;

		default:
			/* Unrecognized or deprecated events are output to STDOUT. */
			solClient_log(SOLCLIENT_LOG_WARNING, "common_eventCallback() called - %s.  Unrecognized or deprecated event.",
					 solClient_session_eventToString(info_p->sessionEvent));
			break;
	}
}

int
main ( int argc, char* argv[] )
{
	solClient_opaqueContext_pt ctx_p;
	solClient_context_createFuncInfo_t cfninfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;
	solClient_opaqueSession_pt sess_p;
	solClient_session_createFuncInfo_t sfninfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;

	struct state sessState = { 0, 0 };

	const char *sprops[20];
	int         pi = 0;

	const char *host, *vpn, *user, *topic;

	if ( argc < 5 ) {
		printf ( "Usage: IOLoop <msg_backbone_ip:port> <vpn> <client-username> <topic>\n" );
		return -1;
	}

	host  = argv[1];
	vpn   = argv[2];
	user  = argv[3];
	topic = argv[4];

	solClient_initialize ( SOLCLIENT_LOG_DEFAULT_FILTER, NULL );
	solClient_log(SOLCLIENT_LOG_NOTICE, "IOLoop initializing..." );

	/* 
	 * Create a NULL Context, which means no Context Thread is created and the application 
	 * is now responsible for driving the event loop.
	 */
	solClient_context_create ( NULL, &ctx_p, &cfninfo, sizeof(cfninfo) );

	sfninfo.rxMsgInfo.callback_p = on_msg;
	sfninfo.rxMsgInfo.user_p     = &sessState;
	sfninfo.eventInfo.callback_p = on_evt;
	sfninfo.eventInfo.user_p     = &sessState;

	pi = 0;
	sprops[pi++] = SOLCLIENT_SESSION_PROP_HOST;
	sprops[pi++] = host;
	sprops[pi++] = SOLCLIENT_SESSION_PROP_VPN_NAME;
	sprops[pi++] = vpn;
	sprops[pi++] = SOLCLIENT_SESSION_PROP_USERNAME;
	sprops[pi++] = user;
	/* disable blocking connect because there is no context thread to 
	 * pump the eventloop so we would hang waiting for connect to complete. */
	sprops[pi++] = SOLCLIENT_SESSION_PROP_CONNECT_BLOCKING;
	sprops[pi++] = SOLCLIENT_PROP_DISABLE_VAL;
	sprops[pi]   = NULL;

	solClient_session_create ( sprops, ctx_p, &sess_p, &sfninfo, sizeof(sfninfo) );
	solClient_session_connect ( sess_p );
	/* Wait until the connection is fully established before moving forward. */
	while ( !sessState.connected_ ) {
		solClient_context_processEvents( ctx_p );
	}

	solClient_log(SOLCLIENT_LOG_NOTICE, "MAINLOOP: Connected. Now subscribing...");
	solClient_session_topicSubscribeExt ( sess_p, 0, topic );

	solClient_log(SOLCLIENT_LOG_NOTICE, "Waiting for message......" );
	fflush ( stdout );
	while ( sessState.count_ < 1 ) {
		/* This blocks for up to 50ms by default; the blocking duration can be 
		 * changed using solClient_context_processEventsWait( ctx_p, millisecs )
		 * where 0 causes a hardspin */
		solClient_context_processEvents( ctx_p );
	}

	solClient_log(SOLCLIENT_LOG_NOTICE, "Exiting." );

	solClient_session_topicUnsubscribeExt ( sess_p, 0, topic );

	solClient_cleanup ( );

	return 0;
}
