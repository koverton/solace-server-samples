#ifndef MSGBUF_HPP
#define MSGBUF_HPP

#include <iostream>
#include <fstream>
#include <string>

/***
 * A message log is a binary log of serialized Solace messages including all 
 * attributes of each message. The structure of each file is:
 * 
 * [ integer: number of records in the file ]
 * [ record1: 
 * 		[integer: size of the serialized message data]
 * 		[binary buffer of serialized message data]
 * ]
 * [ record2: [int] [binary data] ]
 * ...
 * [ recordn: [int] [binary data] ]
 ***/

/**
 * Individual record.
 **/
typedef struct msgbuf {
	unsigned int   size_;
	unsigned char* buf_;
} msgbuf;

void show(msgbuf& m) {
	std::cout << "sz: " << m.size_ << " buf: " << (char*)m.buf_ << std::endl;
}


/**
 * WRITING: typical writer
 * main() {
 *		archstate archiver = { 0 };
 *		startwrite( fname, archiver.wr_ );
 *		// start Solace listener ...
 * }
 * on_msg() {
 *		msgbuf buf = { smfbuf.bufSize, smfbuf.buf_p };
 *		archiver.count_ = put( archiver.wr_, buf, archiver.count_ );
 * }
 **/ 

void puthdr(std::ofstream& wrf, int count) {
	wrf.seekp( 0, std::ios::beg );
	wrf.write( (char*)&count, sizeof(int) );
	if( !wrf ) std::cout << "Dammit wrf!" << std::endl;
	wrf.seekp( 0, std::ios::end );
}

int put(std::ofstream& wrf, msgbuf& buf, int index) {
	wrf.write( (char*)&buf.size_, sizeof(int) );
	if (!wrf) std::cout << "Dammit wrf! Can't put size: " << buf.size_ << std::endl;
	wrf.write( (char*)buf.buf_, buf.size_ );
	if (!wrf) std::cout << "Dammit wrf! Can't put buffer" << std::endl;
	++index;
	puthdr( wrf, index );
	return index;
}

void startwrite(const std::string& fname, std::ofstream& wrf) {
	wrf.open( fname, std::ios::out | std::ios::trunc | std::ios::binary );
	puthdr ( wrf, 0 );
}

void stopwrite(std::ofstream& wrf) {
	wrf.close();
}


/**
 * READING: typical reader replays from a file
 * main() {
 *		archstate archiver = { 0 };
 *		archiver.count_ = startread( fname, archiver.rd_ );
 *		for( int i = 0; i < archiver.count_; ++i ) {
 *			msgbuf rbuf = get( archiver.rd_ );
 *			// ... build output message ...
 *			sendmsg( conn, msg_p );
 *		}
 * }
 **/ 

int startread(const std::string& fname, std::ifstream& rdf) {
	rdf.open( fname, std::ios::in | std::ios::binary );
	rdf.seekg( 0, std::ios::beg );
	int howmany = 0;
	rdf.read( (char*)&howmany, sizeof(int) );
	if (!rdf) std::cout << "Dammit inf! Could'nt read the count " << howmany << std::endl;
	return howmany;
}

int gethdr(std::ifstream& rdf) {
	rdf.seekg( 0, std::ios::beg );
	int howmany = 0;
	rdf.read( (char*)&howmany, sizeof(int) );
	if (!rdf) std::cout << "Dammit inf! Could'nt read the count " << howmany << std::endl;
	rdf.seekg( 0, std::ios::end );
	return howmany;
}

msgbuf get(std::ifstream& rdf) {
	msgbuf result = { 1, 0 };
	rdf.read( (char*)&result.size_, sizeof(int) );
	if( !rdf ) std::cout << "Dammit rdf! Can't read the size." << std::endl;
	result.buf_ = new unsigned char[ result.size_ ];
	rdf.read( (char*)result.buf_, result.size_ );
	if( !rdf ) std::cout << "Dammit! Can't read buf." << std::endl;
	return result;
}

#endif // MSGBUF_HPP
