#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>

using namespace std;
int const BUFSIZE = 1024;

void user_message(const char *msg, const char *detail)
{
    fputs(msg, stderr);
    fputs(": ", stderr);
    fputs(detail, stderr);
    fputc('\n', stderr);
    exit(1);
}

void system_message(const char *msg)
{
    perror(msg);
    exit(1);
}

string read_file(const string file_name)
{
    // const string path = "/home/mohamed/CSED_25/Year_3/Network/Assignments/Assignment_1/";
    string file_path = "./client_directory/" + file_name;
    cout << "file_path: " << file_path << endl;
    ifstream file(file_path, ios::binary);
    if (!file.is_open())
        cerr << "Unable to open the file" << endl;
    file.seekg(0, ios::end);
    streampos fileSize = file.tellg();
    file.seekg(0, ios::beg);
    vector<char> buffer(fileSize);
    file.read(buffer.data(), fileSize);
    file.close();
    return string(buffer.data(), fileSize);
}

// get line by line from th einput file
ifstream *file_ptr = nullptr;

string get_line(const string file_name)
{
    if (!file_ptr)
    {
        // Open the file if it's not already open
        // const string path = "";
        file_ptr = new ifstream(file_name);
        if (!file_ptr->is_open())
        {
            cerr << "Unable to open the file" << endl;
            return "";
        }
    }
    if (file_ptr->eof())
        return ".";
    string line;
    getline(*file_ptr, line);
    if (!*file_ptr)
    {
        file_ptr->close();
        delete file_ptr;
        file_ptr = nullptr;
        return "";
    }

    return line;
}

int calc_content_size(string method, string file_name)
{
    const unsigned int cr_lf = 4;   // number of bytes of \r\n
    const unsigned int blank = 2;   // number of bytes of " "
    const unsigned int header = 16; // number of bytes of content-length
    const unsigned int http_version = 8;
    unsigned int content_size_num = (int)method.size() + (int)file_name.size() + cr_lf * 3 + header + http_version + blank;
    if (method == "POST")
    {
        const unsigned int body_length = read_file(file_name).size();
        content_size_num += body_length;
    }
    int number_of_digits_in_body = (int)to_string(content_size_num).size();
    int number_of_digit_summation = (int)to_string(number_of_digits_in_body + content_size_num).size();
    content_size_num = content_size_num + number_of_digit_summation; // consider the length of the length itself
    return content_size_num;
}

string build_http_request(string input_line)
{
    string method = "";
    string file_name = "";
    istringstream iss(input_line); // parse string on the space
    iss >> method >> file_name;

    string request;
    if (method == "client_get")
    {
        unsigned int content_size_num = calc_content_size("GET", file_name);
        request = "GET";
        request += " /" + file_name + " " + "HTTP/1.1" + "\\r\\n";
        request += "Content-Length: ";
        request += to_string(content_size_num) + "\\r\\n" + "\\r\\n";
    }
    else if (method == "client_post")
    {
        unsigned int content_size_num = calc_content_size("POST", file_name);
        request = "POST";
        request += " /" + file_name + " " + "HTTP/1.1" + "\\r\\n";
        request += "Content-Length: ";
        request += to_string(content_size_num) + "\\r\\n\\r\\n";
        request += read_file(file_name);
    }
    else
    {
        request = ".";
    }
    printf("Request: %s\n", request.c_str());
    return request;
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

void handle_response(string http_request, string http_response)
{
    int non_found = http_response.find("404 Not Found");
    if(non_found != string::npos){
        cout << http_response << endl;
        return;
    }
    if (http_request[0] == 'G')
    {

        // cout <<"response : "<< http_response.size()<<endl;
        size_t bodyStart = http_response.find("\r\n\r\n") + 4;
        string content_body = http_response.substr(bodyStart);
        // save the file in client's repo
        string method = "";
        string file_name = "";
        istringstream iss(http_request); // parse string on the space
        iss >> method >> file_name;
        file_name = "./client_directory/" + file_name;
        // cout<<"content size ::   "<<content_body.size();
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

        // get_file.close();
    }
    // in case of POST I shouldn't do anything
}

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3)
        user_message("Parameter(s)", "server_ipv4 - [port_number]");
    char *servIP = argv[1];
    in_port_t servPort = (argc == 3) ? atoi(argv[2]) : 80;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        system_message("Socket creation failed");

    // build the server address struct
    struct sockaddr_in server_address;
    // ensure that any part of the structure that we do not explicitly set is zero
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    // converting the string representation of server IP to binary representation and store it in the sin_addr.s_addr
    int server_address_convert_status = inet_pton(AF_INET, servIP, &server_address.sin_addr.s_addr);
    if (server_address_convert_status == 0)
        user_message("inet_pton() failed", "invalid IPv4 structure");
    else if (server_address_convert_status < 0)
        system_message("server address conversion failed");
    //(host to network short) ensures that the binary value of server port is formatted as required by the API
    server_address.sin_port = htons(servPort);

    // Establish the connection from the specified socket to the servers' socket
    // identified by the address and port number in server_address struct
    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        system_message("connection failed");
    //
    string http_request;
    while (true)
    {
        // take a single request from the input file
        http_request = build_http_request(get_line("./client_directory/requests.txt")); // get the request that will be sent from input file
        if (http_request == ".")
            break; // the file is ended

        size_t request_length = (size_t)http_request.size(); // Determine input length
        // sending the request over the socket
        // send : takes pointer to char array
        ssize_t success_send_bytes = send(sock, http_request.c_str(), request_length, 0);
        if (success_send_bytes < 0)
            system_message("send() failed");
        else if (success_send_bytes != request_length)
            user_message("send()", "sent unexpected number of bytes");

        // receiving the response from the server (waiting)
        unsigned int total_received_bytes = 0;
        int response_content_length = INT32_MAX; // to avoid comparision error in http_response.size() >= response_content_length
        string http_response;
        while (true)
        {
            char buffer[BUFSIZE];
            ssize_t number_of_bytes = recv(sock, buffer, BUFSIZE - 1, 0);
            buffer[number_of_bytes] = '\0'; //added to be a string
            //printf("buffer : %s", buffer);
           // cout << "buffer : " << buffer << endl;

            if (number_of_bytes < 0)
            {
                system_message("recv() failed");
                // Handle the error (e.g., close the connection or retry)
                break;
            }
            else if (number_of_bytes == 0)
            {
                break;
            }
            http_response.append(buffer, number_of_bytes);
            total_received_bytes += number_of_bytes;

            int response_content_length_this_packet = extract_response_content_length(http_response);
            //cout << "response_content_length_this_packet: " << response_content_length_this_packet << endl;

            if (response_content_length_this_packet != -1)
            {
                response_content_length = response_content_length_this_packet;
                // break;
            }
            if (response_content_length != -1 && total_received_bytes >= response_content_length)
            {
               cout << "Received : \n"<< http_response << endl;
                break;
            }
        }

        handle_response(http_request, http_response);
    }
    close(sock);
    exit(0);
    return 0;
}
