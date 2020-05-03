# solace-server-samples
C-API examples for common server scenarios.

## `ioloop`

Foreground server loop to drive the Solace event pump from the application thread. Requires no context thread but does require regular calling of `solClient_context_processEvents` in a loop. That allows applications to coordinate multiple sources of IO.

## `txservices`

Thread-pool for request/reply services. Demonstrates how Solace simplifies scaling service-listeners by allocating them across multiple threads and dispatching requests across the service listeners. This example also uses transacted sessions to atomically commit the message consumption with publication of the service result. With this model, requests that went to services that experienced failures are automatically freed and rerouted to other service listeners.

## `msgarchiver`

Basic example of program archiving the contents of a queue to a file, and reading the file to republish the messages. Can either listen for messages on topics/queues and archive them to files, or replay archive files back to the message bus. In archive replay mode, it can be configured to replay to the original message destinations or to a new destination.
