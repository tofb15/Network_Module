# Network_Module

A host/client module written in c++ that can be included and used in other programs to establish network communication.

## Author

Name: Tobias "swimba" F. 
Git: tofb15

## Environment
This software was developed on Windows 10 using Visual Studio 2019

## Features:
- Can be used by applications both to act as host and as client.
- Communication made with TCP-No_Delay.
- Multi-threaded connections allowing both listening and receiving at the same time.
- Host can have any number of clients connected.
- Host can broadcast to all clients or choose to communicate with a specific client only.
- Clients can automatically detect, list and join hosts on LAN without the user knowing their ip/port (if the host is set to visible).

## Included Projects:
The solution includes three projects:
- Client-Demo
- Host-Demo
- Network-Module

See the Demo projects how to set up Host and Client Application.
The demo projects is only there to test/showcase the Network-Module and is not needed for the module to be used.
