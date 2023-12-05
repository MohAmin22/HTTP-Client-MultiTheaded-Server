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
const int BUFSIZE = 1024;
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

    // if file is html
    if (strcmp(filename + strlen(filename) - 4, "html") == 0)
    {
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    // if file is jpg
    else if (strcmp(filename + strlen(filename) - 3, "jpg") == 0)
    {
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    // if file is png
    else if (strcmp(filename + strlen(filename) - 3, "png") == 0)
    {
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    else if (strcmp(filename + strlen(filename) - 3, "txt") == 0)
    {
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    else
    {
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }

    // Add the file content to the response
    getResponse.write(buffer.data(), buffer.size());
    // Send the response
    send(clientSocket, getResponse.str().c_str(), getResponse.str().size(), 0);
    file.close();
    std::cout << "File sent successfully" << std::endl;
}

void handlePostRequest(int clientSocket, string requestBody)
{
    string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 78 \r\n\r\nFile saved\r\n";
    send(clientSocket, response.c_str(), response.size(), 0);
    char filename[BUFSIZE];
    sscanf(requestBody.c_str(), "POST /%s HTTP/1.1", filename);
    printf("Filename: %s\n", filename);
    cout << "requestBody: " << requestBody << endl;
    cout << "requestBody.size(): " << requestBody.size() << endl;
    size_t bodyStart = requestBody.find("\\r\\n\\r\\n") + 8;
    string content_body = requestBody.substr(bodyStart);
    cout << "content_body: " << content_body << endl;
    cout << "content_body.size(): " << content_body.size() << endl;
    string file_name(filename);
    file_name = "./server_directory/" + file_name;
    ofstream get_file(file_name, ios::binary);
    // get_file.open("./client_directory/" + file_name);
    if (!get_file.is_open())
    {
        cerr << "Error opening file!" << endl;
    }
    else
    {
        get_file << content_body;
        get_file.close();
    }
    // Send a success response
}



int extract_response_content_length(string http_response)
{
    size_t content_length_start = http_response.find("Content-Length: ");
    if (content_length_start == string::npos)
    {
        return -1;
    }
    content_length_start += 16;
    size_t content_length_end = http_response.find("\r\n", content_length_start);
    string content_length_str = http_response.substr(content_length_start, content_length_end - content_length_start);
    return stoi(content_length_str);
}

void clientHandler(int clientSocket)
{
    unsigned int total_received_bytes = 0;
    int response_content_length = INT32_MAX; // to avoid comparision error in http_response.size() >= response_content_length
    string http_response;
    /////////////////////////////////////////////////// fputs("Received: ", stdout);
    while (true)
    {
        char buffer[BUFSIZE];
        ssize_t number_of_bytes = recv(clientSocket, buffer, BUFSIZE - 1, 0);
        // cout << "buffer : " << buffer << endl;

        if (number_of_bytes < 0)
        {
            // Handle the error (e.g., close the connection or retry)
            break;
        }
        else if (number_of_bytes == 0)
        {
            break;
        }
        if (strncmp(buffer, "GET", 3) == 0)
        {
            char filename[BUFSIZE];
            sscanf(buffer, "GET /%s HTTP/1.1", filename);
            printf("Filename: %s\n", filename);
            handleGetRequest(clientSocket, filename);
            continue;
        }
        http_response.append(buffer, number_of_bytes);
        total_received_bytes += number_of_bytes;

        int response_content_length_this_packet = extract_response_content_length(http_response);
        // cout << "response_content_length_this_packet: " << response_content_length_this_packet << endl;

        if (response_content_length_this_packet != -1)
        {
            response_content_length = response_content_length_this_packet;
            // break;
        }
        if (response_content_length != -1 && total_received_bytes >= response_content_length)
        {
            handlePostRequest(clientSocket, http_response);
            continue;
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
