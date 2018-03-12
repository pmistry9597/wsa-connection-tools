#include "connection.h"
#include <thread>

// NOTES:
// have to change inet_ntoa to inet_pton and/or inet_ntop

void Connection::sendWorker() {
	// if send queue is not empty, loop until all messages sent (set sendRunning true if currrently looping)
		// if empty, wait on condition_variable to send messages (set sendRunning false if going to wait)
		DWORD flags = 0;// flags for wsasend
		while (!quit) {
			// lock sendMutex for protection on sendQueue and for send_cv
			std::unique_lock<std::mutex> sendLock(sendMutex);
			while (!sendQueue.empty()) { // only loop if sendQueue is not empty
				std::string top = sendQueue.front();
				WSABUF sendBuf;
				//const char* sendMsg = top.c_str();
				// copy contents of the c_str from top into a new char array
				char c_msg[top.size()]; // c style string will be stored here for sending
				for (int i = 0; i < top.size(); i++) {
					c_msg[i] = top[i];
				}
				sendBuf.len = top.size();
				sendBuf.buf = c_msg;
				int result = WSASend(connection, &sendBuf, 1, NULL, flags, &sendOverlapped, 0); // last argument should be a completion routine, but so far none is needed
				// assess whether the result is an actual error
				if (result != 0) { // 0 means sending already happened
					// wait for sending to complete if waiting for completion
					if (WSAGetLastError() == WSA_IO_PENDING) {
						WSAWaitForMultipleEvents(1, &sendOverlapped.hEvent, FALSE, WSA_INFINITE, TRUE);
					} else {
						// quit if error occurred (connection no longer functioning)
						quit = true;
						return;
					}
				}
				// remove message from queue
				sendQueue.pop();
			}
			// sendRunning will be false now since going into wait mode
			if (quit) { // check for quit before going into wait
				break; 
			}
			send_cv.wait(sendLock);
		}
}
void Connection::recvWorker() {
	// buffer for received messages - store address into recvOverlapped
		char buf[recvBuf.len];
		recvBuf.buf = buf;
		while (!quit) {
			// stores number of bytes received
			DWORD bytesRecv = 0;
			// run wsarecv to wait for client to send message - then run recv loop in another thread until client disconnects (bytes zero) or null terminator reached
			DWORD flags = 0;// flags for wsarecv operation
			int result = WSARecv(connection, &recvBuf, 1, NULL, &flags, &recvOverlapped, 0); // recvRoutine is not declared yet
			// wait for event to be triggered (by external threads/events)
			if (result != 0) {
				// wait if receiving still happening
				if (WSAGetLastError() == WSA_IO_PENDING) {
					WSAWaitForMultipleEvents(1, &recvOverlapped.hEvent, FALSE, WSA_INFINITE, TRUE);
					WSAResetEvent(recvOverlapped.hEvent); // reset event
				} else {
					// quit since this means connection failure
					quit = true;
					break;
				}
			}		
			// get number of bytes received
			WSAGetOverlappedResult(connection, &recvOverlapped, &bytesRecv, FALSE, 0);
			// 0 bytes received means connection ended
			if (bytesRecv == 0) {
				quit = true;
				return;
			}
			//std::cout << "Bytes received: " + std::to_string(bytesRecv) << "\n";
			// loop through the chars in buf until cbTransferred reached
			// until then submit each null terminated sequence as a string into recvQueue
			std::string strBuffer = ""; // buffer for current string being read until submission
			for (int i = 0; i < bytesRecv; i++) {
				char current = buf[i];
				if (current == '\0') { // submit string as null has been reached
					if (strBuffer != "") {
						std::lock_guard<std::mutex> recvLock(recvMutex); // multi threaded queue so must lock before using recvQueue
						recvQueue.push(strBuffer);
						strBuffer = ""; // clear buffer for next string
					}
					if (recvEvent) { // call function meant to be run on message recv if function is present
						recvEvent();
					}
					recv_cv.notify_all(); // notify any thread waiting for message to arrive
					continue;
				}
				strBuffer += current;
			}	
		}
		// notify recv_cv in case waitForMessage is happening
		recv_cv.notify_all();
}
Connection::Connection(SOCKET connection, int bufsize, const std::function<void()>& recvEvent) {
	this->connection = connection;
	this->recvEvent = recvEvent;
	// set the size to 1 since its only a single character
	recvBuf.len = bufsize;
	recvOverlapped.hEvent = WSACreateEvent();
	sendOverlapped.hEvent = WSACreateEvent();
	quit = false;
	// start sending and receiving thread
	std::thread sendThread(&Connection::sendWorker, this);
	sendThread.detach();
	std::thread recvThread(&Connection::recvWorker, this);
	recvThread.detach();

	// get port and ip address data of the other side
	int addrSize = sizeof(SOCKADDR_IN);
	SOCKADDR_IN addrInfo; // where port and ip will be stored by winsock
	// attempt to get the ip and port data and store into this object if successful
	if (getpeername(connection, (SOCKADDR*)&addrInfo, &addrSize) == 0) { //0 return means successful data return
		int_port = ntohs(addrInfo.sin_port); // convert port data to port number
		str_ip_address = std::string(inet_ntoa(addrInfo.sin_addr)); // convert ip data to string form
	}
}
Connection::~Connection() {
	WSACloseEvent(recvOverlapped.hEvent); // close events when done
	WSACloseEvent(sendOverlapped.hEvent);
	closesocket(connection); // close connection
}
void Connection::push_msg(std::string msg) {
	// protect queue with mutex
	std::lock_guard<std::mutex> sendLock(sendMutex);
	sendQueue.push(msg);
	// notify send worker
	send_cv.notify_one();
}
std::string Connection::pop_msg() {
	std::lock_guard<std::mutex> recvLock(recvMutex);// protect the recvQueue
	// return nothing if empty
	if (recvQueue.empty()) {
		return "";
	}
	std::string msg = std::string(recvQueue.front());
	recvQueue.pop(); // delete the message in recvQueue since no longer needed
	return msg;
}
void Connection::close() {
	quit = true;
	// notify all threads
	WSASetEvent(recvOverlapped.hEvent);
	WSASetEvent(sendOverlapped.hEvent);
	send_cv.notify_all();
	closesocket(connection); // close connection
}



void Connection::set_recvEvent(const std::function<void()>& recvEvent) {
	this->recvEvent = recvEvent;
}
bool Connection::msg_present() {
	return !recvQueue.empty();
}
bool Connection::waitForMessage() {
	std::unique_lock<std::mutex> recvLock(recvMutex);
	while (recvQueue.empty() && !quit) {
		// wait for notification when empty
		recv_cv.wait(recvLock);
	}
	// return value will be whetehr the connection is still alive
	return !quit;
}
bool Connection::is_alive() {
	return !quit;
}
std::string Connection::ip_address() {
	return str_ip_address;
}
int Connection::port() { // return the port number
	return int_port;
}
bool Connection::start_winsock() {
	WORD wsVersion = MAKEWORD(2,2);
	WSADATA wsData;
	if (WSAStartup(wsVersion, &wsData) != 0) { // start winsock
		return false; // return false to indicate failure to start
	} 
	return true; // true to mean successful start since successful if this function reached this point
}