#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <json-c/json.h>

using namespace std;

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024



class File {
private:
    int file_size;
    string md5sum;
    string file_name;

public:
    void set_file_size(int size) {
        file_size = size;
    }
    void set_file_name(string name) {
        file_name = name;
    }

    void calculate_md5() {
        unsigned char c[MD5_DIGEST_LENGTH];
        int i;
        FILE *inFile = fopen(file_name.c_str(), "rb");
        MD5_CTX mdContext;
        int bytes;
        unsigned char data[1024];

        if (inFile == NULL) {
            cout << "File " << file_name << " can't be opened." << endl;
            return;
        }

        MD5_Init(&mdContext);
        while ((bytes = fread(data, 1, 1024, inFile)) != 0) {
            MD5_Update(&mdContext, data, bytes);
        }

        MD5_Final(c, &mdContext);

        for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
            char tmp[3];
            sprintf(tmp, "%02x", c[i]);
            md5sum += tmp;
        }

        fclose(inFile);
    }

    int get_file_size() const {
        return file_size;
    }

    string get_md5sum() const {
        return md5sum;
    }

    string get_file_name() const {
        return file_name;
    }
};

class Client {
public:
    Client();
    ~Client();
    void send_request(const char* request);
    void receive_file_info(json_object* file_info);
    void receive_file(int file_size);
    void connect_to_server();
    void run();

private:
    int client_socket;
    struct sockaddr_in server_address;
    File file;
};

Client::Client() {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &server_address.sin_addr) != 1) {
        perror("Failed to convert server address");
        exit(EXIT_FAILURE);
    }
}

Client::~Client() {
    close(client_socket);
}

void Client::send_request(const char* request) {
    char message[BUFFER_SIZE];
    sprintf(message, "%s", request);
    int bytes_sent = send(client_socket, message, strlen(message), 0);
    if (bytes_sent == -1) {
        perror("send");
        exit(EXIT_FAILURE); 
    }
}

void Client::receive_file_info(json_object* file_info) {
    const char *file_name_ptr = json_object_get_string(json_object_object_get(file_info, "filename"));
    int file_size = json_object_get_int(json_object_object_get(file_info, "file_size"));
    const char *md5_str_ptr = json_object_get_string(json_object_object_get(file_info, "md5_str"));
    printf("Info file: \n");
    printf("  File name: %s\n", file_name_ptr);
    printf("  File size: %d\n", file_size);
    printf("  MD5sum: %s\n",md5_str_ptr);
}

void Client::receive_file(int file_size) {
    FILE *file_fd = fopen("received_file.txt","wb");
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    long int total_bytes_read = 0;

    while (total_bytes_read < file_size) {
        int bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_read == -1) {
            perror("Error receiving file data");
            exit(EXIT_FAILURE);
        }
        int bytes_written = fwrite(buffer, sizeof(char), bytes_read, file_fd);
        if (bytes_written < bytes_read) {
            perror("Error writing file");
            exit(EXIT_FAILURE);
        }
        total_bytes_read += bytes_read;
    }
    printf("receive %ld bytes of Server\n",total_bytes_read);
    memset(buffer, 0, BUFFER_SIZE);
    fclose(file_fd);
}

void Client::connect_to_server() {
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Failed to connect to server");
        exit(EXIT_FAILURE);
    }
}

void Client::run() {
    FILE *fp;
    // đọc dữ liệu json từ tập tin
    fp = fopen("dataClient.json", "r");
    char message[BUFFER_SIZE];
    fread(message, BUFFER_SIZE, 1, fp);
    fclose(fp);
    // Chuyển đổi chuỗi JSON thành đối tượng JSON
    json_object *parsed_json = json_tokener_parse(message);
    // Lấy giá trị của các trường trong đối tượng JSON
    const char *request1 = json_object_get_string(json_object_object_get(parsed_json, "request1"));
    const char *request2 = json_object_get_string(json_object_object_get(parsed_json, "request2"));
    const char *request3 = json_object_get_string(json_object_object_get(parsed_json, "request3"));
    const char *request4 = json_object_get_string(json_object_object_get(parsed_json, "request4"));

    // gửi message: "Request_Dowload_File"
    send_request(request1);
    char response[BUFFER_SIZE];
    // receive server response to first request
    if (recv(client_socket, response, BUFFER_SIZE, 0) == -1) {
        perror("Failed to receive response");
        exit(EXIT_FAILURE);
    }
    printf("Server response: %s\n", response);
    char name[50];
    scanf("%s",name);
    json_object_object_add(parsed_json, "name", json_object_new_string(name));
    const char *file_name = json_object_get_string(json_object_object_get(parsed_json, "name"));
    if (send(client_socket, file_name, strlen(file_name), 0) == -1) {
        perror("Failed to send message");
        exit(EXIT_FAILURE);
    }
    // Nhận thông tin file từ server
    char infor[BUFFER_SIZE];
    recv(client_socket, infor, BUFFER_SIZE, 0);
    json_object *file_info = json_tokener_parse(infor);
    
    // lấy thông tin về file từ đối tượng JSON
    receive_file_info(file_info);
    const char *md5_str_ptr = json_object_get_string(json_object_object_get(file_info, "md5_str"));
    int file_size = json_object_get_int(json_object_object_get(file_info, "file_size"));
    char md5_sum[MD5_DIGEST_LENGTH*2 + 1];
    
    strcpy(md5_sum,md5_str_ptr);
    memset(response, 0, BUFFER_SIZE);

    // Gửi request yêu cầu gửi file đến server
    send_request(request2);
    //Nhận file rồi ghi vào received_file
    receive_file(file_size);
    
    // tính giá trị MD5sum và so sánh
    string  md5_File_recv;
    file.set_file_name(name);
    file.calculate_md5();
    md5_File_recv = file.get_md5sum();
    
    cout << "Received file MD5sum: " << md5_File_recv << endl;
    cout << "MD5 of file: " << md5_sum << endl;

    if(md5_File_recv ==  md5_sum) {
        printf("File received successfully\n");
        send(client_socket, request3, strlen(request3), 0);
    }
    else {
        printf("File received Falsed\n");
        send(client_socket, request4, strlen(request4), 0);
    }
    close(client_socket);   
}

int main() {
    Client client;
    client.connect_to_server();
    client.run();
    return 0;
}