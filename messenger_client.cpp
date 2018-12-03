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
// #include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int SIZE = 80;
const int BUFFSIZE = 1024;
const char *COLON = " \t\n\r:";
const char *PIPE = "|";
int sockfd, menu = 1, invitation = 0; 
std::string invitationMsg;
bool socketCreated = false;
const int MAXCLIENTS = 5;
const std::string PORTNUM = "60784";

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
    // return NULL;
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
    // return NULL;
}

std::string menu_after() {
    std::string user_input;

    std::cout << "Select the operation to perform:\n";
    std::cout << "Chat. format(m friend_username message)\n";
    std::cout << "Invite. format(i firend_username [message])\n";
    std::cout << "Accept. format(ia inviter_username [message])\n";
    std::cout << "Logout.\n" << std::endl;
    getline(std::cin, user_input);

    return user_input;
}

void *connection_handler(void *arg) {
    int sockfd;
    ssize_t n;
    char buf[BUFFSIZE];

    sockfd = *((int *)arg);
    free(arg);

    pthread_detach(pthread_self());
    while ((n = read(sockfd, buf, BUFFSIZE)) > 0) {
	    buf[n] = '\0';
	    std::cout << buf << std::endl;
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
    int sockfd, *sock_ptr, clientfd, len;
    struct sockaddr_in clientaddr;
    struct addrinfo servaddr, *result, *addr;
    char hostname[SIZE];
    socklen_t sock_len;
    pthread_t tid;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

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
        if ((sockfd = socket(addr->ai_family, addr->ai_socktype,
            addr->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if (bind(sockfd, addr->ai_addr, addr->ai_addrlen) == -1) {
            close(sockfd);
            perror("bind");
            continue;
        }

        break;
    }

    if (addr == NULL) {
        fprintf(stderr, "failed to bind socket\n");
        exit(EXIT_FAILURE);
    }

    len = sizeof(addr->ai_addrlen);

    if (getsockname(sockfd, (struct sockaddr *)addr->ai_addr, (socklen_t *)&len) < 0)
	{
		perror("can't get name");
		exit(EXIT_FAILURE);
	}
    socketCreated = true;
    printf("ip = %s, port = %d\n", inet_ntoa(((struct sockaddr_in *)addr->ai_addr)->sin_addr), ntohs(((struct sockaddr_in *)addr->ai_addr)->sin_port));

    freeaddrinfo(result);

    printf("Waititng for connection...\n");

    sock_len = sizeof(clientaddr);

	if (listen(sockfd, 5) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

    while((clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, (socklen_t *)&sock_len))) {
	    std::cout << "remote client IP == " << inet_ntoa(clientaddr.sin_addr);
	    std::cout << ", port == " << ntohs(clientaddr.sin_port) << std::endl;

        sock_ptr = (int *) malloc(sizeof(int));
        *sock_ptr = clientfd;
	
	    pthread_create(&tid, NULL, &connection_handler, (void*)sock_ptr);
    }
    return(NULL);
}

void print_online_friends() {
    std::map<std::string, std::map<std::string, std::string> >::iterator iti;
    std::cout << "Online Friends:" << std::endl;
    for (iti = userLocInfo.begin(); iti != userLocInfo.end(); ++iti) {
        std::cout << iti->first << std::endl;
    }
}

void *process_connection(void *arg) {
    int n;
    char recvBuff[BUFFSIZE];
    char *tokens[SIZE];
    pthread_detach(pthread_self());
    std::map<std::string, std::string> locInfo;
    
    while (1) {
        n = read(sockfd, recvBuff, sizeof(recvBuff));
        if (n <= 0) {
            if (n == 0) {
                std::cout << "server closed" << std::endl;
            } else {
                std::cout << "something wrong" << std::endl;
            }
            close(sockfd);
            // we directly exit the whole process.
            exit(EXIT_FAILURE);
        }
        recvBuff[n] = '\0';
        if (strcmp(recvBuff, "200") == 0) {
            menu = 0;
            std::cout << "\nLogged in\n" << std::endl;
            menu = 2;
        } else if (strcmp(recvBuff, "500") == 0) {
            std::cout << "Error" << std::endl;
        } else { 
            parse_input(recvBuff, tokens, PIPE);
            if (strcmp(tokens[0], "loc") == 0) {
                locInfo.insert(std::pair<std::string, std::string>("address", tokens[2]));
                locInfo.insert(std::pair<std::string, std::string>("port", tokens[3]));
                locInfo.insert(std::pair<std::string, std::string>("fd", "0"));
                userLocInfo[tokens[1]] = locInfo;
                print_online_friends();
            } else if(strcmp(tokens[0], "i") == 0) {
                std::cout << "You have received an invitation from ";
                std::cout << tokens[1] << std::endl;
                std::cout << tokens[1] << " >> " << tokens[2] << std::endl;
            } else if(strcmp(tokens[0], "ia") == 0) {
                std::cout << "Invitation accepted by ";
                std::cout << tokens[1] << std::endl;
                std::cout << tokens[2] << std::endl;
            }
        }
    }
    menu = 1;
    free(recvBuff);
}

std::string menu_before() {
    std::string user_input;

    std::cout << std::endl;
    std::cout << "Select the operation to perform:\n";
    std::cout << "1. Register with the server using command 'r'\n";
    std::cout << "2. Log into the server using command 'l'\n";
    std::cout << "3. Exit the client program using command 'exit'\n";
    getline(std::cin, user_input);

    return user_input;
}

int main(int argc, char *argv[]) {
    int clisockfd, rv, flag;
    int status = 1;
    long portNum = 0;
    std::string port = "";
    std::string host = "";
    struct sockaddr_in address, clisockaddr;
    struct addrinfo hints, *res, *ressave;
    std::string user_input;
    std::string username;
    std::string password;
    pthread_t tid, sid;
    char *tokens[BUFFSIZE];

    if (argc < 2) {
        std::cout << "Client program needs config file as parameters";
        exit(EXIT_FAILURE);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    get_port(argv[1], port);
    if (port.compare("") == 0) {
        std::cout << "Error: No port field in the config file\n";
        exit(EXIT_FAILURE);
    } else {
        portNum = strtol(port.c_str(), NULL, 0);
    }

    get_host(argv[1], host);
    if (host.compare("") == 0) {
        std::cout << "Error: No host field in the config file\n";
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(host.c_str());
    address.sin_port = (portNum == 0 ? portNum : htons((short)portNum));

    if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    pthread_create(&tid, NULL, &process_connection, NULL);
    do {
        if (menu == 1) {
            user_input = menu_before();
            if (user_input.compare("r") == 0) {
                std::cout << "Enter username: ";
                getline(std::cin, username);
                std::cout << "Enter password: ";
                getline(std::cin, password);
                std::string sendBuff = "register | " + username + " | " + password;
                if ((write(sockfd, sendBuff.c_str(), sendBuff.length())) == -1) {
                    perror("Send");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
            } else if (user_input.compare("l") == 0) {
                std::cout << "Enter username: ";
                getline(std::cin, username);
                std::cout << "Enter password: ";
                getline(std::cin, password);
                std::string sendBuff = "login | " + username + " | " + password;
                if ((write(sockfd, sendBuff.c_str(), sendBuff.length())) == -1) {
                    perror("Send");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
            } else if (user_input.compare("exit") == 0) {
                std::string sendBuff = "";
                if ((write(sockfd, sendBuff.c_str(), sendBuff.length())) == -1) {
                    perror("Send");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
                close(sockfd);
                // exit(EXIT_SUCCESS);
                // send_data(sockfd, &sendBuff[0], sendBuff.size());
                status = 0;
            } else {
                std::cout << "Not a valid input\n";
            }
        } else if (menu == 2) {
            if (!socketCreated) {
                pthread_create(&sid, NULL, &create_new_socket, NULL);
                // create_new_socket(NULL);
            }
            user_input = menu_after();
            std::string temp_input = user_input;
            // std::string first_token = user_input.substr(0, user_input.find('|'));
            // std::string friend_name = user_input.substr(first_token.size())
            parse_input(&user_input[0], tokens, PIPE);
            if (strcmp(tokens[0], "i") == 0){
                if ((write(sockfd, temp_input.c_str(), temp_input.length())) == -1) {
                    perror("Send");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
            } else if (strcmp(tokens[0], "ia") == 0) {
                if ((write(sockfd, temp_input.c_str(), temp_input.length())) == -1) {
                    perror("Send");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
            } else if (strcmp(tokens[0], "m") == 0) {
                // std::map<std::string, std::string> locInfo;
                // locInfo.insert(std::pair<std::string, std::string>("address", "127.0.0.1"));
                // locInfo.insert(std::pair<std::string, std::string>("port", PORTNUM));
                // locInfo.insert(std::pair<std::string, std::string>("fd", "0"));
                // userLocInfo["user2"] = locInfo;
                std::string friend_name = tokens[1];
                std::string msg = tokens[2];
                if (userLocInfo.count(friend_name) == 1) {
                    if (stoi(userLocInfo[friend_name]["fd"]) == 0) {
                        // int portn = strtol(PORTNUM.c_str(), NULL, 0);
                        // clisockfd = socket(AF_INET, SOCK_STREAM, 0);
                        // if (clisockfd == 0) {
                        //     perror("socket failed");
                        //     exit(EXIT_FAILURE);
                        // }
                        // clisockaddr.sin_family = AF_INET;
                        // clisockaddr.sin_addr.s_addr = inet_addr(userLocInfo[friend_name]["address"].c_str());
                        // address.sin_port = portn;

                        bzero(&hints, sizeof(struct addrinfo));
                        hints.ai_family = AF_UNSPEC;
                        hints.ai_socktype = SOCK_STREAM;

                        if ((rv = getaddrinfo(userLocInfo[friend_name]["address"].c_str(), PORTNUM.c_str(), &hints, &res)) != 0) {
                            std::cout << "getaddrinfo wrong: " << gai_strerror(rv) << std::endl;
                            exit(1);
                        }
                        // std::cout << "Address: " << userLocInfo[friend_name]["address"].c_str() << std::endl;
                        // std::cout << "Port: " << htons((short)portn) << std::endl;
     
                        // if (connect(clisockfd, (struct sockaddr *)&clisockaddr, sizeof(clisockaddr)) < 0) {
                        //     perror("connect");
                        //     exit(EXIT_FAILURE);
                        // }
                        ressave = res;
                        flag = 0;
                        do {
                            clisockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                            if (sockfd < 0) 
                                continue;
                            if (connect(clisockfd, res->ai_addr, res->ai_addrlen) == 0) {
                                flag = 1;
                                break;
                            }
                            close(sockfd);
                        } while ((res = res->ai_next) != NULL);
                        freeaddrinfo(ressave);
                        if (flag == 0) {
                        fprintf(stderr, "cannot connect\n");
                        exit(1);
                        }
                        userLocInfo[friend_name]["fd"] = std::to_string(clisockfd);
                    } else {
                        clisockfd = stoi(userLocInfo[friend_name]["fd"]);
                    }
                    std::string message = "m|" + username + "|" + msg;
                    if ((write(clisockfd, message.c_str(), message.length())) == -1) {
                        perror("Send");
                        close(sockfd);
                        exit(EXIT_FAILURE);
                    }
                } else {
                    std::cout << "Please register the friend before sending the message" << std::endl;
                }
            }
        } 
        // else if (invitation == 1) {
        //     std::string input;
        //     std::cout << invitationMsg;
        //     getline(std::cin, input);
        //     if (input.compare("yes") == 0) {

        //     }
        // }
        sleep(1);
        // free(recvBuff);
    } while(status);

    close(sockfd);

    return 0;
}
