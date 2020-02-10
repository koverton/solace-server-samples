# msgarchiver

An example program reading messages from Solace and writing them to disk.

The same program can read the files from disk and republish them to Solace.

TBD: Add ability to modify replay destination to support queue-to-queue copies.

## File Format

A message log is a binary log of serialized Solace messages including all 
attributes of each message. The structure of each file is:

``` 
 [ integer: number of records in the file ]
 [ record1: 
 		[integer: size of the serialized message data]
 		[binary buffer of serialized message data]
 ]
 [ record2: [int] [binary data] ]
 ...
 [ recordn: [int] [binary data] ]
```


##  READING: typical reader replays from a file
```C
 main() {
	archstate archiver = { 0 };
	archiver.count_ = startread( fname, archiver.rd_ );
	for( int i = 0; i < archiver.count_; ++i ) {
		msgbuf rbuf = get( archiver.rd_ );
		// ... build output message ...
		sendmsg( conn, msg_p );
	}
 }
```


##  WRITING: typical writer
```C
main() {
	archstate archiver = { 0 };
	startwrite( fname, archiver.wr_ );
	// start Solace listener ...
}
on_msg() {
	msgbuf buf = { smfbuf.bufSize, smfbuf.buf_p };
	archiver.count_ = put( archiver.wr_, buf, archiver.count_ );
}
```

