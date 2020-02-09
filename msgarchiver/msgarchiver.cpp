#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>

#include "msgbuf.hpp"
#include "connector.hpp"

void usage(const char* progname) {
	std::cerr << std::endl 
			<< "USAGE: " << progname 
			<< "{-r <readfile>} OR {-w <writefile>}" << std::endl;
	exit( 0 );
}

/**
 * The archiver is a file reader/writer for archiving events from/to 
 * the Solace event bus.
 **/
typedef struct archstate {
	int count_;
	std::ofstream wr_;
	std::ifstream rd_;
} archstate;

void
on_queue_msg ( solClient_opaqueFlow_pt flow_p, solClient_opaqueMsg_pt msg_p, void* user_p )
{
	archstate* archiver = (archstate*) user_p;
	std::cout << "Got our own msg-callback " << archiver->count_ << std::endl;

	solClient_bufInfo_t smfbuf;
    if ( solClient_msg_getSmfPtr ( msg_p, ( solClient_uint8_t ** ) & smfbuf.buf_p, &smfbuf.bufSize ) != SOLCLIENT_OK ) {
        std::cout << "Crap! Unable to extract SMF from received message" << std::endl;
        return;
    }

	msgbuf buf = { smfbuf.bufSize, (unsigned char*)smfbuf.buf_p };
	archiver->count_ = put( archiver->wr_, buf, archiver->count_ );
}

int
main ( int c, char** a )
{
	if( c != 3 ) {
		usage( a[0] );
	}
	const std::string flag ( a[1] );
	const std::string fname( a[2] );

	archstate archiver = { 0 };
	connstate conn = init( );
	connect( conn, "localhost", "default", "default", "default" );

	if( flag == "-r" || flag == "-R" ) {
		// Reading from the archive and republishing
		archiver.count_ = startread( fname, archiver.rd_ );
		for( int i = 0; i < archiver.count_; ++i ) {
			msgbuf rbuf = get( archiver.rd_ );
			solClient_bufInfo smfbuf;
			smfbuf.buf_p = rbuf.buf_;
			smfbuf.bufSize = rbuf.size_;
			solClient_opaqueMsg_pt msg_p;
			solClient_msg_decodeFromSmf( &smfbuf, &msg_p );
			solClient_msg_dump ( msg_p, NULL, 0 );
			sendmsg( conn, msg_p );
		}
	}
	else if( flag == "-w" || flag == "-W" ) {
		// Writing to the archive from a queue
		startwrite( fname, archiver.wr_ );
		// TODO: Would much prefer to template this to the archiver type
		openqueue( conn, "archive", on_queue_msg, &archiver );

		while( true ) sleep( 10 );
	}
	else {
		usage( a[0] );
	}
	return 0;
}

