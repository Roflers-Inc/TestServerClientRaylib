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
#undef PlaySound
#endif
#include <string>
#include <fstream>
#include <thread>
#include <deque>

std::deque<std::string> messageList(20);

struct Point {
	double x = 0, y = 0;
} mainPoint;

SOCKET mainSocket, messageSocket;
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

void createMessageSocket() {
	messageSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (messageSocket < 0) {
		std::cout << "Error creating message socket; errno: " << errno << std::endl;
		exit(-1);
	}

	servAddr.sin_family = AF_INET;
	servAddr.sin_port = serverInfo.port;
	if (inet_pton(AF_INET, serverInfo.ip.c_str(), &servAddr.sin_addr) <= 0) {
		std::cout << "Wrong address; errno: " << errno << std::endl;
		exit(-1);
	}
	if (connect(messageSocket, (sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
		std::cout << "Failed to connect to server with address: " << serverInfo.ip << "; errno: " << errno << std::endl;
		exit(-1);
	}

}

void receivePacket() {
	while (GetWindowHandle() && mainSocket > 0) {
		auto bytesRead = recv(mainSocket, (char*)&mainPoint, sizeof(mainPoint), 0);
		if (bytesRead < 0) {
			std::cout << "Connection closed with error bytes readen; errno: " << errno << std::endl;
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

void sendMyMessage(char* msg) {
	send(messageSocket, msg, sizeof(msg), 0);
}

void messageThread() {
	char message[30];
	for (std::string& s : messageList) {
		recv(messageSocket, message, sizeof(message), 0);
		s = message;
	}
	while (GetWindowHandle() && messageSocket > 0) {
		if (recv(messageSocket, message, sizeof(message), 0) < 0) {
			std::cout << "Error receiving message" << std::endl;
		}
		messageList.pop_front();
		messageList.push_back(std::string(message));
	}
}

void setfps() {
	std::ifstream in("fps.txt");
	if (!in.is_open()) {
		in.close();
		std::ofstream out("fps.txt");
		SetTargetFPS(60);
		out << 60;
		out.close();
	}
	else {
		int i;
		in >> i;
		SetTargetFPS(i);
	}
}

int main(int argc, char** argv) {
	WSAinit();
	loadcfg(argc, argv);
	serverInfo.port = htons(serverInfo.port);
	createConnection();
	createMessageSocket();
	InitWindow(1000, 700, "Window Title");
	std::thread receiverThread(receivePacket);
	std::thread messageReceiverThread(messageThread);
	receiverThread.detach();
	messageReceiverThread.detach();
	setfps();
	int letterCount = 0;

	Rectangle textBox = { 1000 / 2.0f - 100, 180, 225, 50 };
	bool mouseOnText = false;

	int framesCounter = 0;
	char name[30] = "\0";

	while (!WindowShouldClose()) {

		if (CheckCollisionPointRec(GetMousePosition(), textBox)) mouseOnText = true;
		else mouseOnText = false;

		if (mouseOnText)
		{
			// Set the window's cursor to the I-Beam
			SetMouseCursor(MOUSE_CURSOR_IBEAM);

			// Get char pressed (unicode character) on the queue
			int key = GetCharPressed();

			// Check if more characters have been pressed on the same frame
			while (key > 0)
			{
				// NOTE: Only allow keys in range [32..125]
				if ((key >= 32) && (key <= 125) && (letterCount < 29))
				{
					name[letterCount] = (char)key;
					name[letterCount + 1] = '\0'; // Add null terminator at the end of the string.
					letterCount++;
				}

				key = GetCharPressed();  // Check next character in the queue
			}

			if (IsKeyPressed(KEY_BACKSPACE))
			{
				letterCount--;
				if (letterCount < 0) letterCount = 0;
				name[letterCount] = '\0';
			}
			if (IsKeyPressed(KEY_ENTER) && letterCount > 0) {
				sendMyMessage(name);
				letterCount = 0;
				name[0] = '\0';
			}
		}
		else SetMouseCursor(MOUSE_CURSOR_DEFAULT);

		if (mouseOnText) framesCounter++;
		else framesCounter = 0;

		BeginDrawing();
		ClearBackground(WHITE);
		DrawCircle(mainPoint.x, mainPoint.y, 5, BLACK);

		DrawRectangleRec(textBox, LIGHTGRAY);
		if (mouseOnText) DrawRectangleLines((int)textBox.x, (int)textBox.y, (int)textBox.width, (int)textBox.height, RED);
		else DrawRectangleLines((int)textBox.x, (int)textBox.y, (int)textBox.width, (int)textBox.height, DARKGRAY);

		DrawText(name, (int)textBox.x + 5, (int)textBox.y + 8, 40, MAROON);

		if (mouseOnText)
		{
			if (letterCount < 29)
			{
				// Draw blinking underscore char
				if (((framesCounter / 60) % 2) == 0) DrawText("_", (int)textBox.x + 8 + MeasureText(name, 40), (int)textBox.y + 12, 40, MAROON);
			}
		}

		int c = 0;
		for (std::string i : messageList) {
			DrawText(i.c_str(), 5, 5 + c * 20, 20, BLACK);
			c++;
		}

		EndDrawing();
	}
	CloseWindow();
}