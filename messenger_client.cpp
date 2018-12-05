#include <stdio.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <iostream>
#include <pthread.h>
#include <map>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int SIZE = 80;
const int BUFFSIZE = 1024;
const char *COLON = " \t\n\r:";
const char *PIPE = "|";
int *new_sock_ptr, menu = 1; 
std::string invitationMsg;
bool socketCreated = false;
const int MAXCLIENTS = 5;
const std::string PORTNUM = "60784";
std::string client_username;

std::map<std::string, std::map<std::string, std::string> > userLocInfo;

void parse_input (char *input, char **tokens, const char *delim) {
    char *token;
    int index = 0;
    
    token = strtok(input, delim);
    
    while (token != NULL) {
        tokens[index] = token;
        index++;
        token = strtok(NULL, delim);
    }
    tokens[index] = NULL;
}

void get_port(const char *fileName, std::string &port) {
    FILE *file;
    char buffer[SIZE];
    char *tokens[SIZE];

    file = fopen(fileName, "r");
    if (file == NULL) {
        perror("File Error");
        exit(0);
    } else {
        while(fgets(buffer, SIZE, file) != NULL) {
            parse_input(buffer, tokens, COLON);
            if (strcmp(tokens[0], "servport") == 0) {
                port = tokens[1];
            } 
        }
    }
    fclose(file);
}

void get_host(const char *fileName, std::string &host) {
    FILE *file;
    char buffer[SIZE];
    char *tokens[SIZE];

    file = fopen(fileName, "r");
    if (file == NULL) {
        perror("File Error");
        exit(0);
    } else {
        while(fgets(buffer, SIZE, file) != NULL) {
            parse_input(buffer, tokens, COLON);
            if (strcmp(tokens[0], "servhost") == 0) {
                host = tokens[1];
            } 
        }
    }
    fclose(file);
}

std::string menu_after() {
    std::string user_input;

    std::cout << "Select the operation to perform:\n";
    std::cout << "Chat. format(m friend_username message)\n";
    std::cout << "Invite. format(i firend_username [message])\n";
    std::cout << "Accept. format(ia inviter_username [message])\n";
    std::cout << "Logout.\n" << std::endl;

    std::getline(std::cin, user_input, '\n');
    std::cin.sync();

    return user_input;
}

void *connection_handler(void *arg) {
    int sockfd;
    ssize_t n;
    char buf[BUFFSIZE];
    char *tokens[SIZE];

    sockfd = *((int *)arg);
    free(arg);

    pthread_detach(pthread_self());
    while ((n = read(sockfd, buf, BUFFSIZE)) > 0) {
	    buf[n] = '\0';
        parse_input(buf, tokens, PIPE);
        if (strcmp(tokens[0], "m") == 0) {

            std::cout << "fd: " << userLocInfo[std::string(tokens[1])]["fd"] << std::endl;
            std::cout << "sock: " << sockfd;
            userLocInfo[std::string(tokens[1])]["fd"] = std::to_string(sockfd);

            std::cout << tokens[1] << " >> " << tokens[2] << std::endl;         
        }
    }
    if (n == 0) {
	    std::cout << "client closed" << std::endl;
    } else {
	    std::cout << "something wrong" << std::endl;
    }
    close(sockfd); 
    return(NULL);
}

void *create_new_socket(void *arg) {
    pthread_detach(pthread_self());
    int newsockfd, *sock_ptr, clientfd, len;
    struct sockaddr_in clientaddr;
    struct addrinfo servaddr, *result, *addr;
    char hostname[SIZE];
    socklen_t sock_len;
    pthread_t tid;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.ai_family = AF_INET;
    servaddr.ai_socktype = SOCK_STREAM;
    servaddr.ai_flags = AI_PASSIVE;

    gethostname(hostname, sizeof hostname);

    // Remember to change the port to 5100 before submit
    if ((getaddrinfo(hostname, PORTNUM.c_str(), &servaddr, &result)) != 0) {
        perror("Error");
        exit(EXIT_FAILURE);
    }
    
    for (addr = result; addr != NULL; addr = addr->ai_next) {
        if ((newsockfd = socket(addr->ai_family, addr->ai_socktype,
            addr->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if (bind(newsockfd, addr->ai_addr, addr->ai_addrlen) == -1) {
            close(newsockfd);
            perror("bind");
            continue;
        }

        break;
    }

    if (addr == NULL) {
        fprintf(stderr, "failed to bind socket\n");
        exit(EXIT_FAILURE);
    }

    std::cout << "newsockfd: " << newsockfd << std::endl;

    len = sizeof(addr->ai_addrlen);

    if (getsockname(newsockfd, (struct sockaddr *)addr->ai_addr, (socklen_t *)&len) < 0)
	{
		perror("can't get name");
		exit(EXIT_FAILURE);
	}
    socketCreated = true;
    
    // printf("ip = %s, port = %d\n", inet_ntoa(((struct sockaddr_in *)addr->ai_addr)->sin_addr), ntohs(((struct sockaddr_in *)addr->ai_addr)->sin_port));

    freeaddrinfo(result);

    printf("Waititng for other client connection...\n");

    sock_len = sizeof(clientaddr);

	if (listen(newsockfd, 5) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

    new_sock_ptr = (int *) malloc(sizeof(int));
    *new_sock_ptr = newsockfd;

    while((clientfd = accept(newsockfd, (struct sockaddr *)&clientaddr, (socklen_t *)&sock_len))) {
	    std::cout << "remote client IP == " << inet_ntoa(clientaddr.sin_addr);
	    std::cout << ", port == " << ntohs(clientaddr.sin_port) << std::endl;

        sock_ptr = (int *) malloc(sizeof(int));
        *sock_ptr = clientfd;

        pthread_create(&tid, NULL, &connection_handler, (void*)sock_ptr);
    }
    close(newsockfd);
    return(NULL);
}

void print_online_friends() {
    std::map<std::string, std::map<std::string, std::string> >::iterator iti;
    std::cout << "Online Friends:" << std::endl;
    for (iti = userLocInfo.begin(); iti != userLocInfo.end(); ++iti) {
        std::cout << iti->first << std::endl;
    }
    std::cout << "\n" << std::endl;
}

void *process_connection(void *arg) {
    int n, sockfd;
    char recvBuff[BUFFSIZE];
    char *tokens[SIZE];

    sockfd = *((int *)arg);
    free(arg);
    
    pthread_detach(pthread_self());
    std::map<std::string, std::string> locInfo;
    
    while (true) {
        n = read(sockfd, recvBuff, sizeof(recvBuff));
        if (n <= 0) {
            if (n == 0) {
                std::cout << "server closed" << std::endl;
            } else {
                std::cout << "something wrong" << std::endl;
            }
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        recvBuff[n] = '\0';
        // if (recvBuff[0] != 0) {
        parse_input(recvBuff, tokens, PIPE);
        // }
        if (strcmp(tokens[1], "200") == 0) {
            menu = 0;
            std::cout << "\n" << tokens[1] << ": " << "Logged in\n" << std::endl;
            menu = 2;
        } else if (strcmp(tokens[1], "500") == 0) {
            if (strcmp(tokens[0], "l") == 0) {
                std::cout << "\n" << tokens[1] << ": " << "Wrong username or password" << std::endl;
            } else if (strcmp(tokens[0], "r") == 0) {
                std::cout << "\n" << tokens[1] << ": " << "Username already exists" << std::endl;
            }
        } else if (strcmp(tokens[0], "loc") == 0) {
            locInfo.insert(std::pair<std::string, std::string>("address", tokens[2]));
            locInfo.insert(std::pair<std::string, std::string>("port", tokens[3]));
            locInfo.insert(std::pair<std::string, std::string>("fd", "0"));
            userLocInfo[tokens[1]] = locInfo;
            if (tokens[4] != NULL && strcmp(tokens[4], "end") == 0) {
                print_online_friends();
            } 
        } else if(strcmp(tokens[0], "i") == 0) {
            std::cout << "You have received an invitation from ";
            std::cout << tokens[1] << std::endl;
            // if (strcmp(tokens[2], NULL) != 0) {
                std::cout << tokens[1] << " >> " << tokens[2] << std::endl;
            // }
        } else if(strcmp(tokens[0], "ia") == 0) {
            std::cout << "Invitation accepted by ";
            std::cout << tokens[1] << std::endl;
            // if (strcmp(tokens[2], NULL) != 0) {
                std::cout << tokens[1] << " >> " << tokens[2] << std::endl;
            // }
        } else if(strcmp(tokens[0], "logout") == 0) {
            std::cout << tokens[1] << " have logout\n";
            userLocInfo.erase(tokens[1]);
            print_online_friends();
        } else {
            std::cout << recvBuff << std::endl;
        }
    }
    menu = 1;
}

std::string menu_before() {
    std::string user_input = "";

    std::cout << std::endl;
    std::cout << "Select the operation to perform:\n";
    std::cout << "1. Register with the server using command 'r'\n";
    std::cout << "2. Log into the server using command 'l'\n";
    std::cout << "3. Exit the client program using command 'exit'\n";
    
    std::getline(std::cin, user_input, '\n');
    std::cin.sync();

    return user_input;
}

void handle_register_login(std::string task, int sockfd) {
    std::string username;
    std::string password;
    std::string action;
    std::string sendBuff;

    std::cout << "Enter username: ";
    std::getline(std::cin, username);
    std::cin.sync();
    std::cout << "Enter password: ";
    std::getline(std::cin, password);
    
    if (task.compare("r") == 0) {
        action = "register";
    } else if (task.compare("l") == 0) {
        action = "login";
    }
    sendBuff = action + "|" + username + "|" + password;
    if ((write(sockfd, sendBuff.c_str(), sendBuff.length())) == -1) {
        perror("Send");
    }
    client_username = username;
}

int main(int argc, char *argv[]) {
    int mainsockfd, clisockfd, rv, flag;
    int status = 1;
    int *sock_ptr;
    long serverPort = 0;
    struct sockaddr_in address;
    struct addrinfo hints, *res, *ressave;
    char *tokens[BUFFSIZE];
    pthread_t tid, sid;
    std::string port = "";
    std::string host = "";
    std::string sendBuff;

    if (argc < 2) {
        std::cout << "Client program needs config file as parameters";
        exit(EXIT_FAILURE);
    }

    mainsockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (mainsockfd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    std::cout << "mainsockfd: " << mainsockfd << std::endl;
    // Get the port number from the config file
    get_port(argv[1], port);
    if (port.compare("") == 0) {
        std::cout << "Error: No port field in the config file\n";
        exit(EXIT_FAILURE);
    } else {
        serverPort = strtol(port.c_str(), NULL, 0);
    }

    // Get the host address from the config file
    get_host(argv[1], host);
    if (host.compare("") == 0) {
        std::cout << "Error: No host field in the config file\n";
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(host.c_str());
    address.sin_port = (serverPort == 0 ? serverPort : htons((short)serverPort));

    // Connect to the server
    if (connect(mainsockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    sock_ptr = (int *) malloc(sizeof(int));
        *sock_ptr = mainsockfd;

    // Assign a thread to receive messages from the server
    pthread_create(&tid, NULL, &process_connection, (void *)sock_ptr);

    do {
        std::string user_input = "";
        if (menu == 1) {
            user_input = menu_before();
            if (user_input.compare("r") == 0) {
                handle_register_login(user_input, mainsockfd);

            } else if (user_input.compare("l") == 0) {
                handle_register_login(user_input, mainsockfd);
    
            } else if (user_input.compare("exit") == 0) {
                std::string sendBuff = "";
                if ((write(mainsockfd, sendBuff.c_str(), sendBuff.length())) == -1) {
                    perror("Send");
                }
                close(mainsockfd);
                status = 0;
            } else {
                std::cout << "Not a valid input\n";
            }
        } else if (menu == 2) {
            if (!socketCreated) {
                pthread_create(&sid, NULL, &create_new_socket, NULL);
            }
            user_input = menu_after();
            std::string temp_input = user_input;
            
            parse_input(&user_input[0], tokens, PIPE);
            if (strcmp(tokens[0], "i") == 0){
                if ((write(mainsockfd, temp_input.c_str(), temp_input.length())) == -1) {
                    perror("Send");
                    // close(sockfd);
                    // exit(EXIT_FAILURE);
                }
            } else if (strcmp(tokens[0], "ia") == 0) {
                if ((write(mainsockfd, temp_input.c_str(), temp_input.length())) == -1) {
                    perror("Send");
                    // close(sockfd);
                    // exit(EXIT_FAILURE);
                }
            } else if (strcmp(tokens[0], "m") == 0) {
                std::string friend_name = tokens[1];
                std::string msg = tokens[2];
                if (userLocInfo.count(friend_name) == 1) {
                    if (stoi(userLocInfo[friend_name]["fd"]) == 0) {
                        bzero(&hints, sizeof(struct addrinfo));
                        hints.ai_family = AF_UNSPEC;
                        hints.ai_socktype = SOCK_STREAM;

                        if ((rv = getaddrinfo(userLocInfo[friend_name]["address"].c_str(), PORTNUM.c_str(), &hints, &res)) != 0) {
                            std::cout << "getaddrinfo wrong: " << gai_strerror(rv) << std::endl;
                            exit(1);
                        }
                        ressave = res;
                        flag = 0;
                        do {
                            clisockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                            if (clisockfd < 0) 
                                continue;
                            if (connect(clisockfd, res->ai_addr, res->ai_addrlen) == 0) {
                                flag = 1;
                                break;
                            }
                            close(clisockfd);
                        } while ((res = res->ai_next) != NULL);
                        freeaddrinfo(ressave);
                        if (flag == 0) {
                            fprintf(stderr, "cannot connect\n");
                            exit(EXIT_FAILURE);
                        }
                        userLocInfo[friend_name]["fd"] = std::to_string(clisockfd);
                    } else {
                        clisockfd = stoi(userLocInfo[friend_name]["fd"]);
                        std::cout << "clisockfd: " << clisockfd << std::endl;
                    }
                    std::string message = "m|" + client_username + "|" + msg;
                    // std::string message = "";
                    if ((write(clisockfd, message.c_str(), message.length())) == -1) {
                        perror("Send");
                        // close(sockfd);
                        // exit(EXIT_FAILURE);
                    }
                } else {
                    std::cout << "Please register the friend before sending the message" << std::endl;
                }
            } else if (strcmp(tokens[0], "logout") == 0) {
                menu = 1;
                std::map<std::string, std::map<std::string, std::string> >::iterator iti;
                if ((write(mainsockfd, tokens[0], sizeof(tokens[0]))) == -1) {
                    perror("Send");
                    // close(sockfd);
                    // exit(EXIT_FAILURE);
                }
                for (iti = userLocInfo.begin(); iti != userLocInfo.end(); ++iti){
                    std::cout << iti->second["fd"] << std::endl;
                    close(stoi(iti->second["fd"]));
                }
                std::cout << *((int *)new_sock_ptr) << std::endl;
                close(*((int *)new_sock_ptr));
                socketCreated = false;
                userLocInfo.clear();
                // int p = *((int *)new_sock_ptr);
                // std::cout << p << std::endl;
            }
        } 
        sleep(1);
    } while(status);

    close(mainsockfd);

    return 0;
}
