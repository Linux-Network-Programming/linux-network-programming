# Linux Network Programming - The Definitive Guide

Leanpub (ebook): https://leanpub.com/linuxnetworkprogramming-thedefinitiveguide 

Google Play: https://play.google.com/store/books/details/Marco_Bruno_Linux_Network_Programming_The_Definiti?id=_tkyEgAAQBAJ 


<img src="Cover Front.png" />

Linux Network Programming – The Definitive Guide is a comprehensive technical reference and practical guide to network programming on Linux, written for developers, system programmers, network engineers, and advanced students who want to understand how network applications work, how communication protocols operate, and how Linux implements its networking facilities.

Starting with the foundations of computer networking and the TCP/IP protocol suite, this book progressively explores the Linux networking architecture, socket programming interfaces, transport protocols, and advanced networking mechanisms. It combines theoretical explanations with practical C programming, providing the knowledge required to design, implement, analyze, and optimize network applications on modern Linux systems.

The book goes beyond conventional socket programming tutorials. It examines the interaction between user-space applications, the Linux kernel, networking protocols, and underlying system interfaces, offering an in-depth understanding of both network programming concepts and their Linux-specific implementations.

# What You Will Learn

Networking Fundamentals: Understand Ethernet, ARP, IPv4, IPv6, ICMP, UDP, TCP, DNS, DHCP, and other essential networking protocols.

Linux Socket Programming: Master the Berkeley sockets API, address structures, socket options, connection management, and data transmission.

TCP Programming: Explore connection establishment and termination, flow control, congestion control, retransmission, TCP performance, and advanced TCP features.

UDP Programming: Develop datagram-based applications, implement reliable communication mechanisms, and explore real-time network applications, including audio streaming.

UNIX Domain Sockets: Understand local interprocess communication, credential passing, and file descriptor transfer.

Multiplexed I/O: Learn how to build scalable network applications using select(), poll(), epoll(), and other Linux I/O mechanisms.

High-Performance Networking: Explore techniques such as zero-copy transmission, sendfile(), busy polling, and MSG_ZEROCOPY.

Linux Networking Internals: Investigate network namespaces, virtual network interfaces, bridges, TUN/TAP devices, Netlink, Netfilter, nftables, eBPF, and traffic control.

Practical Network Application Development: Apply the concepts through complete architectural discussions and practical case studies involving network services, concurrent servers, reverse proxies, and peer-to-peer applications.

# A Practical and In-Depth Approach

Throughout the book, concepts are explained progressively, supported by extensive C programming examples and practical demonstrations on Linux. Rather than treating networking as a collection of API calls, the material explains the underlying mechanisms, system interfaces, protocol behavior, and design considerations that determine how network applications operate.

Readers are encouraged to experiment with the examples, inspect network traffic, investigate system calls, and explore the Linux networking environment using the tools and facilities available on modern Linux distributions.

# Who Should Read This Book?

This book is intended for:

Linux and UNIX system programmers.

C and C++ developers interested in network application development.

Network engineers seeking a deeper understanding of Linux networking.

Cybersecurity professionals and security researchers.

Computer science and engineering students.

Developers building high-performance, concurrent, and secure network services.

A working knowledge of the C programming language and basic Linux command-line operations is recommended.

# The Complete Reference

Linux Network Programming – The Definitive Guide provides an extensive treatment of network programming, from fundamental networking concepts to advanced Linux kernel networking facilities. It is designed to serve both as a structured learning resource and as a long-term technical reference for developers working with Linux networking.

Whether you are developing your first TCP server, implementing a sophisticated network service, investigating kernel networking mechanisms, or optimizing an existing application, this book provides the theoretical foundations and practical knowledge needed to understand and master Linux network programming.

# Table of contents
# Part I – Network Fundamentals
	I.1 Introduction to Computer Networks
		I.1.1 Network Architecture
		I.1.2 The OSI Model
		I.1.3 The TCP/IP Model
		I.1.4 Packets, Frames and Encapsulation
		I.1.5 Addressing: MAC, IP, Ports and Sockets
		I.1.6 Connection-Oriented and Connectionless Communication
		I.1.7 Well-Known Ports and Services
		I.1.8 Network Byte Order:  htons, htonl, ntohs, ntohl
		I.1.9 Routing Basics
		I.1.10 Firewall Basics
		I.1.11 The /proc/net Filesystem
	I.2 System Information
		I.2.1 Text and Binary IP Address Representation: inet_ntop, inet_pton
		I.2.2 Converting Network Prefixes: inet_net_ntop, inet_net_pton
		I.2.3 Retrieving Network Interface Information: ioctl and getifaddrs
		I.2.4 Network Interfaces: Names and Indexes
		I.2.5 Getting and Setting Hostname and Domain Name
		I.2.6 Retrieving the Routing Table
		I.2.7 Retrieving the ARP Cache 
	I.3 Useful Functions and System Interfaces
		I.3.1 Structure and Memory Operations: offsetof, sizeof, memset, memcpy 
		I.3.2 Time and Timing: time, clock_gettime, nanosleep
		I.3.3 Process Management: wait, waitpid, waitid
		I.3.4 Host and Endian Conversion: htobe*, htole*, be*toh, le*toh

# Part II – The Protocols
	II.1 Ethernet
		II.1.1 The Ethernet Frame Format
		II.1.2 Wireless LANs – 802.11
	II.2 Address Resolution Protocol (ARP)
		II.2.1 ARP Frame Format
		II.2.2 Proxy ARP and ARP Poisoning
		II.2.3 Implementing an ARP Client
		II.2.4 ARP Packet Construction
	II.3 Internet Protocol Version 4 (IPv4)
		II.3.1 IPv4 Addressing and Address Classes
		II.3.2 Subnetting
		II.3.3 Broadcast, Special, Multicast and Anycast Addresses
		II.3.4 IPv4 Header
		II.3.5 Constructing and Sending an IPv4 Datagram Using Raw Sockets
		II.3.6 Calculating the Checksum
		II.3.7 IPv4 Fragmentation	
		II.3.8 Constructing IPv4 Fragments Using Raw Sockets
	II.4 Internet Protocol Version 6 (IPv6)
		II.4.1 IPv6 Address Representation
		II.4.2 IPv6 Prefix Length and Subnetting
		II.4.3 IPv6 Header
		II.4.4 IPv6 Extension Headers
		II.4.5 Constructing and Sending an IPv6 Datagram Using Raw Sockets
	II.5 Internet Control Message Protocol (ICMP)
		II.5.1 ICMP Message Format
		II.5.2 ICMPv4 Messages
		II.5.3 ICMPv6 Messages
		II.5.4 Linux ICMP Interface 
		II.5.5 Implementing a Ping Program Using Raw Sockets
		II.5.6 Implementing a Traceroute Program Using Raw Sockets
	II.6 User Datagram Protocol (UDP)
	 	II.6.1 UDP Header
	 	II.6.2 UDP Encapsulation
	 	II.6.3 Linux Implementation
		II.6.4 Lightweight User Datagram Protocol (UDP-Lite)
	II.7 Domain Name System (DNS)
		II.7.1 Overview
		II.7.2 Hostnames and DNS
		II.7.3 DNS Header
		II.7.4 DNS Message Format
		II.7.5 Resource Records
		II.7.6 DNS Names and Compression
		II.7.7 DNS Encapsulation
		II.7.8 Address Resolution with getaddrinfo
		II.7.9 Reverse Lookup with getnameinfo
		II.7.10 DNS Zone Transfer
		II.7.11 Domain Name System Security Extensions (DNSSEC)
		II.7.12 BIND
	II.8 Dynamic Host Configuration Protocol (DHCP)
		II.8.1 Overview
		II.8.2 DHCP Message Format 
		II.8.3 DHCP Message Types
		II.8.4 DHCP Options
		II.8.5 DHCP Message Exchange 
		II.8.6 Implementing a DHCP Client
		II.8.7 Constructing the DHCPDISCOVER
		II.8.8 Processing the DHCPOFFER
		II.8.9 Sending the DHCPREQUEST
		II.8.10 Receiving the DHCPACK
	II.9 Transmission Control Protocol (TCP)
		II.9.1 TCP Header
		II.9.2 TCP Encapsulation
		II.9.3 Linux Implementation 
		II.9.4 TCP Echo Client
		II.9.5 TCP Echo Server

# Part III – Socket APIs
	III.1 Introduction to Sockets
		III.1.1 Socket Abstraction
		III.1.2 Socket Domains and Types
		III.1.3	Socket Addresses 
	III.2 Socket Addressing
		III.2.1 The sockaddr Structure
		III.2.2 IPv4 Socket Addresses: sockaddr_in
		III.2.3 IPv6 Socket Addresses: sockaddr_in6
		III.2.4 UNIX Domain Socket Addresses: sockaddr_un
		III.2.5 Link-Layer Socket Addresses: sockaddr_ll
	III.3 Creating and Closing Sockets
		III.3.1 Creating Sockets: socket
		III.3.2 Shutting Down Socket Communication: shutdown
		III.3.3 Closing Sockets: close
	III.4 Binding a Socket: bind
	III.5 Listening and Accepting Connections 
		III.5.1 Listening for Connections: listen 
		III.5.2 Accepting Connections: accept 
	III.6 Connecting a Socket: connect 
	III.7 Sending and Receiving Data 
		III.7.1 Sending Data: send, sendto and sendmsg 
		III.7.2 Receiving Data: recv, recvfrom and recvmsg		
		III.7.3 Sending and Receiving Multiple Messages: sendmmsg and recvmmsg 
		III.7.4 Out-of-Band Data: sockatmark
	III.8 Socket Information
		III.8.1 Retrieving Socket Addresses: getsockname and getpeername
      III.9 Socket Configuration
          III.9.1 Socket Options Overview
  III.9.2 Retrieving and Setting Socket Options: getsockopt and setsockopt
	
# Part IV – TCP Programming
	IV.1 TCP Overview
		IV.1.1 TCP Connection Establishment 
		IV.1.2 TCP Connection States 
		IV.1.3 Transmission Control Block (TCB)
		IV.1.4 Sequence Numbers
		IV.1.5 Maximum Segment Size (MSS)
		IV.1.6 Flow Control 
		IV.1.7 Retransmission and Timers
		IV.1.8 Congestion Control
		IV.1.9 TCP Connection Termination
		IV.1.10 TCP Connection Reset
		IV.1.11 TCP Error Handling
		IV.1.12 Linux TCP Interface
	IV.2 TCP with Netcat 
		IV.2.1 TCP Client and Server with Netcat 
		IV.2.2 Observing TCP Packets 
	IV.3 TCP Clients
		IV.3.1 TCP Client Architecture
		IV.3.2 TCP Client Lifecycle
		IV.3.3 Error Handling
	IV.4 TCP Servers
		IV.4.1 TCP Server Architecture
		IV.4.2 TCP Server Lifecycle
		IV.4.3 Error Handling
	IV.5 Concurrent Servers
		IV.5.1 Process-per-Connection Servers
		IV.5.2 Thread-per-Connection Servers
		IV.5.3 Preforked Servers
		IV.5.4 Thread Pools
	IV.6 Advanced TCP Topics
		IV.6.1 Partial Reads and Writes
		IV.6.2 Framing Application Protocols: Overview
		IV.6.3 Length-Prefixed Framing
		IV.6.4 Delimiter-Based Framing
		IV.6.5 TCP Socket Options
		IV.6.6 TCP Keepalive
          IV.6.7 Partially Filled TCP Segments and TCP_CORK
		IV.6.8 TCP Quick ACK
		IV.6.9 TCP User Timeout
		IV.6.10 Nagle's Algorithm
 	IV.7 TCP Fast Open
		IV.7.1 Overview 
		IV.7.2 TCP Connection and Cookie
		IV.7.3 Linux Implementation
          IV.7.4 Using TCP_FASTOPEN and MSG_FASTOPEN
          IV.7.5 Enabling Fast Open with TCP_FASTOPEN_CONNECT
          IV.7.6 Application Data in the SYN+ACK
          IV.7.7 A FreeBSD Implementation
	IV.8 TCP Applications
		IV.8.1 Implementing a TCP Chat Application
		IV.8.2 Implementing a TCP File Transfer Application
		IV.8.3 Observing the TCP Byte Stream

# Part V – UDP Programming
	V.1 Datagram Sockets
		V.1.1 Linux Implementation
		V.1.2 Sending Datagrams
		V.1.3 Receiving Datagrams
	V.2 Advanced UDP
		V.2.1 Connected UDP Sockets
		V.2.2 Broadcast Communication
		V.2.3 Implementing Host Discovery with UDP Broadcast
		V.2.4 Multicast Communication: Overview
		V.2.5 Multicast Communication: Linux Implementation
		V.2.6 Implementing a Multicast Chat Application
		V.2.7 Internet Group Management Protocol Version 2 (IGMPv2)
		V.2.8 Internet Group Management Protocol Version 3 (IGMPv3)
	V.3 Reliable Communication over UDP
		V.3.1 Acknowledgment Mechanisms
		V.3.2 Retransmission Strategies
		V.3.3 Application-Level Reliability
		V.3.4 Implementing a Reliable File Transfer Application over UDP: Server
		V.3.5 Implementing a Reliable File Transfer Application over UDP: Client
		V.3.6 UDP Application Packet Analysis with tcpdump
	V.4 DNS Programming
    		V.4.1 DNS Queries over UDP
    		V.4.2 Implementing a DNS Client
    		V.4.3 Parsing DNS Responses
		V.4.4 Analyzing DNS Responses with tcpdump
    		V.4.5 DNS over TCP
		V.4.6 Linux Interface: resolver
		V.4.7 Implementing a DNS Client Using libresolv
	V.5 Implementing a UDP Audio Streaming Server

# Part VI – UNIX Domain Sockets
	VI.1 Local IPC with Sockets
		VI.1.1 UNIX Domain Sockets Overview
		VI.1.2 Creating Connected Socket Pairs: socketpair
		VI.1.3 Sequenced-Packet Sockets: SOCK_SEQPACKET
	VI.2 Advanced UNIX Domain Sockets
		VI.2.1 Abstract Sockets
		VI.2.2 Ancillary Messages: cmsghdr
		VI.2.3 Credential Passing: SCM_CREDENTIALS
		VI.2.4 Passing File Descriptors: SCM_RIGHTS
	VI.3 Practical Applications
		VI.3.1 Sharing a TCP Connection Between Processes
		VI.3.2 Passing File Descriptors Between Processes
		VI.3.3 Building a Socket-Based Logging Service
		VI.3.4 Interprocess Communication Patterns

# Part VII – Multiplexed I/O
	VII.1 Multiplexed Networking
		VII.1.1 Blocking and Nonblocking I/O
		VII.1.2 Event-Driven Architectures
	VII.2 select
		VII.2.1 Monitoring File Descriptors with select
	VII.3 poll
		VII.3.1 Monitoring File Descriptors with poll
	VII.4 epoll
		VII.4.1 Creating an epoll Instance: epoll_create1
		VII.4.2 Registering File Descriptors: epoll_ctl
		VII.4.3 Waiting for Events: epoll_wait and epoll_pwait
		VII.4.4 Implementing an epoll-Based Server
		VII.4.5 Level-Triggered Mode
		VII.4.6 Edge-Triggered Mode

# Part VIII – Asynchronous I/O and Networking
	VIII.1 Asynchronous Networking
		VIII.1.1 Event-Driven and Completion-Based Networking
	VIII.2 Networking with io_uring
 		VIII.2.1 Overview
		VIII.2.2 Asynchronous Accept: io_uring_prep_accept
		VIII.2.3 Submitting Requests: io_uring_submit 
		VIII.2.4 Queue Operations	
		VIII.2.5 A Simple Asynchronous Server with io_uring
		VIII.2.6 Asynchronous Connect: io_uring_prep_connect
		VIII.2.7 Asynchronous Send: io_uring_prep_send
		VIII.2.8 Asynchronous Receive:  io_uring_prep_recv
 		VIII.2.9 Handling Multiple Clients Asynchronously
 		VIII.2.10 Maintaining a Pool of Outstanding Accept Requests
	VIII.3 Buffer Rings
		VIII.3.1 Overview 
		VIII.3.2 Buffer IDs and Buffer Group IDs
		VIII.3.3 Registering and Initializing Buffer Rings: io_uring_setup_buf_ring 			  and io_uring_buf_ring_init
		VIII.3.4 Adding Buffers: io_uring_buf_ring_add and 						  io_uring_buf_ring_advance
		VIII.3.5 Releasing Buffer Rings: io_uring_free_buf_ring
	VIII.4 Multishot Operations
		VIII.4.1 Overview
		VIII.4.2 Multishot Accept: io_uring_prep_multishot_accept
		VIII.4.3 Multishot Receive: io_uring_prep_recv_multishot
		VIII.4.4 Implementing a Multishot Server
		VIII.4.5 Choosing Between epoll and io_uring
	VIII.5 Zero-Copy Networking with io_uring
		VIII.5.1 Overview 
		VIII.5.2 Zero-Copy Send: io_uring_prep_send_zc
		VIII.5.3 Handling Notifications 
		VIII.5.4 Implementing a Zero-Copy TCP Server

# Part IX – High-Performance Networking
	IX.1 Zero-Copy Networking
		IX.1.1 sendfile
		IX.1.2 Sending a File Over TCP with sendfile
		IX.1.3 splice
		IX.1.4 Implementing a TCP Bridge with splice
		IX.1.5 vmsplice
		IX.1.6 tee
		IX.1.7 Serving Multiple Clients with tee
		IX.1.8 Serving Multiple Clients Concurrently with tee
	IX.2 Batched Networking Operations
		IX.2.1 sendmmsg
		IX.2.2 recvmmsg
		IX.2.3 Implementing a Server with sendmmsg
		IX.2.4 Implementing a Client with recvmmsg
		IX.2.5 Tracing Batched Message Operations with strace
	IX.3 Modern Socket Optimizations
		IX.3.1 The SO_REUSEPORT Socket Option
		IX.3.2 Sharing a Port Among Multiple Sockets with SO_REUSEPORT
		IX.3.3 Busy Polling
		IX.3.4 Implementing a Busy-Polling UDP Receiver
		IX.3.5 Socket Buffer Tuning
		IX.3.6 Zero-Copy Socket Transmission: MSG_ZEROCOPY

# Part X — Application Layer Protocols
	X.1 Email Protocols 
		X.1.1 Simple Mail Transfer Protocol (SMTP)
		X.1.2 SMTP Servers on Linux 
		X.1.3 The mail Command
		X.1.4 Post Office Protocol Version 3 (POP3)
		X.1.5 Internet Message Access Protocol (IMAP)
		X.1.6 Email Security and TLS
	X.2 File Transfer Protocols
		X.2.1 File Transfer Protocol (FTP)
		X.2.2 FTP Clients and Servers on Linux
		X.2.3 FTP Over TLS (FTPS)
		X.2.4 Trivial File Transfer Protocol (TFTP)
	X.3 HTTP and Web Communication
		X.3.1 HTTP/1.1
		X.3.2 HTTP/2
		X.3.3 HTTP/3
		X.3.4 WebSocket
	X.4 Message Queuing Telemetry Transport (MQTT)
	X.5 Real-Time Communication
		X.5.1 Session Initiation Protocol (SIP)
		X.5.2 Real-Time Transport Protocol (RTP) and RTP Control Protocol (RTCP)
		X.5.3 WebRTC

# Part XI – Linux Networking Internals
	XI.1 Linux Namespaces
		XI.1.1 Overview
		XI.1.2 Network Namespace
		XI.1.3 Process Execution Context
		XI.1.4 Creating a Network Namespace Using unshare
		XI.1.5 Entering a Network Namespace Using setns
		XI.1.6 Named and Unnamed Network Namespaces
		XI.1.7 Applications of Network Namespaces
	XI.2 Virtual Network Interfaces
		XI.2.1 Virtual Ethernet Devices
		XI.2.2 Bridges
		XI.2.3 TUN interfaces
		XI.2.4 TAP interfaces
	XI.3 Netlink Sockets
 		XI.3.1 Netlink Fundamentals
		XI.3.2 Querying Networking Information: rtnetlink
		XI.3.3 Retrieving Network Interface Information: RTM_GETLINK
		XI.3.4 Retrieving IP Address Information: RTM_GETADDR
		XI.3.5 Retrieving Routing Table Entries: RTM_GETROUTE
		XI.3.6 Retrieving Neighbor Table Entries: RTM_GETNEIGH
		XI.3.7 Configuring Network Interfaces: RTM_NEWLINK and RTM_SETLINK
		XI.3.8 Configuring IP Addresses: RTM_NEWADDR and RTM_DELADDR
		XI.3.9 Configuring Routes: RTM_NEWROUTE and RTM_DELROUTE
		XI.3.10 Configuring Neighbor Entries: RTM_NEWNEIGH and RTM_DELNEIGH
	XI.4 Socket Diagnostics with sock_diag
		XI.4.1 Overview
		XI.4.2 IPv4 and IPv6 Sockets: inet_diag.h
		XI.4.3 Retrieving TCP Socket Information 
		XI.4.4 Retrieving UDP Socket Information
		XI.4.5 UNIX Domain Sockets: unix_diag.h
		XI.4.6 Retrieving UNIX Domain Socket Information
	XI.5 Raw and Packet Sockets
		XI.5.1 Raw Sockets: SOCK_RAW
		XI.5.2 Packet Sockets: AF_PACKET
		XI.5.3 Implementing an Ethernet Packet Sniffer with AF_PACKET
		XI.5.4 Packet Socket Options
	XI.6 Linux Firewalling
		XI.6.1 Netfilter Architecture
		XI.6.2 Connection Tracking: conntrack
		XI.6.3 nftables Fundamentals
		XI.6.4 The Netfilter Netlink Interface: NETLINK_NETFILTER
		XI.6.5 Programming the nftables Interface with Netlink
	XI.7 eBPF
		XI.7.1 Introduction to eBPF
		XI.7.2 Attaching an eBPF Program to a Kernel Tracepoint with libbpf
		XI.7.3 The bpf System Call
	XI.8 eBPF in Linux Networking
		XI.8.1 eXpress Data Path (XDP)
		XI.8.2 Redirecting Packets to User Space: AF_XDP
		XI.8.3 Traffic Control with tc and eBPF
		XI.8.4 The Netlink Interface to Traffic Control
		XI.8.5 Creating a clsact Queueing Discipline: RTM_NEWQDISC
		XI.8.6 Attaching Traffic Control Filters: RTM_NEWTFILTER

# Part XII – Case Studies
	XII.1 Implementing a File Sharing Server
	XII.2 Implementing a Concurrent Chat Server with Multiplexed I/O
	XII.3 Implementing a Concurrent TCP Reverse Proxy
		XII.3.1 TCP Reverse Proxy with epoll
		XII.3.2 TCP Reverse Proxy with Pipes and splice
		XII.3.3 Performance Improvement Exercise
	XII.4 Implementing a P2P Video Call Application



