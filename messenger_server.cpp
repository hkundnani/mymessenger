#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <iostream>
#include <map>
#include <utility>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>

const int SIZE = 80;
const char *COLON = " \t\n\r:";
const char *PIPE = " \t\n\r|";
const char *USER_FILE = " \t\n\r|;";
const int MAXCLIENTS = 5;

int sockfd;

std::map<std::string, std::map<std::string, std::string> > userLocInfo;
std::map<std::string, std::vector<std::string> > userFriendInfo;

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
        exit(EXIT_FAILURE);
    } else {
        while(fgets(buffer, SIZE, file) != NULL) {
            parse_input(buffer, tokens, COLON);
            if (strcmp(tokens[0], "port") == 0) {
                port = tokens[1];
            } 
        }
    }
    fclose(file);
}

bool validate_login(char *username, char *password, const char *fileName) {
    FILE *fp;
    char buffer[SIZE];
    char *tokens[SIZE];
    std::string name = "";
    std::vector<std::string> frnd_info;

    fp = fopen(fileName, "r");
    if (fp == NULL) {
        perror("File Error");
        exit(EXIT_FAILURE);

    } else {
        if (userLocInfo.count(username) > 0) {
            fclose(fp);
            return false;
        }
        while (fgets(buffer, SIZE, fp) != NULL) {
            parse_input(buffer, tokens, USER_FILE);
            if ((strcmp(tokens[0], username) == 0) && (strcmp(tokens[1], password) == 0)) {
                name = username;
                for (int i = 2; tokens[i] != NULL; i++) {
                    frnd_info.push_back(tokens[i]);
                }
                userFriendInfo[name] = frnd_info;
                fclose(fp);
                return true;
            }
        }
    }
    fclose(fp);
    return false;
}

void add_login_users(std::string username, int fd) {
    int len = 0;
    struct sockaddr_in newaddr;
    std::map<std::string, std::string> loc_info;
    std::string user = std::to_string(fd);

    len = sizeof(newaddr);
    memset(&newaddr, 0, len);
    getpeername(fd, (struct sockaddr*)&newaddr, (socklen_t*)&len);

    printf("ip %s, port %d\n",inet_ntoa(newaddr.sin_addr), (newaddr.sin_port));
    
    loc_info.insert(std::pair<std::string, std::string>("address", std::string(inet_ntoa(newaddr.sin_addr))));
    loc_info.insert(std::pair<std::string, std::string>("port", std::to_string(ntohs(newaddr.sin_port))));
    loc_info.insert(std::pair<std::string, std::string>("fd", user));
    userLocInfo[username] = loc_info;
}

bool register_user(std::string username, std::string password, const char *fileName) {
    FILE *fp;
    char buffer[SIZE];
    char *tokens[SIZE];
    int flag = 0;
    // char *str = NULL;
    std::string str; 

    fp = fopen(fileName, "a+");
    if (fp == NULL) {
        perror("File Error");
        exit(EXIT_FAILURE);

    } else {
        while (fgets(buffer, SIZE, fp) != NULL) {
            parse_input(buffer, tokens, USER_FILE);
            if (strcmp(tokens[0], username.c_str()) == 0) {
                flag = 1;
                break;
            }
        }
    }
    if (flag) {
        fclose(fp);
        return false;
    } else {
        str = username + "|" + password;
        // str = strcat(username, "|"); 
        // str = strcat(str, password);
        fprintf(fp, "%s\n", str.c_str());
        fclose(fp);
        return true;
    }
}

// TODO improve logic
void send_location(std::string username) {
    std::string location;
    int frndfd;
    std::string end = "done";
    std::vector<std::string> friends_list = userFriendInfo[username];
    std::vector<std::string> online_friends_list;
    std::vector<std::string>::iterator name;
    int userfd = stoi(userLocInfo[username]["fd"]);

    for (name = friends_list.begin(); name != friends_list.end(); ++name) {
        if (userLocInfo.count(*name) > 0) {
            online_friends_list.push_back(*name);
            location = "loc|" + username + "|" + userLocInfo[username]["address"] + "|" + userLocInfo[username]["port"] + "|end";
            frndfd = stoi(userLocInfo[*name]["fd"]);
            if ((send(frndfd, &location[0], location.size(), 0)) == -1) {
                perror("send");
                // close(fd);
                // FD_CLR(fd, &readfds);   
                // clients[i] = 0;
            }
        }
    }

    for (name = online_friends_list.begin(); name != online_friends_list.end(); ++name) {
        location = "loc|" + *name + "|" + userLocInfo[*name]["address"] + "|" + userLocInfo[*name]["port"];
        if ((name + 1) == online_friends_list.end()) {
            location += "|end";
        }
        if ((send(userfd, &location[0], location.size(), 0)) == -1) {
            perror("send");
            // close(fd);
            // FD_CLR(fd, &readfds);   
            // clients[i] = 0;
        }
    }
}

void send_invitation(int client, char *friendName, char* msg) {
    std::map<std::string, std::map<std::string, std::string> >::iterator iti;
    std::string username;
    std::string message = msg;
    std::string frnd = friendName;
    std::string invitation;
    int fd;

    for (iti = userLocInfo.begin(); iti != userLocInfo.end(); ++iti) {
        if (stoi(iti->second["fd"]) == client) {
            username = iti->first;
        }
    }
    if (username.compare(frnd) == 0) {
        message = "You can't send invitation to yourself";
        if ((send(client, &message[0], message.size(), 0)) == -1) {
            perror("send");
            // close(fd);
            // FD_CLR(fd, &readfds);   
            // clients[i] = 0;
        }
    } else {
        for (iti = userLocInfo.begin(); iti != userLocInfo.end(); ++iti) {
            if (frnd.compare(iti->first) == 0) {
                fd = stoi(userLocInfo[frnd]["fd"]);
                invitation = "i|" + username + "|" + message;
                if ((send(fd, &invitation[0], invitation.size(), 0)) == -1) {
                    perror("send");
                    // close(fd);
                    // FD_CLR(fd, &readfds);   
                    // clients[i] = 0;
                }
                // message = "Invitation Sent";
                // if ((send(client, &message[0], message.size(), 0)) == -1) {
                //     perror("send");
                //     // close(fd);
                //     // FD_CLR(fd, &readfds);   
                //     // clients[i] = 0;
                // }
            }
        }
    }
}

void accept_invitation(int client, char *friendName, char* msg, const char *fileName) {
    std::map<std::string, std::map<std::string, std::string> >::iterator iti;
    std::string username;
    std::string message = msg;
    std::string frnd = friendName;
    std::string invitation_accept;
    int fd;
    FILE *fpr, *fpw;
    char buffer[SIZE];
    char tempbuffer[SIZE];
    char *tokens[SIZE];
    //file
    fpr = fopen(fileName, "r");
    if (fpr == NULL) {
        perror("File Error");
        exit(EXIT_FAILURE);
    }
    fpw = fopen("temp", "w");
    if (fpw == NULL) {
        perror("File Error");
        exit(EXIT_FAILURE);
    }

    for (iti = userLocInfo.begin(); iti != userLocInfo.end(); ++iti) {
        if (stoi(iti->second["fd"]) == client) {
            username = iti->first;
            userFriendInfo[username].push_back(frnd);
            userFriendInfo[frnd].push_back(username);
        }
        if (frnd.compare(iti->first) == 0) {
            fd = stoi(userLocInfo[frnd]["fd"]);
            invitation_accept = "ia|" + username + "|" + message;
            if ((send(fd, &invitation_accept[0], invitation_accept.size(), 0)) == -1) {
                perror("send");
                // close(fd);
                // FD_CLR(fd, &readfds);   
                // clients[i] = 0;
            }
            // std::string success = "";
            // if ((send(client, &success[0], success.size(), 0)) == -1) {
            //     perror("send");
            //     // close(fd);
            //     // FD_CLR(fd, &readfds);   
            //     // clients[i] = 0;
            // }
        }
    }
    
    while (fgets(buffer, SIZE, fpr) != NULL) {
        strcpy(tempbuffer, buffer);
        tempbuffer[strlen(tempbuffer) - 1] = '\000';     
        parse_input(buffer, tokens, USER_FILE);
        std::string name = tokens[0];
        if (username.compare(name) == 0) {
            strcat(tempbuffer, ";");
            strcat(tempbuffer, frnd.c_str());
        }
        if (frnd.compare(name) == 0) {
            strcat(tempbuffer, ";");
            strcat(tempbuffer, username.c_str());
        }
        int len = strlen(tempbuffer);
        if (tempbuffer[len] != '\n') {
            tempbuffer[len] = '\n';
        }
        // std::cout << len << std::endl;
        tempbuffer[len + 1] = '\0';
        fprintf(fpw, "%s", tempbuffer);
    }
    fclose(fpr);
    fclose(fpw);
    rename("temp", fileName);

    send_location(username);
}

void logout_user(int clientfd) {
    std::map<std::string, std::map<std::string, std::string> >::iterator iti;
    std::string username;
    std::vector<std::string>::iterator name;
    std::string msg;
    int frndfd;

    for (iti = userLocInfo.begin(); iti != userLocInfo.end(); ++iti) {
        if (stoi(iti->second["fd"]) == clientfd) {
            username = iti->first;
        }
    }
    std::vector<std::string> friends_list = userFriendInfo[username];

    for (name = friends_list.begin(); name != friends_list.end(); ++name) {
        if (userLocInfo.count(*name) > 0) {
            msg = "logout|" + username;
            frndfd = stoi(userLocInfo[*name]["fd"]);
            if ((send(frndfd, &msg[0], msg.size(), 0)) == -1) {
                perror("send");
                // close(fd);
                // FD_CLR(fd, &readfds);   
                // clients[i] = 0;
            }
        }
    }
    userFriendInfo.erase(username);
    userLocInfo.erase(username);
}

void sig_chld(int signo)
{
	pid_t pid;
	int stat;
	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {}
		close(sockfd);
	return ;
}

int main(int argc, char *argv[]) {
    int maxfd, clients[MAXCLIENTS], clientfd, len, index, num = 0;
    std::string port = "";
    char *tokens[SIZE];
    char hostname[SIZE];
    struct sigaction sa;
    struct sockaddr_in newaddr;
    struct addrinfo address, *result, *addr;
    std::string username;
    std::string password;
    //set of socket descriptors  
    fd_set readfds, clfds;

    if (argc < 3) {
        printf("%s\n", "Server program needs user info and config file as parameters" );
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = sig_chld;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGCHLD, &sa, NULL);

    get_port(argv[2], port);
    if (port.compare("") == 0) {
        printf("Error: No port field in the config file");
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.ai_family = AF_INET;
    address.ai_socktype = SOCK_STREAM;
    address.ai_flags = AI_PASSIVE;

    gethostname(hostname, sizeof hostname);

    if ((getaddrinfo(hostname, port.c_str(), &address, &result)) != 0) {
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

    // printf("ip = %s, port = %d\n", inet_ntoa(address.), htons(address.sin_port));

    printf("ip = %s, port = %d\n", inet_ntoa(((struct sockaddr_in *)addr->ai_addr)->sin_addr), ntohs(((struct sockaddr_in *)addr->ai_addr)->sin_port));

    freeaddrinfo(result);

    printf("Waititng for connection...\n");

	if (listen(sockfd, 5) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

    //initialise all client_socket[] to 0 so not checked  
    for (int i = 0; i < MAXCLIENTS; i++)   
    {   
        clients[i] = 0;   
    }

    //clear the socket set  
    FD_ZERO(&readfds);

    //add master socket to set  
    FD_SET(sockfd, &readfds);   
    maxfd = sockfd;   
    
    while (1) {
        // int *status = NULL;
        char *sendbuff = (char *) malloc(SIZE * sizeof(char));
        char recvbuff[1024];
        
        clfds = readfds;

        if (select( maxfd + 1 , &clfds , NULL , NULL , NULL) < 0) {
            perror("Select");
        }

        if (FD_ISSET(sockfd, &clfds))   
        { 
            if ((clientfd = accept(sockfd, (struct sockaddr
                                *)(&newaddr), (socklen_t *)&len)) < 0) {
                if (errno == EINTR)
                    continue;
                else {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
            }
            //add child sockets to set  
            for (index = 0; index < MAXCLIENTS; index++)   
            {   
                if (clients[index] <= 0) {
                    clients[index] = clientfd;
                    FD_SET(clients[index], &readfds);
                    break;
                }  
            }

            // if (index == MAXCLIENTS) {
            //     std::cout << "Too many connections\n";
            //     close(clientfd);
            // }

            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
        }

        //else its some IO operation on some other socket 
        for (int i = 0; i < MAXCLIENTS; i++)   
        {   
            if(clients[i] < 0) {
                continue;
            }   
                 
            if (FD_ISSET(clients[i], &clfds))   
            {   
                //Check if it was for closing , and also read the  
                //incoming message
                if ((num = recv(clients[i], recvbuff, 1024, 0)) == -1) {
                    perror("recv");
                    exit(EXIT_FAILURE);
                } else if (num == 0) {
                    //Somebody disconnected , get his details and print  
                    getpeername(clients[i] , (struct sockaddr*)&newaddr, (socklen_t*)&len);   
                    printf("Host disconnected, ip %s, port %d\n",inet_ntoa(newaddr.sin_addr), ntohs(newaddr.sin_port));
                    //Close the socket and mark as 0 in list for reuse
                    //TODO remove the user from the two maps  
                    close(clients[i]);
                    FD_CLR(clients[i], &readfds);   
                    clients[i] = 0;
                } else {
                    recvbuff[num] = '\0';
                    if (recvbuff != NULL) {
                        parse_input(recvbuff, tokens, PIPE);
                        if (strcmp(tokens[0], "login") == 0) {
                            username = tokens[1];
                            if (validate_login(tokens[1], tokens[2], argv[1])) {
                                
                                add_login_users(username, clients[i]);
                                   
                                strcpy(sendbuff, "l|200");
                            } else {
                                strcpy(sendbuff, "l|500");
                            }
                            if ((send(clients[i], sendbuff, sizeof(sendbuff), 0)) == -1)
                            {
                                perror("send");
                                close(clients[i]);
                                FD_CLR(clients[i], &readfds);   
                                clients[i] = 0;
                            }
                            if (strcmp(sendbuff, "l|200") == 0) {
                                send_location(username);
                            }   
                        } else if (strcmp(tokens[0], "register") == 0) {
                            username = tokens[1];
                            password = tokens[2];
                            if (register_user(username, password, argv[1])) {
                                
                                add_login_users(username, clients[i]);
                                
                                strcpy(sendbuff, "r|200");
                            } else {
                                strcpy(sendbuff, "r|500");
                            }
                            if ((send(clients[i], sendbuff, sizeof(sendbuff), 0)) == -1)
                            {
                                perror("send");
                                close(clients[i]);
                                FD_CLR(clients[i], &readfds);   
                                clients[i] = 0;
                            }
                        } else if (strcmp(tokens[0], "i") == 0) {
                            if (tokens[2] == NULL) {
                                strcpy(tokens[2], "");
                            }
                            send_invitation(clients[i], tokens[1], tokens[2]);
                        } else if (strcmp(tokens[0], "ia") == 0) {
                            if (tokens[2] == NULL) {
                                strcpy(tokens[2], "");
                            }
                            accept_invitation(clients[i], tokens[1], tokens[2], argv[1]);
                        } else if (strcmp(tokens[0], "logout") == 0) {
                            logout_user(clients[i]);
                        }
                    }
                }
            } 
        }
        free(sendbuff);
    }
    return 0;
}