#include <iostream>
#include <cstring>
#include <thread>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <mutex>

using namespace std;
using namespace std::chrono;

const int MAX_CONNECTIONS = 10;
const int MAX_TIME_OUT = 1000; // 1000ms = 1 second
const int BUFSIZE = 1024;

int user_count = 0;
int time_out = 0;
mutex user_mutex;
mutex time_out_mutex;


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
        string response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: " + to_string(86) + "\r\nFile not found\r\n";
        send(clientSocket, response.c_str(), response.size(), 0);
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
    std::ostringstream getResponse;
    if (strcmp(filename + strlen(filename) - 4, "html") == 0){
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    else if (strcmp(filename + strlen(filename) - 3, "jpg") == 0){
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    else if (strcmp(filename + strlen(filename) - 3, "png") == 0){
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    else if (strcmp(filename + strlen(filename) - 3, "txt") == 0){
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " << buffer.size() << "\r\n\r\n";
    }
    else{
        getResponse << "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " << buffer.size()<< "\r\n\r\n";
    }
    getResponse.write(buffer.data(), buffer.size());
    send(clientSocket, getResponse.str().c_str(), getResponse.str().size(), 0);
    file.close();
}

void handlePostRequest(int clientSocket, string requestBody)
{
    string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 78 \r\n\r\nFile saved\r\n";
    send(clientSocket, response.c_str(), response.size(), 0);
    char filename[BUFSIZE];
    sscanf(requestBody.c_str(), "POST /%s HTTP/1.1", filename);
    printf("Filename: %s\n", filename);
    size_t bodyStart = requestBody.find("\\r\\n\\r\\n") + 8;
    string content_body = requestBody.substr(bodyStart);
    string file_name(filename);
    file_name = "./server_directory/" + file_name;
    ofstream get_file(file_name, ios::binary);
    if (!get_file.is_open())
    {
        cerr << "Error opening file!" << endl;
    }
    else
    {
        get_file << content_body;
        get_file.close();
    }
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

    cout << "user count : " << user_count << endl;
    // set timeout
    chrono::high_resolution_clock::time_point start_time_for_this_user = chrono::high_resolution_clock::now();
    time_out_mutex.lock();
    int time_out_for_this_user_milli = time_out;
    time_out_mutex.unlock();
    high_resolution_clock::time_point time_end_milli = start_time_for_this_user + milliseconds(time_out_for_this_user_milli);
    duration<double> timeSinceEpoch_end = time_end_milli.time_since_epoch();
    double milli_till_time_out = duration_cast<milliseconds>(timeSinceEpoch_end).count();
    //
    // cout << "time out for this user : " << time_out_for_this_user_milli << endl;
    unsigned int total_received_bytes = 0;
    int request_content_length = -1; // to avoid comparision error in http_request.size() >= request_content_length
    string http_request;
    while (true)
    {
        chrono::high_resolution_clock::time_point current_time_for_this_user = chrono::high_resolution_clock::now();
        duration<double> timeSinceEpoch_current = current_time_for_this_user.time_since_epoch();
        double milli_current = duration_cast<milliseconds>(timeSinceEpoch_current).count();
        // check for time_out
        if (milli_current >= milli_till_time_out && request_content_length == -1)
        {
            cout << "time caused timeout (diff) : " << milli_current - milli_till_time_out << endl;
            cout << "time out in socket FD : " << clientSocket << endl;
            printf("Closing connection\n");
            close(clientSocket);
            user_mutex.lock();
            user_count--;
            user_mutex.unlock();
            return;
        }
        char buffer[BUFSIZE];
        ssize_t number_of_bytes = recv(clientSocket, buffer, BUFSIZE - 1, 0);
        if (number_of_bytes < 0)
        {
            break;
        }
        else if (number_of_bytes == 0)
        {
            printf("number_of_bytes == 0\n");
            break;
        }
        if (strncmp(buffer, "GET", 3) == 0 && request_content_length == -1)
        {
            char filename[BUFSIZE];
            sscanf(buffer, "GET /%s HTTP/1.1", filename);
            printf("Filename: %s\n", filename);
            handleGetRequest(clientSocket, filename);
            continue;
        }
        http_request.append(buffer, number_of_bytes);
        total_received_bytes += number_of_bytes;

        int request_content_length_this_packet = extract_response_content_length(http_request);
        // cout << "response_content_length_this_packet: " << response_content_length_this_packet << endl;

        if (request_content_length_this_packet != -1)
        {
            request_content_length = request_content_length_this_packet;
            // break;
        }
        // cout<<"remaining bytes : "<<request_content_length - total_received_bytes<<endl;
        if (request_content_length != -1 && total_received_bytes >= request_content_length)
        {
            handlePostRequest(clientSocket, http_request);
            request_content_length = -1;
            http_request = "";
            total_received_bytes = 0;
            continue;
        }
    }
    printf("Closing connection\n");
    close(clientSocket);
    user_mutex.lock();
    user_count--;
    user_mutex.unlock();
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
        // Increase the number of current user (helps us to calculate the timeout)
        user_count++;
        std::cout << "Client with address  " << clientAddress.sin_port << "  connected" << std::endl;
        std::thread clientThread(clientHandler, clientSocket);
        clientThread.detach();
        // Create a thread for each client connection
        //adjust the timeout
        time_out_mutex.lock();
        time_out = MAX_TIME_OUT-user_count*100;
        time_out_mutex.unlock();
    }

    // Close the server socket (unreachable in this example)
    close(serverSocket);

    return 0;
}
