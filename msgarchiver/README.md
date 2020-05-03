# msgarchiver

An example program reading messages from Solace and writing them to disk.

The same program can read the files from disk and republish them to Solace, either to their original destinations or to a configured destination.

## File Format

A message log is a binary log of serialized Solace messages including all 
attributes of each message. The structure of each file is:

![file structure](msgarchiver.png)

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

## Example program

The `msgarchiver` executable can run either as a listener archiving messages, or 
a reader playing back messages from the archive.

```bash
merzbow:msgarchiver koverton$ ./msgarchiver

USAGE: ./msgarchiver {connection-props-file} {-w <writefile> -s source} OR {-r <readfile> {opt:-d destination} }

	{connection-props-file} file with Solace session properties for connecting
	--
	{writefile}   : file on localdisk to which messages will be archived
	{source     } : source of consumed messages, e.g. 'queue:myqueuename'
	--
	{readfile}    : file on localdisk fromwhich messages will be read and republished
	{destination} : alternate destination to republish messages, e.g. 'queue:myqueuename' OR 'topic:my/topic/name'
```

### Writing Mode

In this mode, the msgarchiver listens to the Solace PubSub+ event broker for events on any configured destination queue and appends them to an archive file for later use:

```shell
./msgarchiver docker.properties -w mynhlarchive.dat -s 'queue:nhl/game/415'
Setting source: queue:nhl/game/415
SRC: nhl/game/415 TYPE: 1
 ... written 0 ...
 ... written 22617 ...
 ... written 105051 ...
 ... written 192329 ...
 ... written 275651 ...
 ... written 370890 ...
 ... written 413818 ...
 ...
```

### Reading Mode

In this mode, the msgarchiver reads from an archive file and republishes to a Solace PubSub+ event broker. This can be used to republish events to different destinations, or move events from one broker to another (or both).

```bash
./msgarchiver docker.properties -r mynhlarchive.dat -d queue:nhl/game/415
writing messages tnhl/game/415 ...
...
```

