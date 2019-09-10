#include "../Network-Module/NetworkModule.hpp"

static int counter = 0;

void CALLBACK ProcessPackage(Package p) {
	counter++;
	std::string s = "Client #" + std::to_string(p.senderId) + " says: ";
	s += p.msg;
	s += "\n";

	printf(s.c_str());
}

int main() {
	SetConsoleTitle(("This is The Host"));

	Network n;
	bool ok = n.SetupHost(54000);

	char msg[] = "Server Says Hi!";
	n.Send(msg, sizeof(msg), -1);

	//Keep main from closing
	while (ok) {
		if (counter >= 3) {
			counter = 0;

			n.Send(msg, sizeof(msg), -1);
		}
		n.CheckForPackages(&ProcessPackage);
	};

	return 0;
}