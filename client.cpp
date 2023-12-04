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

void user_message(const char *msg, const char *detail) {
    fputs(msg, stderr);
    fputs(": ", stderr);
    fputs(detail, stderr);
    fputc('\n', stderr);
    exit(1);
}

void system_message(const char *msg) {
    perror(msg);
    exit(1);
}

string read_file(const string file_name) {
    const string path = "/home/mohamed/CSED_25/Year_3/Network/Assignments/Assignment_1/";
    ifstream file(path + file_name); // creating ifstream object and associate it with the file to be read
    if (!file.is_open())
        cerr << "Unable to open the file" << endl;
    stringstream stream;
    stream << file.rdbuf(); // extract the entire file content and pass it to the stringstream object
    string file_content = stream.str();
    file.close();
    return file_content;
}

// get line by line from th einput file
ifstream *file_ptr = nullptr;

string get_line(const string file_name) {
    if (!file_ptr) {
        // Open the file if it's not already open
        const string path = "/home/mohamed/CSED_25/Year_3/Network/Assignments/Assignment_1/";
        file_ptr = new ifstream(path + file_name);
        if (!file_ptr->is_open()) {
            cerr << "Unable to open the file" << endl;
            return "";
        }
    }
    if (file_ptr->eof())
        return ".";
    string line;
    getline(*file_ptr, line);
    if (!*file_ptr) {
        file_ptr->close();
        delete file_ptr;
        file_ptr = nullptr;
        return "";
    }

    return line;
}

int calc_content_size(string method, string file_name) {
    const unsigned int cr_lf = 4; // number of bytes of \r\n
    const unsigned int blank = 2; // number of bytes of " "
    const unsigned int header = 16; // number of bytes of content-length
    const unsigned int http_version = 8;
    unsigned int content_size_num = (int) method.size() + (int) file_name.size() + cr_lf * 3 + header + http_version + blank;
    if (method == "POST") {
        const unsigned int body_length = read_file(file_name).size();
        content_size_num += body_length;
    }
    int number_of_digits_in_body = (int) to_string(content_size_num).size();
    int number_of_digit_summation = (int) to_string(number_of_digits_in_body + content_size_num).size();
    content_size_num = content_size_num + number_of_digit_summation; // consider the length of the length itself
    return content_size_num;
}

string build_http_request(string input_line) {
    string method = "";
    string file_name = "";
    string host_name = "";
    string port_number = "";
    istringstream iss(input_line);// parse string on the space
    iss >> method >> file_name >> host_name >> port_number;

    string request;
    if (method == "client_get") {
            unsigned int content_size_num = calc_content_size("GET", file_name);
        request = "GET";
        request += " " + file_name + " " + "HTTP/1.1" + "\\r\\n";
        request += "Content-Length: ";
        request += to_string(content_size_num) + "\\r\\n" + "\\r\\n";

    } else if (method == "client_post") {
        unsigned int content_size_num = calc_content_size("POST", file_name);
        request = "POST";
        request += " " + file_name + " " + "HTTP/1.1" + "\\r\\n";
        request += "Content-Length: ";
        request += to_string(content_size_num) + "\\r\\n" + "\\r\\n";
        request += read_file(file_name);
    } else {
        request = "INVALID METHOD -- build_http_request";
    }
    return request;
}

int extract_response_content_length(string http_response) {
    int length = 0;
    int position = http_response.find("Content-Length: ");
    // assuming that the message will not be separated between content-length and the length after it
    while ((position != string::npos) && (position < (int) http_response.size()) && (http_response[position] != '\\')) {
        if (isdigit(http_response[position])) {
            string digit = "";
            digit += http_response[position];
            length = length * 10 + stoi(digit);
        }
        position++;
    }
    return length;
}

void handle_response(string http_request, string http_response) {
    if (http_request[0] == 'G') {
        int position_last_occ_cr_lf = http_response.rfind("\\r\\n");
        int first_pos_in_body = position_last_occ_cr_lf + 4;
        string content_body = http_response.substr(first_pos_in_body);

        // save the file in client's repo
        string method = "";
        string file_name = "";
        istringstream iss(http_request);// parse string on the space
        iss >> method >> file_name;
        ofstream get_file;
        get_file.open("/home/mohamed/CSED_25/Year_3/Network/Assignments/Assignment_1/" + file_name);

        if (!get_file.is_open()) {
            cerr << "Error opening file!" << endl;
        }
        get_file << content_body;
        get_file.close();
    }
    // in case of POST I shouldn't do anything
}

int main(int argc, char *argv[]) {
//    string line;
//    while(true){
//        line = get_line("input.txt");
//        if(line ==".") break;
//        cout << line << " ";
//    }

    // the port number is optional
    if (argc < 2 || argc > 3)
        user_message("Parameter(s)", "server_ipv4 - [port_number]");
    char *servIP = argv[1];
    in_port_t servPort = (argc == 3) ? atoi(argv[2]) : 80;

    // creating a reliable, stream socket using TCP using IPv4 as address family
    // returning socket file descriptor (FD) of the socket
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) system_message("Socket creation failed");

    // build the server address struct
    struct sockaddr_in server_address;
    //ensure that any part of the structure that we do not explicitly set is zero
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    // converting the string representation of server IP to binary representation and store it in the sin_addr.s_addr
    int server_address_convert_status = inet_pton(AF_INET, servIP, &server_address.sin_addr.s_addr);
    if (server_address_convert_status == 0) user_message("inet_pton() failed", "invalid IPv4 structure");
    else if (server_address_convert_status < 0) system_message("server address conversion failed");
    //(host to network short) ensures that the binary value of server port is formatted as required by the API
    server_address.sin_port = htons(servPort);


    // Establish the connection from the specified socket to the servers' socket
    // identified by the address and port number in server_address struct
    if (connect(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
        system_message("connection failed");
    //
    string http_request;
    while (true) {
        //take a single request from the input file
        http_request = build_http_request(get_line("input.txt")); // get the request that will be sent from input file
        if (http_request == ".") break; // the file is ended

        size_t request_length = (size_t) http_request.size(); // Determine input length
        //sending the request over the socket
        //send : takes pointer to char array
        ssize_t success_send_bytes = send(sock, http_request.c_str(), request_length, 0);
        if (success_send_bytes < 0)
            system_message("send() failed");
        else if (success_send_bytes != request_length)
            user_message("send()", "sent unexpected number of bytes");

        //receiving the response from the server (waiting)
        unsigned int total_received_bytes = 0;
        int response_content_length = INT32_MAX; // to avoid comparision error in http_response.size() >= response_content_length
        string http_response;
        /////////////////////////////////////////////////// fputs("Received: ", stdout);
        while (true) {
            char buffer[BUFSIZE];
            // receive up to the buffer size (minus 1 to leave space for a null terminator) bytes from the sender
            // blocking call
            ssize_t number_of_bytes = recv(sock, buffer, BUFSIZE - 1, 0);

            if (number_of_bytes < 0) {
                system_message("recv() failed");
            } else if (number_of_bytes == 0) {
                break; // Connection closed
            }
            http_response += buffer;

            total_received_bytes += number_of_bytes; // use it for validation
            buffer[number_of_bytes] = '\0'; // Terminate the string with null char

            int response_content_length_this_packet = extract_response_content_length(
                    http_response); // int not unsigned as I return -1 when the content-length header is not received yet
            if (response_content_length_this_packet != 0) response_content_length = response_content_length_this_packet;
            if ((int) http_response.size() >= response_content_length) {
                cout << http_response << endl;
                break;
            }
        }
        if (total_received_bytes == (int) http_response.size()) cout << "recieved the entire message";
        else cout << "recieved part of the message";
        handle_response(http_request, http_response);
    }
    close(sock);
    exit(0);
    return 0;
}
