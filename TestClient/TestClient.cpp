#include <raylib.h>
#include <iostream>
#if defined(_WIN32)           
#define NOGDI             // All GDI defines and routines
#define NOUSER            // All USER defines and routines
#endif

#include <WS2tcpip.h> // or any library that uses Windows.h

#if defined(_WIN32)           // raylib uses these names as function parameters
#undef near
#undef far
#endif
#include <string>
#include <fstream>
#include <thread>

struct Point {
	double x, y;
} mainPoint;

SOCKET mainSocket, connection;
sockaddr_in servAddr;
struct {
	std::string ip = "127.0.0.1";
	int port = 15212;
} serverInfo;

void WSAinit() {
	WSADATA ws;
	WSAStartup(MAKEWORD(2, 2), &ws);
}

void createConnection() {
	mainSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (mainSocket < 0) {
		std::cout << "Error creating main socket; errno: " << errno << std::endl;
		exit(-1);
	}

	servAddr.sin_family = AF_INET;
	servAddr.sin_port = serverInfo.port;
	if (inet_pton(AF_INET, serverInfo.ip.c_str(), &servAddr.sin_addr) <= 0) {
		std::cout << "Wrong address; errno: " << errno << std::endl;
		exit(-1);
	}
	if (connect(mainSocket, (sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
		std::cout << "Failed to connect to server with address: " << serverInfo.ip << "; errno: " << errno << std::endl;
		exit(-1);
	}
}

void receivePacket() {
	while (GetWindowHandle() && send(mainSocket, NULL, 0, 0) != SOCKET_ERROR) {
		auto bytesRead = recv(mainSocket, (char*)&mainPoint, sizeof(mainPoint), 0);
		if (bytesRead < 0) {
			std::cout << "Connection closed with 0 bytes readen; errno: " << errno << std::endl; 
			break;
		}
	}
	closesocket(mainSocket);
	return;
}

void loadcfg(int argc, char** argv) {
	std::ifstream cfg("server.txt");
	if (cfg.is_open()) {
		char buffer[31];
		cfg.getline(buffer, 30);
		serverInfo.ip = buffer;
		cfg >> serverInfo.port;
		cfg.close();
	}
	std::ofstream outcfg("server.txt");
	if (argc > 1) {
		serverInfo.ip = argv[1];
		if (argc > 2) {
			serverInfo.port = std::stoi(argv[2]);
		}
	}
	outcfg << serverInfo.ip << std::endl;
	outcfg << serverInfo.port;
	outcfg.close();
}

int main(int argc, char** argv) {
	WSAinit();
	loadcfg(argc, argv);
	serverInfo.port = htons(serverInfo.port);
	createConnection();
	InitWindow(1000, 700, "Window Title");
	std::thread receiverThread(receivePacket);
	receiverThread.detach();
	SetTargetFPS(30);
	while (!WindowShouldClose()) {
		BeginDrawing();
		ClearBackground(WHITE);
		DrawCircle(mainPoint.x, mainPoint.y, 5, BLACK);
		EndDrawing();
	}
	CloseWindow();

}