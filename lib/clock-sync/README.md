# Clock Synchronization library
These library reactors are inspired by the PTP protocol. Currently the sync reactors only have
one input and output, meaning that we need one per connection to remote federate. 
Connections between the slave and master should be physical.

Three messages are exchanged between Master and Slave:
1. First the master sends a SYNC message to the slave. The SYNC message contains the timestamp t1
2. The slave records the time of reception of SYNC as t2 (this will be the logical tag at which the reaction triggered by the SYNC message is executed.)
3. The sync responds with a DELAY_REQ message. The slave stores t3, the time of transmission.
4. The master records the time of receiving the DELAY_REQ as t4. Again this will be the logical tag
of the reaction triggered by the receiving event. The master sends this timestamp in a DELAY_RESP message sent back.

