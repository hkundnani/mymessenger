#include <stdio.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <map>
#include <sys/socket.h>
// #include <sys/types.h>
// #include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int SIZE = 80;
const int BUFFSIZE = 1024;
const char *COLON = " \t\n\r:";
const char *PIPE = " \t\n\r|";
int sockfd, menu = 1, invitation = 0; 
std::string invitationMsg;
bool socketCreated = false;
const int MAXCLIENTS = 5;

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

char *get_port(const char *fileName) {
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
                fclose(file);
                return tokens[1];
            } 
        }
    }
    fclose(file);
    return NULL;
}

char *get_host(const char *fileName) {
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
                fclose(file);
                return tokens[1];
            } 
        }
    }
    fclose(file);
    return NULL;
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
    int sockfd, *sock_ptr, clientfd;
    struct sockaddr_in servaddr, clientaddr;
    socklen_t sock_len;
    pthread_t tid;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(5100);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

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
                userLocInfo[tokens[1]] = locInfo;
            } else if(strcmp(tokens[0], "i") == 0) {
                std::cout << "You have received an invitation from ";
                std::cout << tokens[1] << std::endl;
                std::cout << tokens[2] << std::endl;
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
    int clisockfd;
    int status = 1;
    long portNum = 0;
    char *port = NULL;
    char *host = NULL;
    struct sockaddr_in address, clisockaddr;
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

    port = get_port(argv[1]);
    if (port != NULL) {
        portNum = strtol(port, NULL, 0);
    } else {
        std::cout << "Error: No port field in the config file";
        exit(EXIT_FAILURE);
    }

    host = get_host(argv[1]);
    if (host == NULL) {
        std::cout << "Error: No host field in the config file";
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(host);
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
            }
            user_input = menu_after();
            std::string temp_input = user_input;
            // std::string first_token = user_input.substr(0, user_input.find('|'));
            // std::string friend_name = user_input.substr(first_token.size())
            parse_input(&user_input[0], tokens, PIPE);
            if (strcmp(tokens[0], "i") == 0){
                if ((write(sockfd, user_input.c_str(), user_input.length())) == -1) {
                    perror("Send");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
            } else if (strcmp(tokens[0], "ia") == 0) {
                if ((write(sockfd, user_input.c_str(), user_input.length())) == -1) {
                    perror("Send");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
            } else if (strcmp(tokens[0], "m") == 0) {
                std::string friend_name = tokens[1];
                std::string msg = tokens[2];
                if (userLocInfo.count(friend_name) == 1) {
                    if (stoi(userLocInfo[friend_name]["fd"]) == 0) {
                        int portn = stoi(userLocInfo[friend_name]["port"]);
                        clisockfd = socket(AF_INET, SOCK_STREAM, 0);
                        if (clisockfd == 0) {
                            perror("socket failed");
                            exit(EXIT_FAILURE);
                        }
                        clisockaddr.sin_family = AF_INET;
                        clisockaddr.sin_addr.s_addr = inet_addr(userLocInfo[friend_name]["address"].c_str());
                        address.sin_port = htons((short)portn);

                        if (connect(clisockfd, (struct sockaddr *)&clisockaddr, sizeof(clisockaddr)) < 0) {
                            perror("connect");
                            exit(EXIT_FAILURE);
                        }
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