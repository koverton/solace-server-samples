#include "msgbuf.hpp"

int main (int c, const char** a) {
	const char* fname = "hello";
	msgbuf start = { strlen(fname)+1, (unsigned char*)fname };

	show( start );

	std::ofstream wrf;
	startwrite( fname, wrf );
	int count = 0;

	count = put( wrf, start, count );
	count = put( wrf, start, count );
	count = put( wrf, start, count );
	count = put( wrf, start, count );

	stopwrite( wrf );
	std::cout << "Done writing , now opening for read." << std::endl;

	std::ifstream inf;
	int howmany = startread( fname, inf );
	if (howmany != count) {
		std::cout << "Dammit! The count in the header doesn't match what we wrote!" << std::endl;
		std::cout << "Wrote: " << count << " Read: " << howmany << std::endl;
	}

	std::cout << "How many? " << howmany << std::endl;
	for( int i = 0; i < howmany; i++) {
		msgbuf result = get( inf );
		show( result );
	}
	inf.close();

	return 0;
}

