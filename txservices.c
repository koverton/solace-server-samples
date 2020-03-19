#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

void
on_err ( solClient_returnCode_t rc, const char *errstr )
{
    solClient_errorInfo_pt info = solClient_getLastErrorInfo ();
    solClient_log ( SOLCLIENT_LOG_ERROR,
                    "%s - ReturnCode=\"%s\", SubCode=\"%s\", ResponseCode=%d, Info=\"%s\"",
                    errstr, solClient_returnCodeToString(rc),
                    solClient_subCodeToString(info->subCode), 
                    info->responseCode, 
                    info->errorStr );
    solClient_resetLastErrorInfo ();
}

solClient_rxMsgCallback_returnCode_t
freemsg(solClient_opaqueMsg_pt repmsg) {
    solClient_returnCode_t rc = solClient_msg_free ( &repmsg );
    if ( rc != SOLCLIENT_OK ) {
        on_err ( rc, "solClient_msg_free()" );
    }
    return SOLCLIENT_CALLBACK_OK;
}
/*
 * Replier consumer flow Rx message callback
 *   It sends a reply for a received request and then commits the transaction.
 */
static solClient_rxMsgCallback_returnCode_t
on_flow_msg (
    solClient_opaqueFlow_pt flow,
    solClient_opaqueMsg_pt msg,
    void *user_p )
{
    solClient_returnCode_t               rc;
    solClient_destination_t              replyto;
    solClient_opaqueMsg_pt               repmsg = NULL;
    solClient_opaqueTransactedSession_pt txsess = NULL;    

    printf("\n\tMSG on thread [%lx] -- this is a separate TX-dispatcher thread\n", (unsigned long)pthread_self());
    
    /* Get ReplyTo address */
    rc = solClient_msg_getReplyTo(  msg, &replyto, sizeof(replyto) );
    if ( rc != SOLCLIENT_OK ) {
        on_err ( rc, "solClient_msg_getReplyTo()");
        return SOLCLIENT_CALLBACK_OK;
    }

    /* Get the Transacted Session pointer */
    rc = solClient_flow_getTransactedSession( flow, &txsess );
    if (rc != SOLCLIENT_OK) {
        on_err ( rc, "solClient_flow_getTransactedSession()");
        return SOLCLIENT_CALLBACK_OK;
    }

    printf( "\n\tReplier receives a request message. It sends a reply message and then commits the transaction.\n" );

    /* Create a reply message */
    rc = solClient_msg_alloc ( &repmsg );
    if ( rc != SOLCLIENT_OK ) {
        on_err ( rc, "solClient_msg_alloc()" );
        return SOLCLIENT_CALLBACK_OK;
    }

    /* Set the reply message Destination */
    rc = solClient_msg_setDestination ( repmsg, &replyto, sizeof(replyto) );
    if ( rc != SOLCLIENT_OK ) {
        on_err ( rc, "solClient_msg_setDestination()" );
        return freemsg( repmsg );
    }

    /* Set the reply message Delivery Mode */
    rc = solClient_msg_setDeliveryMode ( repmsg, SOLCLIENT_DELIVERY_MODE_PERSISTENT );
    if ( rc != SOLCLIENT_OK ) {
        on_err ( rc, "solClient_msg_setDeliveryMode()" );
        return freemsg( repmsg );
    }

    /* Send the reply message */
    rc = solClient_transactedSession_sendMsg ( txsess, repmsg );
    if (rc != SOLCLIENT_OK ) {
        on_err ( rc, "solClient_transactedSession_sendMsg()" );
        return freemsg( repmsg );
    }

    /* Commit the consumed message and sent message */
    rc = solClient_transactedSession_commit( txsess );
    if ( rc != SOLCLIENT_OK ) {
        on_err ( rc, "solClient_transactedSession_commit()" );
        return freemsg( repmsg );
    }

    return SOLCLIENT_CALLBACK_OK;
}

void
on_flow_evt (solClient_opaqueFlow_pt flow, solClient_flow_eventCallbackInfo_pt evtinfo, void *user_p)
{
    printf( "\n\tFLOWEVT on thread [%lx] -- this is the Context thread\n\n", (unsigned long)pthread_self() );
}


solClient_rxMsgCallback_returnCode_t
on_sess_msg ( solClient_opaqueSession_pt sess, solClient_opaqueMsg_pt msg, void *user_p )
{
    printf( "WARNING!! DIRECT MESSAGING CALLBACK RATHER THAN FLOW CALLBACK!" );
    return SOLCLIENT_CALLBACK_OK;
}

void
on_sess_evt ( solClient_opaqueSession_pt sess, solClient_session_eventCallbackInfo_pt evtinfo, void *user_p )
{
    printf( "\n\tSESSEVT on thread [%lx] -- this is the Context thread\n\n", (unsigned long)pthread_self() );
}


int
main ( int argc, char *argv[] )
{
    solClient_returnCode_t                rc = SOLCLIENT_OK;
    solClient_opaqueContext_pt            ctx;
    solClient_opaqueSession_pt            sess = NULL;
    solClient_opaqueFlow_pt               reqflow = NULL;
    solClient_opaqueTransactedSession_pt *svc_txsess;
    solClient_opaqueFlow_pt              *svc_flow;
    const char                            sendid[] = "Requestor";
    solClient_destination_t               destination;

    solClient_context_createFuncInfo_t cfninfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;
    solClient_session_createFuncInfo_t sfninfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
    solClient_flow_createFuncInfo_t    ffninfo = SOLCLIENT_FLOW_CREATEFUNC_INITIALIZER;

    const char *sprops[20];
    const char *txprops[20];
    const char *fprops[20];
    int         svccount, i, pi = 0;

    const char *host, *vpn, *user, *pass, *qname;

    if ( argc < 6 ) {
        printf( "\tUSAGE: %s <host_port> <vpn> <username> <password> <request-queue> <#-workers>\n\n", argv[0] );
        return -1;
    }

    host  = argv[1];
    vpn   = argv[2];
    user  = argv[3];
    pass  = argv[4];
    qname = argv[5];
    svccount= atoi( argv[6] );

    svc_txsess = malloc( sizeof(solClient_opaqueSession_pt) * svccount );
    svc_flow   = malloc( sizeof(solClient_opaqueFlow_pt) * svccount );

    solClient_initialize ( SOLCLIENT_LOG_DEFAULT_FILTER, NULL );
    solClient_log(SOLCLIENT_LOG_NOTICE, "Initializing..." );

    /* 
     * Create a NULL Context, which means no Context Thread is created and the application 
     * is now responsible for driving the event pump.
     */
    if ( ( rc = solClient_context_create ( SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
                                           &ctx,
                                           &cfninfo,
                                           sizeof (cfninfo) ) ) != SOLCLIENT_OK ) {
        on_err ( rc, "solClient_context_create()" );
        exit( 1 );
    }

    sfninfo.rxMsgInfo.callback_p = on_sess_msg;
    sfninfo.rxMsgInfo.user_p     = NULL;
    sfninfo.eventInfo.callback_p = on_sess_evt;
    sfninfo.eventInfo.user_p     = NULL;

    pi = 0;
    sprops[pi++] = SOLCLIENT_SESSION_PROP_HOST;
    sprops[pi++] = host;
    sprops[pi++] = SOLCLIENT_SESSION_PROP_VPN_NAME;
    sprops[pi++] = vpn;
    sprops[pi++] = SOLCLIENT_SESSION_PROP_USERNAME;
    sprops[pi++] = user;
    sprops[pi++] = SOLCLIENT_SESSION_PROP_PASSWORD;
    sprops[pi++] = pass;
    sprops[pi]   = NULL;

    solClient_session_create ( sprops, ctx, &sess, &sfninfo, sizeof(sfninfo) );
    solClient_session_connect ( sess );
    printf( "\tSession Connected\n" );


    /****************************************************************
     * Create Transacted Sessions for Service Listeners with their own 
     * dispatcher threads and create Service Listener flows on each one
     ***************************************************************/
    pi = 0;
    txprops[pi++] = SOLCLIENT_TRANSACTEDSESSION_PROP_CREATE_MESSAGE_DISPATCHER;
    txprops[pi++] = SOLCLIENT_PROP_ENABLE_VAL;
    txprops[pi++] = NULL;

    /* Congigure the Flow function information */
    ffninfo.rxMsgInfo.callback_p = on_flow_msg;
    ffninfo.rxMsgInfo.user_p     = NULL;
    ffninfo.eventInfo.callback_p = on_flow_evt;
    ffninfo.eventInfo.user_p     = NULL;

    /* Configure the Flow properties */
    pi = 0;
    fprops[pi++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    fprops[pi++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;
    fprops[pi++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
    fprops[pi++] = qname;
    fprops[pi++] = NULL;

    for(i = 0; i < svccount; i++) {
        /* Create the TX-session */
        if ((rc = solClient_session_createTransactedSession(txprops, 
                                                        sess,
                                                        &(svc_txsess[i]),
                                                        NULL)) != SOLCLIENT_OK ) {
            on_err ( rc, "solClient_session_createTransactedSession()" );
            solClient_cleanup();
            exit( 1 );
        }
        /* Bind the service listener flow on that TX-session */
        if ((rc = solClient_transactedSession_createFlow ( fprops, 
                                                        svc_txsess[i], 
                                                        &(svc_flow[i]), 
                                                        &ffninfo, 
                                                        sizeof(ffninfo))) != SOLCLIENT_OK ) {
            on_err(rc, "solClient_transactedSession_createFlow() one ");
            solClient_cleanup();
            exit( 1 );
        }
    }

    /*************************************************************************
     * Sleep repeatedly allowing bg-threads to hand events
     *************************************************************************/

    while(1) {
        sleep( 2 );
    }

    /************* Cleanup *************/
    /* Cleanup all transacted sessions */
    for(i = 0; i < svccount; i++) {
        if (svc_txsess[i] != NULL ) {
            if ( ( rc = solClient_transactedSession_destroy( &(svc_txsess[i]) ) ) != SOLCLIENT_OK ) {
            on_err ( rc, "solClient_transactedSession_destroy()" );
            }
        }
    }
    /* Disconnect the Session */
    if ( ( rc = solClient_session_disconnect ( sess ) ) != SOLCLIENT_OK ) {
        on_err ( rc, "solClient_session_disconnect()" );
    }
    /* Cleanup */
    if ( ( rc = solClient_cleanup (  ) ) != SOLCLIENT_OK ) {
        on_err ( rc, "solClient_cleanup()" );
    }
    goto notInitialized;

  notInitialized:
    return 0;
}  
