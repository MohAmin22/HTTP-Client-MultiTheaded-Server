#include <iostream>
#include <cstring>
#include <thread>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fstream>
#include <sstream>

const int MAX_CONNECTIONS = 10;
using namespace std;

void handleGetRequest(int clientSocket, const char *filename)
{
    // Build the full path to the requested file
    std::string filePath = "./server_directory/" + std::string(filename);

    // Open the requested file
    std::ifstream file(filePath, std::ios::binary);

    if (!file.is_open())
    {
        // If the file cannot be opened, send a 404 Not Found response
        cout << "File not found" << std::endl;
        string response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found\r\n";
        send(clientSocket, response.c_str(), response.size(), 0);
        close(clientSocket);
        return;
    }

    // Read the content of the file
    file.seekg(0, ios::end);
    streampos fileSize = file.tellg();
    file.seekg(0, ios::beg);
    // Read the entire file into a vector
    vector<char> buffer(fileSize);
    file.read(buffer.data(), fileSize);
    printf("File size: %ld bytes\n", buffer.size());
    printf("File content:\n%s\n", buffer.data());

    std::ostringstream getResponse;
    
    //if file is html
    if (strcmp(filename + strlen(filename) - 4, "html") == 0)
    {
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    //if file is jpg
    else if (strcmp(filename + strlen(filename) - 3, "jpg") == 0)
    {
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    //if file is png
    else if (strcmp(filename + strlen(filename) - 3, "png") == 0)
    {
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    else if (strcmp(filename + strlen(filename) - 3, "txt") == 0)
    {
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    getResponse.write(buffer.data(), buffer.size());
    // Send the response
    send(clientSocket, getResponse.str().c_str(), getResponse.str().size(), 0);
    file.close();
    std::cout << "File sent successfully" << std::endl;
}

void handlePostRequest(int clientSocket, const char *requestBody)
{
    std::string bodyStr(requestBody);
    size_t nameStart = bodyStr.find("filename=") + 10;
    size_t nameEnd = bodyStr.find("\"", nameStart);
    std::string fileName = bodyStr.substr(nameStart, nameEnd - nameStart);
    size_t extStart = fileName.rfind('.') + 1;
    std::string fileExt = fileName.substr(extStart);
    const char *successResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nFile saved successfully\r\n";
    send(clientSocket, successResponse, strlen(successResponse), 0);
    ;
    size_t contentStart = bodyStr.find("\r\n\r\n") + 4;
    std::string fileContent = bodyStr.substr(contentStart);

    // Build the full path to save the file
    std::string filePath = "./server_directory/" + fileName;

    // Open the file for writing
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open())
    {
        // If the file cannot be opened, send a 500 Internal Server Error response
        const char *errorResponse = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nError saving file\r\n";
        send(clientSocket, errorResponse, strlen(errorResponse), 0);
        return;
    }
    // Write the content to the file
    outFile << fileContent;
    outFile.close();
    // Send a success response
}

void clientHandler(int clientSocket)
{
    while (true)
    {
        // Receive the HTTP request
        const int bufferSize = 1024;
        char buffer[bufferSize];
        int bytesRead = recv(clientSocket, buffer, bufferSize - 1, 0);
        if (bytesRead <= 0)
        {
            // Connection closed or error
            break;
        }

        buffer[bytesRead] = '\0'; // Null-terminate the received data
        printf("Received message:\n%s\n", buffer);

        // Check if it's a GET or POST request
        if (strncmp(buffer, "GET", 3) == 0)
        {
            char filename[bufferSize];
            sscanf(buffer, "GET /%s HTTP/1.1", filename);
            printf("Filename: %s\n", filename);
            handleGetRequest(clientSocket, filename);
        }
        else if (strncmp(buffer, "POST", 4) == 0)
        {

            //get the file name
            char filename[bufferSize];
            sscanf(buffer, "POST /%s HTTP/1.1", filename);
            printf("Filename: %s\n", filename);

            // get the file length
            char *contentLength = strstr(buffer, "Content-Length: ");
            int fileLength = atoi(contentLength + 16);
            printf("File length: %d\n", fileLength);

            // Read the request body
            

            
            // Extract the request body from the POST request
            const char *bodyStart = strstr(buffer, "\r\n\r\n");

            if (bodyStart != nullptr)
            {
                bodyStart += 4; // Move past the \r\n\r\n
                handlePostRequest(clientSocket, bodyStart);
            }
        }
    }
    printf("Closing connection\n");
    close(clientSocket);
}

int main(int argc, char **argv)
{

    if (argc != 2)
    {
        perror("Usage: ./server <port>");
        exit(EXIT_FAILURE);
    }
    int PORT = atoi(argv[1]);
    // Create socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Bind socket to a port
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("Error binding to port");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(serverSocket, MAX_CONNECTIONS) < 0)
    {
        perror("Error listening for connections");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // Accept and handle incoming connections

    // Accept and handle incoming connections
    while (true)
    {
        sockaddr_in clientAddress{};
        socklen_t clientAddrSize = sizeof(clientAddress);

        // Accept a connection
        int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddrSize);
        if (clientSocket == -1)
        {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }
        std::cout << "Client with address  " << clientAddress.sin_port << "  connected" << std::endl;
        std::thread clientThread(clientHandler, clientSocket);
        clientThread.detach();
        // Create a thread for each client connection
    }

    // Close the server socket (unreachable in this example)
    close(serverSocket);

    return 0;
}
