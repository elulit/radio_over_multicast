# radio_over_multicast
virtualization of radio application over multicast.

I run this project with GNS3 simulation, although it can run on any platform using c.

You need to run "make" on each of the hosts (1 server and numerous "listener" and "controller").
You need to run the "server" first with arguments ./radio_server 4444 224.10.10.10.10 5001
You need to run the "listener" first with arguments ./radio_listener  224.10.10.10 5001
You need to run the "controller" first with arguments ./radio_controller 132.72.106.236 4444

with the port 5001 is the UDP port for the multicast and 4444 is the TCP for the queries(both can be chosen different).
