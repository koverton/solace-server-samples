/**
 * Example application accessing Solace message serialized buffer for full message 
 * archive / republish capability.
 *
 * Can be useful for moving messages from one queue to another, across brokers or VPNs 
 * with native performance.
 **/
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>

#include "msgbuf.hpp"
#include "connector.hpp"

void 
usage(const char* progname) {
	std::cerr << std::endl 
		<< "USAGE: " << progname << " {connection-props-file} "
		<< "{-w <writefile>} {-s source} OR {-r <readfile> {opt:-d destination} }" << std::endl << std::endl
		<< "\t{connection-props-file} file with Solace session properties for connecting" << std::endl
		<< "\t--" << std::endl
		<< "\t{writefile}   : file on localdisk to which messages will be archived" << std::endl
		<< "\t{source     } : source of consumed messages, e.g. 'queue:myqueuename' OR 'topic:my/topic/name'" << std::endl
		<< "\t--" << std::endl
		<< "\t{readfile}    : file on localdisk fromwhich messages will be read and republished" << std::endl
		<< "\t{destination} : alternate destination to republish messages, e.g. 'queue:myqueuename' OR 'topic:my/topic/name'" << std::endl
		<< std::endl;
	exit( 0 );
}

/**
 * The archiver is a file reader/writer for archiving events from/to 
 * the Solace event bus.
 **/
typedef struct archstate 
{
	int        count_;
	std::ofstream wr_;
	std::ifstream rd_;
} archstate;

/**
 * Primary event handler for messages from the Solace EventBroker.
 * This will static-cast the attached userdata to `archstate*` 
 * and invoke the handler for messages on that state object.
 **/
bool
on_queue_msg ( solClient_opaqueFlow_pt flow_p, solClient_opaqueMsg_pt msg_p, void* user_p )
{
	archstate* archiver = (archstate*) user_p;
	// Extract the serialized SMF Buffer from the message for archiving
	solClient_bufInfo_t smfbuf;
	if ( solClient_msg_getSmfPtr ( msg_p, ( solClient_uint8_t ** ) & smfbuf.buf_p, &smfbuf.bufSize ) != SOLCLIENT_OK ) {
		std::cout << "Crap! Unable to extract SMF from received message" << std::endl;
		return false;
	}
	// Wrap the SMF Buffer into a `msgbuf`
	msgbuf buf = { smfbuf.bufSize, (unsigned char*)smfbuf.buf_p };
	// Write the buffer out to the archive file
	archiver->count_ = put( archiver->wr_, buf, archiver->count_ );
	return true;
}

// Reads cmdline arg destination type
solClient_destinationType 
parsedesttype(const std::string& destparam) {
	if ( std::string::npos != destparam.find("queue:") )
		return SOLCLIENT_QUEUE_DESTINATION;
	return SOLCLIENT_TOPIC_DESTINATION;
}
// Reads cmdline arg destination name
std::string 
parsedest(const std::string& destparam ) {
	if ( std::string::npos != destparam.find("queue:") || 
	     std::string::npos != destparam.find("topic:") ) {
		return destparam.substr(6).c_str();
	}
	return destparam;
}

/**
 * MAIN APPLICATION LOOP
 **/
int
main ( int c, char** a )
{
	if( c < 4 ) {
		usage( a[0] );
	}
	const std::string pfile( a[1] );
	const std::string flag ( a[2] );
	const std::string fname( a[3] );
	if ( c > 4 && c != 6 ) {
		usage( a[0] );
	}

	archstate archiver = { 0 };
	connstate conn = init( );
	connect( conn, pfile );
	// Read from archive file and republish
	if( flag == "-r" || flag == "-R" ) {
		// set any alternate destination when chosen
		solClient_destination_t dest = { SOLCLIENT_NULL_DESTINATION , NULL };
		std::string destname;
		if ( c > 4 ) {
			std::cerr << "Setting dest: " << a[5] << std::endl;
			destname = parsedest( a[5] );
			dest.destType = parsedesttype( destname );
			dest.dest = destname.c_str();
		}
		else {
			std::cerr << "Dest.dest: " << (NULL == dest.dest ? "(null)":dest.dest) << std::endl;
		}
		// Read from the archive and republishing
		archiver.count_ = startread( fname, archiver.rd_ );
		for( int i = 0; i < archiver.count_; ++i ) {
			// Get the next message from the archive
			msgbuf rbuf = get( archiver.rd_ );
			// Wrap msgbuf into an SMF buffer 
			solClient_bufInfo smfbuf = { rbuf.buf_, rbuf.size_ };
			// Wrap the SMF Buffer into a message for sending
			solClient_opaqueMsg_pt msg_p;
			solClient_msg_decodeFromSmf( &smfbuf, &msg_p );
			// Set optional destination when specified
			if ( dest.destType != SOLCLIENT_NULL_DESTINATION )  {
				std::cerr << "Setting dest to: " << dest.dest << std::endl;
				solClient_msg_setDestination( msg_p, &dest, sizeof(dest) );
			}
			// Replublish the message
			//solClient_msg_dump ( msg_p, NULL, 0 );
			sendmsg( conn, msg_p );
			cleanmsgstore( conn );
		}
	}
	// Write to archive as we consume from a queue
	else if( flag == "-w" || flag == "-W" ) {
		// set any alternate destination when chosen
		solClient_destination_t source = { SOLCLIENT_NULL_DESTINATION , NULL };
		std::string srcname;
		if ( c > 4 ) {
			std::cerr << "Setting source: " << a[5] << std::endl;
			srcname = parsedest( a[5] );
			source.destType = parsedesttype( a[5] );
			source.dest     = srcname.c_str();
			std::cout << "SRC: " << source.dest << " TYPE: " << source.destType << std::endl;
			if ( source.destType != SOLCLIENT_QUEUE_DESTINATION ) {
				std::cerr << "SORRY! Source must be a queue; cannot yet consume from topics." << std::endl;
				exit( 0 );
			}
		}
		else {
			std::cerr << "Source.dest: " << (NULL == source.dest ? "(null)":source.dest) << std::endl;
		}
		// Open the archive for writing
		startwrite( fname, archiver.wr_ );
		// Open the queue for consumption; the callback `on_queue_msg` 
		// will archive each message upon arrival
		openqueue( conn, source.dest, on_queue_msg, &archiver );
		// Nothing else to do, so give the app thread back to the scheduler
		while( true ) { 
			sleep( 5 );
			std::cerr << " ... written " << archiver.count_ << " ... " << std::endl;
		}
	}
	// Help message
	else {
		usage( a[0] );
	}
	return 0;
}
