#pragma once
#include <WS2tcpip.h>
#pragma comment (lib, "ws2_32.lib")
#include <vector>
#include <iostream>
#include <thread>
#include <mutex>

struct Connection
{
	int id;
	bool isConnected;
	SOCKET socket;
	std::string ip, port;
	std::thread* thread;//The thread used to listen for messages
	Connection() {
		id = -2;
		isConnected = false;
		socket = NULL;
		ip = port = "";
		thread = nullptr;
	}
};

class Network
{
public:
	Network();
	~Network();

	/*
		Call SetupHost() to initialize a host socket. Dont call this and SetupHost() in the same application.
	*/
	bool SetupHost(unsigned short port);
	/*
		Call SetupClient() to initialize a client socket. Dont call this and SetupHost() in the same application.
	*/
	bool SetupClient(const char* host_ip, unsigned short hostport);
	/*
		Send a message to connection "receiverID".
		If SetupClient has been called, any number passed to receiverID will all send to the connected host.
		If SetupHost has been called, passing -1 to receiverID will send the message to all connected clients.

		Return true if message could be sent to all receivers.
	*/
	bool Send(const char* message, size_t size, int receiverID = 0);
	bool Send(const char* message, size_t size, Connection conn);

private:
	SOCKET m_soc = 0;
	sockaddr_in m_myAddr = {};
	std::thread* m_clientAcceptThread = nullptr;

	bool m_isServer = false;
	bool m_isInitialized = false;
	/*Do not access m_connections without mutex lock*/
	std::vector<Connection> m_connections;
	std::mutex m_mutex_connections;

	/*
		Only used by the server. This function is called in a new thread and waits for new incomming connection requests.
		Accepted connections are stored in m_connections. A new thread is created for each connection directly Listen() in order to listen for incomming messages from that connection;
	*/
	void WaitForNewConnections();

	/*
		This function is called by multiple threads. Listen for incomming messages from one specific connection.
		If this application is a client, only one thread will call Listen() which will listen to the host.
		If this application is a host, this function will be called by one thread for each client connected to the host.

		Host connection requests is handled in WaitForNewConnections()
	*/
	void Listen(const Connection conn);//Rename this function
};