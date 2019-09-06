#include "../TCP-Connect/NetworkModule.hpp"

int main() {

	Network n;
	n.SetupHost(54000);

	//Keep main from closing
	while (true) {};

	return 0;
}