#pragma once
#include <WS2tcpip.h>
#pragma comment (lib, "ws2_32.lib")
#include <vector>
#include <unordered_map>
#include <string>
#include <thread>
#include <mutex>

#define MAX_PACKAGE_SIZE 64
#define MAX_AWAITING_PACKAGES 1000
//#define DEBUG_NETWORK

typedef unsigned long long TCP_CONNECTION_ID;

struct Connection
{	
	std::string ip, port;
	TCP_CONNECTION_ID id;
	bool isConnected;
	SOCKET socket;
	std::thread* thread;//The thread used to listen for messages

	Connection() {
		id = 0;
		isConnected = false;
		socket = NULL;
		ip = port = "";
		thread = nullptr;
	}
};

enum class NETWORK_EVENT_TYPE {
	NETWORK_ERROR,
	CLIENT_JOINED,
	CLIENT_DISCONNECTED,
	CLIENT_RECONNECTED,
	MSG_RECEIVED,
};

struct MessageData {
	char msg[MAX_PACKAGE_SIZE];
};

struct NetworkEvent {
	NETWORK_EVENT_TYPE eventType;
	TCP_CONNECTION_ID clientID;
	MessageData* data;

	NetworkEvent() {
		eventType = NETWORK_EVENT_TYPE::NETWORK_ERROR;
		clientID = 0;
		data = nullptr;
	}
};

/*
	This function needs to be implemented by the application and passed to SetupHost or SetupClient.
*/
//void CALLBACK ProcessPackage(Package p);

class Network
{
public:
	enum HostFlags : USHORT
	{
		USE_RANDOM_IDS = 1U,
		//ALLOW_CLIENT_ID_REQUEST = 2U, //Allow a client to reconnect with the same id if they remember it after a disconnect
		ENABLE_LAN_SEARCH_VISIBILITY = 2U
	};

	Network();
	~Network();

	void checkForPackages(void (*m_callbackfunction)(NetworkEvent));

	/*
		Call SetupHost() to initialize a host socket. Dont call this and SetupHost() in the same application.
	*/
	bool setupHost(unsigned short port, USHORT hostFlags = (USHORT)HostFlags::USE_RANDOM_IDS);
	/*
		Call SetupClient() to initialize a client socket. Dont call this and SetupHost() in the same application.
	*/
	bool setupClient(const char* host_ip, unsigned short hostport);
	/*
		Send a message to connection "receiverID".
		If SetupClient has been called, any number passed to receiverID will all send to the connected host.
		If SetupHost has been called, passing -1 to receiverID will send the message to all connected clients.

		Return true if message could be sent to all receivers.
	*/
	bool send(const char* message, size_t size, TCP_CONNECTION_ID receiverID = 0);
	bool send(const char* message, size_t size, Connection* conn);
	
	bool searchHostsOnLan();

	void shutdown();

private:
	const char lanHostSearchMessage[MAX_PACKAGE_SIZE] = "AnyLanHostsHere?";

	bool m_shutdown = false;
	
	//TCP CONNECTION
	SOCKET m_soc = 0;
	sockaddr_in m_myAddr = {};
	std::thread* m_clientAcceptThread = nullptr;

	//UDP CONNECTION
	SOCKET m_UDP_soc = 0;
	sockaddr_in m_UDP_myAddr = {};
	std::thread* m_UDPListener = nullptr;

	//GENERIC
	bool m_isServer = false;
	bool m_isInitialized = false;
	USHORT m_hostFlags;
	TCP_CONNECTION_ID m_nextID = 0; //Only used if m_useRandomIDs == false
	TCP_CONNECTION_ID m_myClientID = 0; //Used by clients only

	/*Do not access m_connections without mutex lock*/
	std::unordered_map<size_t, Connection*> m_connections;
	std::mutex m_mutex_connections;

	MessageData* m_awaitingMessages;
	NetworkEvent* m_awaitingEvents;

	int m_pstart = 0, m_pend = 0;
	std::mutex m_mutex_packages;
	
	bool startUDPSocket(unsigned short port);
	void listenForUDP();

	TCP_CONNECTION_ID generateID();
	void addNetworkEvent(NetworkEvent n, int dataSize);

	/*
		Only used by the server. This function is called in a new thread and waits for new incomming connection requests.
		Accepted connections are stored in m_connections. A new thread is created for each connection directly Listen() in order to listen for incomming messages from that connection;
	*/
	void waitForNewConnections();

	/*
		This function is called by multiple threads. Listen for incomming messages from one specific connection.
		If this application is a client, only one thread will call Listen() which will listen to the host.
		If this application is a host, this function will be called by one thread for each client connected to the host.

		Host connection requests is handled in WaitForNewConnections()
	*/

	void listen(const Connection* conn);//Rename this function
};