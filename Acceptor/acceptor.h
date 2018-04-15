#ifndef ACCEPTOR_H
#define ACCEPTOR_H

#include <queue>
#include <mutex>
#include <atomic>
#include <functional>
#include <condition_variable>

// winsock stuff
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
// end of winsock stuff

class Acceptor {
	SOCKET lsock; // socket for listening
	std::queue<SOCKET> accepted; // sockets for connections that have been accepted
	std::mutex accepted_mutex; // protection for the accepted queue
	std::condition_variable accepted_cv; // notifications for threads waiting on the accepted que
	std::function<void()> accept_event; // event to be run on connection accept
	std::atomic<bool> quit; // true if the thread associated with this should quit
	std::atomic<bool> resumed; // true if listening
	std::mutex resumed_mutex; // associated with resuemd atomic for condition_variable
	std::mutex thread_mutex; // this will remain locked as long as the worker thread is active
	std::condition_variable resumed_cv; // notification for resumed bool changes
	// port for connection listening and max possible pending connections
	int port; int max_listen;
	int listen_status; // result of attempt to listen on a socket
	std::mutex listen_mutex; // used to protect listen_status and wait for return on listen_status change
	// used for event-driven accept of connection and to notify worker while on wait
	WSAOVERLAPPED overlapped;
	// to be run by worker thread meant for listening
	void worker();
public:
	Acceptor();
	~Acceptor();
	void attach_event(const std::function<void()>& accept_event); // change event to be run on connection accept
	int start_listen(int port, int max_listen); // start accepting connections on a port and only allow these number of pending clients
	int start_listen(); // start listening with preset port and max_lsten
	void stop_listen(); // stop accepting connections
	SOCKET pop_socket(); // remove a socket from queue and return it (wait for no connections waiting)
	bool client_present(); // true if connected client is waiting
	bool waitForClient(); // wait until client connects, false if return due to accept failure
	bool listening(); // return if currently accepting connections
	bool active(); // return true if could be or is already accepting
	static bool start_winsock(); // starts fundamental winsock resources - must run before any networking can happen
};

#endif