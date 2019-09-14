#include "NetworkModule.hpp"
//
//static BOOL bOptVal = TRUE;
//static int bOptLen = sizeof(bool);

Network::Network() {}

Network::~Network() {}

void Network::checkForPackages(void(*m_callbackfunction)(NetworkEvent))
{

	bool morePackages = true;
	while (morePackages)
	{
		std::lock_guard<std::mutex> lock(m_mutex_packages);

		if (m_pstart == m_pend) {
			morePackages = false;
			break;
		}

		m_callbackfunction(m_awaitingEvents[m_pstart]);

		m_pstart = (m_pstart + 1) % MAX_AWAITING_PACKAGES;
	}

}

bool Network::setupHost(unsigned short port, USHORT hostFlags)
{
	if (m_isInitialized)
		return false;

	m_connections.clear();
	m_connections.reserve(128);

	m_hostFlags = hostFlags;
	m_awaitingMessages = new MessageData[MAX_AWAITING_PACKAGES];
	m_awaitingEvents = new NetworkEvent[MAX_AWAITING_PACKAGES];

	WSADATA data;
	WORD version = MAKEWORD(2, 2); // use version winsock 2.2
	int status = WSAStartup(version, &data);
	if (status != 0) {
		printf("Error starting WSA\n");
		return false;
	}
	m_soc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //IPPROTO_TCP
	if (m_soc == INVALID_SOCKET) {
		printf("Error creating socket\n");
	}
	m_myAddr.sin_addr.S_un.S_addr = ADDR_ANY;
	m_myAddr.sin_family = AF_INET;
	m_myAddr.sin_port = htons(port);

	if (bind(m_soc, (sockaddr*)& m_myAddr, sizeof(m_myAddr)) == SOCKET_ERROR) {
		printf("Error binding socket\n");
		return false;
	}

	BOOL bOptVal = TRUE;
	int bOptLen = sizeof(bool);

	int iOptVal = 0;
	int iOptLen = sizeof(int);

	setsockopt(m_soc, IPPROTO_TCP, TCP_NODELAY, (char*)&bOptVal, bOptLen);//Prepare the socket to listen
	setsockopt(m_soc, IPPROTO_TCP, SO_SNDBUF, (char*)&iOptVal, iOptLen);//Prepare the socket to listen
	
	::listen(m_soc, SOMAXCONN);

	m_shutdown = false;
	m_isInitialized = true;
	m_isServer = true;

	//Start a new thread that will wait for new connections
	m_clientAcceptThread = new std::thread(&Network::waitForNewConnections, this);
	if (m_hostFlags & (USHORT)HostFlags::ENABLE_LAN_SEARCH_VISIBILITY) {
		if (startUDPSocket(port + 1)) {
			m_UDPListener = new std::thread(&Network::listenForUDP, this);
		}
	}

	return true;
}

bool Network::setupClient(const char* IP_adress, unsigned short hostport)
{
	if (m_isInitialized)
		return false;

	m_awaitingMessages = new MessageData[MAX_AWAITING_PACKAGES];
	m_awaitingEvents = new NetworkEvent[MAX_AWAITING_PACKAGES];

	WSADATA data;
	WORD version = MAKEWORD(2, 2); // use version winsock 2.2
	int status = WSAStartup(version, &data);
	if (status != 0) {
		printf("Error starting WSA\n");
		return false;
	}

	m_soc = socket(AF_INET, SOCK_STREAM, 0); //IPPROTO_TCP
	if (m_soc == INVALID_SOCKET) {
		printf("Error creating socket\n");
		return false;
	}

	//m_myAddr.sin_addr.S_un.S_addr = ADDR_ANY;
	m_myAddr.sin_family = AF_INET;
	m_myAddr.sin_port = htons(hostport);
	inet_pton(AF_INET, IP_adress, &m_myAddr.sin_addr);

	BOOL bOptVal = TRUE;
	int bOptLen = sizeof(bool);

	int iOptVal = 0;
	int iOptLen = sizeof(int);

	setsockopt(m_soc, IPPROTO_TCP, TCP_NODELAY, (char*)& bOptVal, bOptLen);//Prepare the socket to listen
	setsockopt(m_soc, IPPROTO_TCP, SO_SNDBUF, (char*)& iOptVal, iOptLen);//Prepare the socket to listen

	//connect needs error checking!
	int conres = connect(m_soc, (sockaddr*)&m_myAddr, sizeof(m_myAddr));
	if (conres == SOCKET_ERROR) {
		printf("Error connecting to host\n");
		return false;
	}

	//char tempMsg[MAX_PACKAGE_SIZE] = {};
	/*char* test = reinterpret_cast<char*>(&reconnectID);
	send(m_soc, test, sizeof(ConnectionID), 0);
	recv(m_soc, reinterpret_cast<char*>(&m_myClientID), sizeof(ConnectionID), 0);*/

	//m_myClientID = *reinterpret_cast<ConnectionID*>(tempMsg);
	//if (reconnectID != -1) {
	//	const char* test = reinterpret_cast<const char*>(&reconnectID);
	//	//send(m_soc, test, sizeof(ConnectionID), 0);
	//}
	
	Connection* conn = new Connection;
	conn->isConnected = true;
	conn->socket = m_soc;
	conn->ip = "";
	conn->port = ntohs(m_myAddr.sin_port);
	conn->id = 0;
	conn->thread = new std::thread(&Network::listen, this, conn); //Create new listening thread listening for the host
	m_connections[conn->id] = conn;

	startUDPSocket(hostport + 1);

	m_isInitialized = true;
	m_isServer = false;

	return true;
}

bool Network::send(const char* message, size_t size, TCP_CONNECTION_ID receiverID)
{
	if (receiverID == -1 && m_isServer) {
		int n = 0;
		{
			std::lock_guard<std::mutex> mu(m_mutex_connections);
			n = m_connections.size();
		}
		int success = 0;
		for (int i = 0; i < n; i++)
		{

		}

		Connection* conn = nullptr;
		for (auto it : m_connections)
		{
			conn = it.second;
			if(send(message, size, conn))
				success++;
		}

		return true;
	}	

	char msg[MAX_PACKAGE_SIZE] = {0};
	memcpy(msg, message, size);

	Connection* conn = nullptr;
	{
		std::lock_guard<std::mutex> mu(m_mutex_connections);
		if (receiverID > m_connections.size()) {
			return false;
		}

		conn = m_connections[receiverID];
	}

	if (!conn) {
		return false;
	}

	if (!conn->isConnected) {
		return false;
	}

	if (::send(conn->socket, msg, MAX_PACKAGE_SIZE, 0) == SOCKET_ERROR) {
		return false;
	}

	return true;
}

bool Network::send(const char* message, size_t size, Connection* conn)
{
	if (!conn->isConnected) {
		return false;
	}

	if (::send(conn->socket, message, size, 0) == SOCKET_ERROR) {
		return false;
	}
		
	return true;
}

bool Network::searchHostsOnLan()
{	
	char msg[MAX_PACKAGE_SIZE] = "AnyLanHostsHere?";

	if (::sendto(m_UDP_soc, msg, MAX_PACKAGE_SIZE, 0, (sockaddr*)&m_UDP_myAddr, sizeof(m_UDP_myAddr)) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		printf("Send Error: %d", err);
		return false;
	}

	return true;
}

bool Network::startUDPSocket(unsigned short port)
{
	m_UDP_soc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_UDP_soc == INVALID_SOCKET) {
		printf("Error creating socket with error: %d\n", WSAGetLastError());
		return false;
	}

	ULONG bAllow = 1;
	if (setsockopt(m_UDP_soc, SOL_SOCKET, SO_BROADCAST, (char*)& bAllow, sizeof(bAllow)) != 0) {
		printf("Error setsockopt with error: %d\n", WSAGetLastError());
		return false;
	}

	m_UDP_myAddr.sin_family = AF_INET;
	m_UDP_myAddr.sin_port = htons(port);
	if (m_isServer) {
		m_UDP_myAddr.sin_addr.S_un.S_addr = INADDR_ANY;

		if (bind(m_UDP_soc, (sockaddr*)& m_UDP_myAddr, sizeof(m_UDP_myAddr)) == SOCKET_ERROR) {
			printf("Error binding socket with error: %d\n", WSAGetLastError());
			return false;
		}
	}
	else {
		m_UDP_myAddr.sin_addr.S_un.S_addr = INADDR_BROADCAST;
	}

	//if (m_isServer) {
	//	if (bind(m_UDP_soc, (sockaddr*)& m_UDP_myAddr, sizeof(m_UDP_myAddr)) == SOCKET_ERROR) {
	//		printf("Error binding socket with error: %d\n", WSAGetLastError());
	//		return false;
	//	}
	//}

	//setsockopt(m_UDP_soc, SOL_SOCKET, SO_, (char*)& bAllow, sizeof(bAllow));

	return true;
}

void Network::listenForUDP()
{
	char buffer[MAX_PACKAGE_SIZE];
	sockaddr_in client = {0};
	int clientSize = sizeof(sockaddr_in);

	char sendMSG[] = "Broadcast message from SLAVE TAG";

	//if (sendto(m_UDP_soc, sendMSG, strlen(sendMSG) + 1, 0, (sockaddr*)& m_UDP_myAddr, sizeof(m_UDP_myAddr)) == SOCKET_ERROR) {
	//	printf("Error Sending UDP : %d\n", WSAGetLastError());
	//}

	while (!m_shutdown)
	{
		int bytesRec = recvfrom(m_UDP_soc, buffer, MAX_PACKAGE_SIZE, 0, (sockaddr*)& client, &clientSize);
		if (bytesRec > 0) {
			printf("Recived %d bytes : from UDP\n", bytesRec);
			printf(buffer);
			printf("\n");
		}
		else {
			printf("Error recvfrom with error: %d\n", WSAGetLastError());		
		}

	}
}

TCP_CONNECTION_ID Network::generateID()
{
	TCP_CONNECTION_ID id = 0;

	if (m_hostFlags & (USHORT)HostFlags::USE_RANDOM_IDS) {
		int n = sizeof(TCP_CONNECTION_ID) / sizeof(int);

		for (size_t i = 0; i < n; i++)
		{
			int r = rand();
			id |= (TCP_CONNECTION_ID)r << (i * 32);
		}
	}
	else {
		id == m_nextID++;
	}

	return id;
}

void Network::shutdown()
{
	m_shutdown = true;

	if (m_isServer) {
		::shutdown(m_soc, 2);
		if (closesocket(m_soc) == SOCKET_ERROR) {
#ifdef DEBUG_NETWORK
			printf("Error closing m_soc\n");
#endif
		}
		else if (m_clientAcceptThread) {
			m_clientAcceptThread->join();
			delete m_clientAcceptThread;
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_mutex_connections);
		for (auto it : m_connections)
		{
			Connection* conn = it.second;
			::shutdown(conn->socket, 2);
			if (closesocket(conn->socket) == SOCKET_ERROR) {
#ifdef DEBUG_NETWORK
				printf((std::string("Error closing socket") + std::to_string(conn->id) + "\n").c_str());
#endif
			}
			else if (conn->thread) {
				conn->thread->join();
				delete conn->thread;
				delete conn;
			}
		}
	}

	delete[] m_awaitingEvents;
	delete[] m_awaitingMessages;

	m_connections.clear();
	m_isInitialized = false;
}

void Network::addNetworkEvent(NetworkEvent n, int dataSize)
{
	std::lock_guard<std::mutex> lock(m_mutex_packages);
	
	memcpy(m_awaitingMessages[m_pend].msg, n.data->msg, dataSize);
	m_awaitingEvents[m_pend].eventType = n.eventType;
	m_awaitingEvents[m_pend].clientID = n.clientID;
	m_awaitingEvents[m_pend].data = &m_awaitingMessages[m_pend];

	m_pend = (m_pend + 1) % MAX_AWAITING_PACKAGES;
	if (m_pend == m_pstart)
		m_pstart++;
}

void Network::waitForNewConnections()
{
	while (!m_shutdown)
	{
		sockaddr_in client;
		int clientSize = sizeof(client);

		SOCKET clientSocket = accept(m_soc, (sockaddr*)& client, &clientSize);
		if (clientSocket == INVALID_SOCKET) {
			//Do something
			continue;
		}

		char host[NI_MAXHOST] = { 0 }; // Client's remote name

		inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);

		if (true) {
			Connection* conn = new Connection;
			conn->isConnected = true;
			conn->socket = clientSocket;
			conn->ip = host;
			conn->port = ntohs(client.sin_port);

			bool ok = false;
			do {
				conn->id = generateID();
				if (m_connections.find(conn->id) == m_connections.end()) {
					ok = true;
				}
			} while (!ok);

			conn->thread = new std::thread(&Network::listen, this, conn); //Create new listening thread for the new connection
			m_connections[conn->id] = conn;
		}
	}
}

void Network::listen(const Connection* conn)
{
	NetworkEvent nEvent;
	nEvent.clientID = conn->id;
	nEvent.eventType = NETWORK_EVENT_TYPE::CLIENT_JOINED;
	addNetworkEvent(nEvent, 0);

	bool connectionIsClosed = false;
	char msg[MAX_PACKAGE_SIZE];

	while (!connectionIsClosed && !m_shutdown) {
		ZeroMemory(msg, sizeof(msg));
		int bytesReceived = recv(conn->socket, msg, MAX_PACKAGE_SIZE, 0);

#ifdef DEBUG_NETWORK
		printf((std::string("bytesReceived: ") + std::to_string(bytesReceived) + "\n").c_str());
#endif // DEBUG_NETWORK		
		switch (bytesReceived)
		{
		case 0:
#ifdef DEBUG_NETWORK
			printf("Client Disconnected\n");
#else
		case SOCKET_ERROR :
#endif // DEBUG_NETWORK
			connectionIsClosed = true;
			nEvent.eventType = NETWORK_EVENT_TYPE::CLIENT_DISCONNECTED;
			addNetworkEvent(nEvent, 0);
			break;
#ifdef DEBUG_NETWORK
		case SOCKET_ERROR:
			printf("Error Receiving: Disconnecting client.\n");
			connectionIsClosed = true;
			nEvent.eventType = NETWORK_EVENT_TYPE::CLIENT_DISCONNECTED;
			addNetworkEvent(nEvent, 0);
			break;
#endif // DEBUG_NETWORK
		default:
		
			nEvent.data = reinterpret_cast<MessageData*>(msg);
			nEvent.eventType = NETWORK_EVENT_TYPE::MSG_RECEIVED;

			addNetworkEvent(nEvent, bytesReceived);
		
			break;
		}
	}
}