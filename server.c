#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdbool.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
#define MAX_GROUPS 10
#define GROUP_NAME_LENGTH 50

#define CLOSESOCKET(s) closesocket(s)
#define SOCKET int
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define GETSOCKETERRNO() (WSAGetLastError())


typedef struct {
    char name[GROUP_NAME_LENGTH];
    SOCKET members[MAX_CLIENTS];
    int memberCount;
    int maxIndex;
    int admin;
} GroupChat;

GroupChat groups[MAX_GROUPS];
int groupCount = 0;

int addClientToGroup(GroupChat* group, SOCKET client) {
    
    if (group->memberCount < MAX_CLIENTS) { // 그룹에서 수용할 수 있는 멤버 수 이내일 시

        for (int i = 0; i < group->maxIndex; i++) { // 그룹에 이미 존재하는 멤버일 시 아무것도 하지 않는다.
            if (group->members[i] == client)
                return 1;
        }

        for (int i = 0; i <= group->maxIndex; i++) { // 정상적으로 작동할 시
            if (group->members[i] == 0) {
                group->members[i] = client;
                group->memberCount++;
           
                if (group->memberCount > group->maxIndex) { 
                    group->maxIndex = group->memberCount; // maxIndex까지만 반복문을 수행하여 반복을 최소화하여 성능을 향상시키기 위해 maxIndex 변수를 설정함 -> memberCount(멤버수)까지만 반복을 수행하도록 하기 위함 
                }
                return 2;
            }

        }

    }
    else // 그룹에서 수용할 수 있는 멤버 수를 초과했을 시;
    {
        return 3;
    }
}

int deleteClientToGroup(GroupChat* group, SOCKET client) {
    if (group->admin != client) { // 해당 클라이언트가 이 그룹의 관리자가 아닐 때만 그룹을 나갈 수 있음
        for (int i = 0; i < group->maxIndex; i++) { // 그룹에서 해당 클라이언트가 있는지 조회하고 있으면 삭제
            if (group->members[i] == client) {
                group->members[i] = 0;
                group->memberCount--;
                return 1;
            }
        }
        return 2; // 그룹에서 해당 클라이언트를 찾지 못햇을 때
    }
    else {
        return 3; // 해당 클라이언트가 관리자일 때
    }
}

void broadcastToGroup(GroupChat* group, SOCKET sender, const char* message) {
    for (int i = 0; i < group->memberCount; i++) {
        if (group->members[i] != sender) {
            send(group->members[i], message, strlen(message), 0);
        }
    }
}

GroupChat* findGroupByName(const char* name) {
    for (int i = 0; i < groupCount; i++) {
        if (strcmp(groups[i].name, name) == 0) {
            return &groups[i];
        }
    }
    return NULL;
}

int deleteGroupByName(GroupChat* group) {
    if (group) { // 해당하는 그룹이 존재할 때
        memset(group, 0, sizeof(group)); // 해당 그룹을 삭제함
        return 1;
    }
    else {
        return 2;// 해당하는 그룹이 존재하지 않을 때
    }
}

void handleClientCommand(SOCKET client, const char* command) {
    char response[BUFFER_SIZE];
    char groupName[GROUP_NAME_LENGTH] = { 0 };

    if (sscanf(command, "CREATE %s", groupName) == 1) {
        if (findGroupByName(groupName)) {
            sprintf(response, "Group '%s' already exists.\n", groupName);
        }
        else if (groupCount >= MAX_GROUPS) {
            sprintf(response, "Cannot create more groups. Maximum limit reached.\n");
        }
        else {
            strcpy(groups[groupCount].name, groupName); // 그룹을 생성하는 코드

            groups[groupCount].memberCount = 0;       // 
            groups[groupCount].maxIndex = 0;          //
            for (int i = 0; i < MAX_CLIENTS; i++) {   // 그룹을 초기화하는 코드
                groups[groupCount].members[i] = 0;    //
            }                                         //
            groups[groupCount].admin = client; // 그룹을 생성한 클라이언트를 관리자로 설정함.

            int result = addClientToGroup(&groups[groupCount], client); // 처음으로 그룹을 생성한 클라이언트를 그룹에 추가하는 코드
            groupCount++;  

            if (result == 2) { // result 값이 2 일 때는 정상 동작 했다는 뜻
                sprintf(response, "Group '%s' created and joined.\n", groupName);
            } 
        }
        send(client, response, strlen(response), 0); // 클라이언트에게 결과 전송
    }
    else if (sscanf(command, "JOIN %s", groupName) == 1) {
        GroupChat* group = findGroupByName(groupName);
        if (group) {
            int result = addClientToGroup(group, client);
            
            if (result == 1) { // 해당 클라이언트가 이미 이 그룹에 멤버일 때
                sprintf(response, "Client '%d' already exists in Group '%s'.\n", client, groupName);
            } else if (result == 2) { // 정상적으로 그룹에 조인
                sprintf(response, "Joined group '%s'.\n", groupName);
            } else // 그룹의 수용인원이 만석일 때
            {
                sprintf(response, "Group '%s' is fulled.\n", groupName);
            }
        }
        else {
            sprintf(response, "Group '%s' does not exist.\n", groupName);
        }
        send(client, response, strlen(response), 0);
    }
    else if (sscanf(command, "LEAVE %s", groupName) == 1) {
        GroupChat* group = findGroupByName(groupName);
        if (group) {
            int result = deleteClientToGroup(group, client);

            if (result == 1) {
                sprintf(response, "Client '%d' leave from Group '%s'.\n", client, groupName);
            }
            else if (result == 2) {
                sprintf(response, "Client '%d' does not exist in Group '%s'.\n", client, groupName);
            }
            else if (result == 3) {
                sprintf(response, "Client '%d' is admin of Group '%s' and can't leave this Group.\n", client, groupName);
            }
        }
        else {
            sprintf(response, "Group '%s' does not exist.\n", groupName);
        }
        send(client, response, strlen(response), 0);
    }
    else if (sscanf(command, "DELETE %s", groupName) == 1) {
        GroupChat* group = findGroupByName(groupName);
        int result = deleteGroupByName(group);
        if (result == 1) {
            sprintf(response, "Group '%s' delete success.\n", groupName);
        } else if (result == 2) {
            sprintf(response, "Group '%s' dose not exist.\n", groupName);
        }
        send(client, response, strlen(response), 0);
    }
    else {
        sprintf(response, "Unknown command: %s\n", command);
        send(client, response, strlen(response), 0);
    }
}

int main() {

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }

    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE; 

    struct addrinfo* bind_address;
    getaddrinfo(0, "8080", &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);

    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
    }

    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    SOCKET clientSockets[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clientSockets[i] = INVALID_SOCKET;
    }

    fd_set master;
    FD_ZERO(&master);
    FD_SET(socket_listen, &master);
    SOCKET max_socket = socket_listen;

    printf("Waiting for connections...\n");
    while (1) {

        fd_set readfds;
        readfds = master;

        if (select(max_socket + 1, &readfds, 0, 0, 0) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

        if (FD_ISSET(socket_listen, &readfds)) {
            struct sockaddr_storage client_address;
            socklen_t client_len = sizeof(client_address);
            SOCKET socket_client = accept(socket_listen, (struct sockaddr*) &client_address, &client_len);
            if (!ISVALIDSOCKET(socket_client)) {
                fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
                return 1;
            }

            bool added = false;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clientSockets[i] == INVALID_SOCKET) {
                    clientSockets[i] = socket_client;
                    FD_SET(socket_client, &master);
                    if (socket_client > max_socket) {
                        max_socket = socket_client;
                    }
                    added = true;
                    char address_buffer[100];
                    getnameinfo((struct sockaddr*)&client_address, client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
                    printf("New connection from %s\n", address_buffer);
                    break;
                }
            }
            if (!added) {
                printf("Maximum clients reached. Connection refused.\n");
                closesocket(socket_client);
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {

            SOCKET socket_client = clientSockets[i];

            if (socket_client != INVALID_SOCKET && FD_ISSET(socket_client, &readfds)) {
                
                char read[BUFFER_SIZE];
                
                int bytesReceived = recv(socket_client, read, BUFFER_SIZE - 1, 0);
                if (bytesReceived <= 0) {
                    printf("Client %d disconnected\n", socket_client);
                    closesocket(socket_client);
                    FD_CLR(socket_client, &master);
                    clientSockets[i] = INVALID_SOCKET;
                }
                else {
                    read[bytesReceived] = '\0';
                    printf("Received: %s from client %d\n", read, socket_client);

                    if (strncmp(read, "CREATE", 6) == 0 || strncmp(read, "JOIN", 4) == 0 || strncmp(read, "LEAVE", 5) == 0 || strncmp(read, "DELETE", 6) == 0) {
                        handleClientCommand(socket_client, read);
                    }
                    else {
                        for (int j = 0; j < groupCount; j++) {
                            for (int k = 0; k < groups[j].memberCount; k++) {
                                if (groups[j].members[k] == socket_client) {
                                    broadcastToGroup(&groups[j], socket_client, read);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    closesocket(socket_listen);
    WSACleanup();
    return 0;
}
