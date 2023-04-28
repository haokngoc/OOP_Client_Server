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

#define PORT 8080
#define BUFFER_SIZE 1024

class Server {
public:
    Server();
    ~Server();
    void run();
private:
    void calculate_md5(char *filename, char *md5_str);
    int create_socket();
    void set_socket_options(int socket_fd);
    void bind_socket(int socket_fd, struct sockaddr_in address);
    void listen_for_connections(int socket_fd);
    void send_file_info(int new_socket, char* filename);
    void send_file_to_client(int new_socket, const char *filename);
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    char message[BUFFER_SIZE];
    int num_requests = 0;
};

Server::Server() {
    // Tạo socket
    server_fd = create_socket();
    // Đặt các tùy chọn cho socket
    set_socket_options(server_fd);
    bind_socket(server_fd, address);
    // Lắng nghe kết nối từ Client
    listen_for_connections(server_fd);
    printf("Server is listening on port %d...\n", PORT);
}

Server::~Server() {
    close(server_fd);
}

void Server::run() {
    while(1) {
        // Chấp nhận kết nối từ Client
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
       printf("Handling request #%d from %s:%d\n", ++num_requests, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        // Xử lý request từ Client
        int num_bytes_received = recv(new_socket, buffer, BUFFER_SIZE, 0);
        if(num_bytes_received == -1 ) {
            printf("Failed");
            exit(EXIT_FAILURE);
        }
        else {
            printf("%s\n", buffer);
        }
        // clear response buffer
        memset(buffer, 0, BUFFER_SIZE);
        // Đọc dữ liệu JSON từ tập tin
        FILE *fp;
        fp = fopen("dataServer.json", "r");
        char message[1024];
        fread(message, 1024, 1, fp);
        fclose(fp);
        // Chuyển đổi chuỗi JSON thành đối tượng JSON
        json_object *parsed_json = json_tokener_parse(message);
        const char *request = json_object_get_string(json_object_object_get(parsed_json, "filename"));
        //gửi request tới client
        send(new_socket, request, strlen(request), 0);
        // nhận tên file mà client gửi đến
        char filename[BUFFER_SIZE];
        memset(filename, 0, BUFFER_SIZE);
        recv(new_socket, filename, BUFFER_SIZE, 0);
        printf("File is requested to send: %s\n",filename);
        // Gửi thông tin về file cho client
        send_file_info(new_socket, filename);
        // nhận message : "Please send file";
        memset(buffer, 0, BUFFER_SIZE);
        recv(new_socket, buffer, BUFFER_SIZE, 0);
        printf("Client response: %s\n",buffer);
        memset(buffer, 0, BUFFER_SIZE);
        // Gửi nội dung của file cho client
        send_file_to_client(new_socket, filename);
        //nhận phản hồi từ Client xem đã dowload File thành công hay chưa
        recv(new_socket, buffer, BUFFER_SIZE, 0);
        printf("Client response: %s\n",buffer);
        memset(buffer, 0, BUFFER_SIZE);
        if(strcmp(buffer,"Dowload Done") == 0) {
                close(server_fd);
        }
        // Đóng kết nối tới client
        close(new_socket);
    }
}

void Server::calculate_md5(char *filename, char *md5_str) {
    unsigned char c[MD5_DIGEST_LENGTH];
    int i;
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];

    if (inFile == NULL) {
        printf("%s can't be opened.\n", filename);
    }

    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
    {
        MD5_Update (&mdContext, data, bytes);
    }
        
    MD5_Final (c,&mdContext);

    for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&md5_str[i*2], "%02x", c[i]);
    }
    md5_str[MD5_DIGEST_LENGTH*2] = '\0';
    
    fclose (inFile);
}

int Server::create_socket() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
        }
        return socket_fd;
    }

void Server::set_socket_options(int socket_fd) {
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
}

void Server::bind_socket(int socket_fd, struct sockaddr_in address) {
    memset(&address, '0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
}
}

void Server::listen_for_connections(int socket_fd) {
    if (listen(socket_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}

void Server::send_file_info(int new_socket, char* filename) {
    char md5_str[MD5_DIGEST_LENGTH*2 +1];
    int file_descriptor = open(filename, O_RDONLY);
    if (file_descriptor == -1) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    int file_size = lseek(file_descriptor, 0, SEEK_END);
    lseek(file_descriptor, 0, SEEK_SET);
    calculate_md5(filename, md5_str);       
    printf("md5: %s\n",md5_str);
    json_object *info_file = json_object_new_object();
    // thêm thông tin của file vào đối tượng JSON
    json_object_object_add(info_file, "filename", json_object_new_string(filename));
    json_object_object_add(info_file, "file_size", json_object_new_int(file_size));
    json_object_object_add(info_file, "md5_str", json_object_new_string(md5_str));

    // gửi Json tới client
    const char *json_str = json_object_to_json_string(info_file);
    int bytes = send(new_socket, json_str, strlen(json_str), 0);
    json_object_put(info_file);
}

void Server::send_file_to_client(int new_socket, const char *filename) {
    // Mở file
    int file_descriptor = open(filename, O_RDONLY);
    if (file_descriptor == -1) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    // Gửi dữ liệu từ file tới client
    off_t offset = 0;
    ssize_t sent_bytes = 0;
    while ((sent_bytes = sendfile(new_socket, file_descriptor, &offset, BUFFER_SIZE)) > 0) {
        printf("Sent %ld bytes of file\n", sent_bytes);
    }
    
    // Đóng file descriptor
    close(file_descriptor);
}

int main() {
    Server server;
    server.run();
    return 0;
}
