#include "NetworkModule.hpp"
//
//static BOOL bOptVal = TRUE;
//static int bOptLen = sizeof(bool);

Network::Network() {}

Network::~Network() {}

void Network::CheckForPackages(void(*m_callbackfunction)(Package))
{

	bool morePackages = true;
	while (morePackages)
	{
		std::lock_guard<std::mutex> lock(m_mutex_packages);

		if (m_pstart == m_pend) {
			morePackages = false;
			break;
		}

		m_callbackfunction(m_awaitingPackages[m_pstart]);

		m_pstart = (m_pstart + 1) % MAX_AWAITING_PACKAGES;
	}

}

bool Network::SetupHost(unsigned short port, USHORT hostFlags)
{
	if (m_isInitialized)
		return false;

	//Tell GenerateID() to use random ids or not.
	//m_useRandomIDs = hostFlags & (USHORT)HostFlags::USE_RANDOM_IDS;
	m_hostFlags = hostFlags;

	m_awaitingPackages = new Package [MAX_AWAITING_PACKAGES];

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
	
	listen(m_soc, SOMAXCONN);

	m_isInitialized = true;
	m_isServer = true;

	//Start a new thread that will wait for new connections
	m_clientAcceptThread = new std::thread(&Network::WaitForNewConnections, this);

	return true;
}

bool Network::SetupClient(const char* IP_adress, unsigned short hostport, ConnectionID reconnectID)
{
	if (m_isInitialized)
		return false;

	m_awaitingPackages = new Package[MAX_AWAITING_PACKAGES];

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
	char* test = reinterpret_cast<char*>(&reconnectID);
	send(m_soc, test, sizeof(ConnectionID), 0);
	recv(m_soc, reinterpret_cast<char*>(&m_myClientID), sizeof(ConnectionID), 0);

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
	conn->thread = new std::thread(&Network::Listen, this, conn); //Create new listening thread listening for the host
	m_connections[conn->id] = conn;

	m_isInitialized = true;
	m_isServer = false;

	return true;
}

bool Network::Send(const char* message, size_t size, ConnectionID receiverID)
{
	if (receiverID == -1 && m_isServer) {
		//int n = 0;
		//{
		//	std::lock_guard<std::mutex> mu(m_mutex_connections);
		//	n = m_connections.size();
		//}
		//int success = 0;
		//for (int i = 0; i < n; i++)
		//{
		//	if (Send(message, size, i))
		//		success++;
		//}
		//printf((std::string("Broadcasting to ") + std::to_string(success) + "/" + std::to_string(n) + " Clients: " + std::string(message) + "\n").c_str());

		return true;
	}	

	char msg[MAX_PACKAGE_SIZE] = {0};
	memcpy(msg, message, size);

	Connection* conn;
	{
		std::lock_guard<std::mutex> mu(m_mutex_connections);
		if (receiverID > m_connections.size())
			return false;

		conn = m_connections[receiverID];
	}

	if (!conn->isConnected)
		return false;

	if (send(conn->socket, msg, MAX_PACKAGE_SIZE, 0) == SOCKET_ERROR)
		return false;

	return true;
}

bool Network::Send(const char* message, size_t size, const Connection* conn)
{
	if (!conn->isConnected)
		return false;

	if (send(conn->socket, message, size, 0) == SOCKET_ERROR)
		return false;

	return true;
}

ConnectionID Network::GenerateID()
{
	ConnectionID id = 0;

	if (m_hostFlags & (USHORT)HostFlags::USE_RANDOM_IDS) {
		int n = sizeof(ConnectionID) / sizeof(int);

		for (size_t i = 0; i < n; i++)
		{
			int r = rand();
			id |= (ConnectionID)r << (i * 32);
		}
	}
	else {
		id == m_nextID++;
	}

	return id;
}

void Network::WaitForNewConnections()
{
	while (true)
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
		std::string s = host;
		s += " connected on port " + std::to_string(ntohs(client.sin_port)) + "\n";
		printf(s.c_str());

		ConnectionID id = 0;
		recv(clientSocket, reinterpret_cast<char*>(&id), sizeof(ConnectionID), 0);
		if (id != 0) {
			s = "Client Requested to get id: " + std::to_string(id);
			if (m_hostFlags & (USHORT)HostFlags::ALLOW_CLIENT_ID_REQUEST) {
				s += "/t Accepted./n";
			}
			else {
				s += "/t Denied./n";
			}

			//Check if id is valid

			//Confirm client id
			send(clientSocket, reinterpret_cast<char*>(&id), sizeof(ConnectionID), 0);

			printf(s.c_str());
		}

		if (id == 0) {
			Connection* conn = new Connection;
			conn->isConnected = true;
			conn->socket = clientSocket;
			conn->ip = host;
			conn->port = ntohs(client.sin_port);

			bool ok = false;
			do {
				conn->id = GenerateID();
				if (m_connections.find(conn->id) == m_connections.end()) {
					ok = true;
				}
			} while (!ok);

			conn->thread = new std::thread(&Network::Listen, this, conn); //Create new listening thread for the new connection
			m_connections[conn->id] = conn;
		}
		else {

		}
	}
}

void Network::Listen(const Connection* conn)
{
	bool connectionIsClosed = false;
	char msg[MAX_PACKAGE_SIZE];

	while (!connectionIsClosed) {
		ZeroMemory(msg, sizeof(msg));
		int bytesReceived = recv(conn->socket, msg, MAX_PACKAGE_SIZE, 0);
		printf((std::string("bytesReceived: ") + std::to_string(bytesReceived) + "\n").c_str());
		
		switch (bytesReceived)
		{
		case 0:
			printf("Client Disconnected\n");
			connectionIsClosed = true;
			break;
		case SOCKET_ERROR:
			printf("Error Receiving: Disconnecting client.\n");
			connectionIsClosed = true;
			break;
		default:
		{
			std::lock_guard<std::mutex> lock(m_mutex_packages);
			m_awaitingPackages[m_pend].senderId = conn->id;
			memcpy(m_awaitingPackages[m_pend].msg, msg, bytesReceived);
			m_pend = (m_pend + 1) % MAX_AWAITING_PACKAGES;
			if (m_pend == m_pstart)
				m_pstart++;
		}
			break;
		}
	}
}

void Network::Pinger()
{
	//while (true)
	//{
	//	std::this_thread::sleep_for(std::chrono::seconds(1));
	//	
	//	for (auto conn : m_connections)
	//	{
	//		if (conn.second->isConnected) {
	//			std::string s = "Ping " + std::to_string(conn.second->lastPingID);
	//			Send(s.c_str(), s.length + 1, conn.second);
	//		}
	//	}

	//}
}
