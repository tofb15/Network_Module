#include "../Network-Module/NetworkModule.hpp"
#include <iostream>

static int counter = 1;
Network network;

//PlayerJoined: Do whatever
void PlayerJoined(TCP_CONNECTION_ID id) {
	printf((std::string("New player Joined with id: ") + std::to_string(id) + "\n").c_str());
}
//PlayerDisconnected: Do whatever
void PlayerDisconnected(TCP_CONNECTION_ID id) {
	printf((std::string("Player Disconnected with id: ") + std::to_string(id) + "\n").c_str());
}

//PlayerReconnected: Do whatever
void PlayerReconnected(TCP_CONNECTION_ID id) {
	printf((std::string("Player Reconnected with id: ") + std::to_string(id) + "\n").c_str());
}
//MessageRecieved: Do whatever
void DecodeMessage(NetworkEvent nEvent) {
	counter++;
	std::string s = "Client #" + std::to_string(nEvent.clientID) + " says: ";
	s += nEvent.data->rawMsg;
	s += "\n";

	printf(s.c_str());
}

void CALLBACK ProcessPackage(NetworkEvent nEvent) {

	switch (nEvent.eventType)
	{
	case NETWORK_EVENT_TYPE::NETWORK_ERROR:
		break;
	case NETWORK_EVENT_TYPE::CONNECTION_ESTABLISHED:
		PlayerJoined(nEvent.clientID);
		break;
	case NETWORK_EVENT_TYPE::CONNECTION_CLOSED:
		PlayerDisconnected(nEvent.clientID);
		break;
	case NETWORK_EVENT_TYPE::CONNECTION_RE_ESTABLISHED:
		PlayerReconnected(nEvent.clientID);
		break;
	case NETWORK_EVENT_TYPE::MSG_RECEIVED:
		DecodeMessage(nEvent);
		break;
	default:
		break;
	}
}

int main() {
	SetConsoleTitle(("This is The Host"));

	printf(std::string("Press Enter to start server program... ").c_str());
	getchar();

	/* 1) Setup server. The ENABLE_LAN_SEARCH_VISIBILITY flag will make the host respond to clients that look for hosts on the LAN.*/
	network.initialize();
	bool ok = network.host(54000, Network::HostFlags::USE_RANDOM_IDS | Network::HostFlags::ENABLE_LAN_SEARCH_VISIBILITY);
	if (!ok) {
		printf("Failed to host\n");
		return 0;
	}

	/* 2) set up server meta data. This will be broadcasted on the LAN if any client is looking for hosts. (Optional)*/
	char desc[] = "Grymma Servern";
	network.setServerMetaDescription(desc, sizeof(desc));

	/* 3) Server Loop*/
	printf("Host is active and waiting for clients!\n");
	char msg[] = "Server Says Hi!";
	while (ok) {
		if (counter % 3 == 0) {
			//Send some message to all clients every now and then.
			counter++;
			printf("====Saying Hi To My Clients====\n");
			network.send(msg, sizeof(msg), -1);
		}
		if (counter < 320) {
			//Check if clients sent any messages
			network.checkForPackages(&ProcessPackage);
		}
		else
		{
			//Stop the loop after a while
			ok = false;
		}
	};

	std::this_thread::sleep_for(std::chrono::seconds(2));

	// 4) Clean up by calling shutdown(). Any connected clients will get the CONNECTION_CLOSED Event
	printf("Closing Server...");
	network.shutdown();
	printf(" OK");

	return 0;
}