#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#define PORT 8080
#define MAX_QUEUE_SIZE 9
#define NUM_THREADS 4

typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int data_ready;
    char ip[16];
    int port;
    int id;
} SharedData;

SharedData shared_data = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, "", 0, 10};

typedef struct Node Node;

struct Node
{
    char ip_address[16];
    int port;
    struct Node *next;
};

Node *createNode(const char *ip_address, int port)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    strncpy(newNode->ip_address, ip_address, sizeof(newNode->ip_address) - 1);
    newNode->ip_address[sizeof(newNode->ip_address) - 1] = '\0'; // Ensure null-terminated string
    newNode->port = port;
    newNode->next = NULL;
    return newNode;
}

Node *insertAtBeginning(Node *head, const char *ip_address, int port)
{
    Node *newNode = createNode(ip_address, port);
    newNode->next = head;
    return newNode;
}

Node *insertAtEnd(Node *head, const char *ip_address, int port)
{
    Node *newNode = createNode(ip_address, port);
    if (head == NULL)
    {
        return newNode;
    }
    Node *temp = head;
    while (temp->next != NULL)
    {
        temp = temp->next;
    }
    temp->next = newNode;
    return head;
}

Node *deleteNode(Node *head, const char *ip_address, int port)
{
    Node *temp = head;
    Node *prev = NULL;

    while (temp != NULL)
    {
        if (strcmp(temp->ip_address, ip_address) == 0 && temp->port == port)
        {
            if (prev != NULL)
            {
                prev->next = temp->next;
            }
            else
            {
                head = temp->next;
            }
            free(temp);
            break;
        }
        prev = temp;
        temp = temp->next;
    }

    return head;
}

void printLinkedList(Node *head)
{
    Node *temp = head;
    while (temp != NULL)
    {
        printf("%s:%d -> ", temp->ip_address, temp->port);
        temp = temp->next;
    }
    printf("NULL\n");
}

void freeLinkedList(Node *head)
{
    Node *temp;
    while (head != NULL)
    {
        temp = head;
        head = head->next;
        free(temp);
    }
}

char IPbuffer[16];
pthread_mutex_t ip_lock = PTHREAD_MUTEX_INITIALIZER;

char *getIPAddress()
{
    char *ip_copy = malloc(sizeof(IPbuffer));
    if (ip_copy == NULL)
    {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&ip_lock);
    strcpy(ip_copy, IPbuffer);
    pthread_mutex_unlock(&ip_lock);

    return ip_copy;
}

void freeIPAddress(char *ip_copy)
{
    free(ip_copy);
}

typedef struct
{
    int messageType;
} CommonMessage;

typedef struct
{
    CommonMessage commonmessage;
    char fileName[256];
    char peerIP[16];
    int portNumber;

} ObtainMessage;

typedef struct
{
    ObtainMessage *obtain[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex3;
    pthread_cond_t not_empty3;
    pthread_cond_t not_full3;
} ObtainQueue;

ObtainQueue obtain_queue;

void init_obtain_queue()
{
    obtain_queue.front = 0;
    obtain_queue.rear = -1;
    obtain_queue.count = 0;
    pthread_mutex_init(&obtain_queue.mutex3, NULL);
    pthread_cond_init(&obtain_queue.not_empty3, NULL);
    pthread_cond_init(&obtain_queue.not_full3, NULL);
}

void enqueue_obtain(ObtainMessage *message)
{
    pthread_mutex_lock(&obtain_queue.mutex3);

    while (obtain_queue.count >= MAX_QUEUE_SIZE)
    {
        pthread_cond_wait(&obtain_queue.not_full3, &obtain_queue.mutex3);
    }

    obtain_queue.rear = (obtain_queue.rear + 1) % MAX_QUEUE_SIZE;
    obtain_queue.obtain[obtain_queue.rear] = message;
    obtain_queue.count++;

    pthread_cond_signal(&obtain_queue.not_empty3);
    pthread_mutex_unlock(&obtain_queue.mutex3);
}

ObtainMessage *dequeue_obtain()
{
    pthread_mutex_lock(&obtain_queue.mutex3);

    while (obtain_queue.count <= 0)
    {
        pthread_cond_wait(&obtain_queue.not_empty3, &obtain_queue.mutex3);
    }

    ObtainMessage *obtain_message = obtain_queue.obtain[obtain_queue.front];
    obtain_queue.front = (obtain_queue.front + 1) % MAX_QUEUE_SIZE;
    obtain_queue.count--;

    pthread_cond_signal(&obtain_queue.not_full3);
    pthread_mutex_unlock(&obtain_queue.mutex3);

    return obtain_message;
}

typedef struct
{
    CommonMessage commonmessage;
    char messageID[19];
    int TTL;
    char fileName[256];
} QueryMessage;

typedef struct
{
    QueryMessage *query[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex2;
    pthread_cond_t not_empty2;
    pthread_cond_t not_full2;
} QueryQueue;

QueryQueue query_queue;

void init_query_queue()
{
    query_queue.front = 0;
    query_queue.rear = -1;
    query_queue.count = 0;
    pthread_mutex_init(&query_queue.mutex2, NULL);
    pthread_cond_init(&query_queue.not_empty2, NULL);
    pthread_cond_init(&query_queue.not_full2, NULL);
}

void enqueue_query(QueryMessage *message)
{
    pthread_mutex_lock(&query_queue.mutex2);

    while (query_queue.count >= MAX_QUEUE_SIZE)
    {
        pthread_cond_wait(&query_queue.not_full2, &query_queue.mutex2);
    }

    query_queue.rear = (query_queue.rear + 1) % MAX_QUEUE_SIZE;
    query_queue.query[query_queue.rear] = message;
    query_queue.count++;

    pthread_cond_signal(&query_queue.not_empty2);
    pthread_mutex_unlock(&query_queue.mutex2);
}

QueryMessage *dequeue_query()
{
    pthread_mutex_lock(&query_queue.mutex2);

    while (query_queue.count <= 0)
    {
        pthread_cond_wait(&query_queue.not_empty2, &query_queue.mutex2);
    }

    QueryMessage *query_message = query_queue.query[query_queue.front];
    query_queue.front = (query_queue.front + 1) % MAX_QUEUE_SIZE;
    query_queue.count--;

    pthread_cond_signal(&query_queue.not_full2);
    pthread_mutex_unlock(&query_queue.mutex2);

    return query_message;
}

typedef struct
{
    CommonMessage commonmessage;
    char messageID[19];
    char fileName[256];
    char peerIP[20];
    int portNumber;
} HitQueryMessage;

typedef struct
{
    HitQueryMessage *hitquery[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex1;
    pthread_cond_t not_empty1;
    pthread_cond_t not_full1;
} HitQueryQueue;

HitQueryQueue hit_queue;

void init_hit_queue()
{
    hit_queue.front = 0;
    hit_queue.rear = -1;
    hit_queue.count = 0;
    pthread_mutex_init(&hit_queue.mutex1, NULL);
    pthread_cond_init(&hit_queue.not_empty1, NULL);
    pthread_cond_init(&hit_queue.not_full1, NULL);
}

void enqueue_hit(HitQueryMessage *message)
{
    pthread_mutex_lock(&hit_queue.mutex1);

    while (hit_queue.count >= MAX_QUEUE_SIZE)
    {
        pthread_cond_wait(&hit_queue.not_full1, &hit_queue.mutex1);
    }

    hit_queue.rear = (hit_queue.rear + 1) % MAX_QUEUE_SIZE;
    hit_queue.hitquery[hit_queue.rear] = message;
    hit_queue.count++;

    pthread_cond_signal(&hit_queue.not_empty1);
    pthread_mutex_unlock(&hit_queue.mutex1);
}

HitQueryMessage *dequeue_hit()
{
    pthread_mutex_lock(&hit_queue.mutex1);

    while (hit_queue.count <= 0)
    {
        pthread_cond_wait(&hit_queue.not_empty1, &hit_queue.mutex1);
    }

    HitQueryMessage *hit_message = hit_queue.hitquery[hit_queue.front];
    hit_queue.front = (hit_queue.front + 1) % MAX_QUEUE_SIZE;
    hit_queue.count--;

    pthread_cond_signal(&hit_queue.not_full1);
    pthread_mutex_unlock(&hit_queue.mutex1);

    return hit_message;
}

typedef struct upstream upstream;

struct upstream
{
    char messageID[19];
    char upstreamID[16];
};

struct upstream stream[40];
int currentIndex = 0;
pthread_mutex_t streamLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t searchLock = PTHREAD_MUTEX_INITIALIZER;

void initializeMutex()
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&searchLock, &attr);
    pthread_mutexattr_destroy(&attr);
}

void addUpstream(const char *messageID, const char *upstreamID)
{
    pthread_mutex_lock(&streamLock);

    strncpy(stream[currentIndex].messageID, messageID, sizeof(stream[currentIndex].messageID) - 1);
    strncpy(stream[currentIndex].upstreamID, upstreamID, sizeof(stream[currentIndex].upstreamID) - 1);

    currentIndex = (currentIndex + 1) % 40;

    pthread_mutex_unlock(&streamLock);
}

char *getUpstreamID(const char *messageID)
{
    pthread_mutex_lock(&streamLock);
    char *notFound = "Not Found";

    for (int i = 0; i < 40; i++)
    {
        if (strcmp(stream[i].messageID, messageID) == 0)
        {
            pthread_mutex_unlock(&streamLock);
            return stream[i].upstreamID;
        }
    }

    pthread_mutex_unlock(&streamLock);
    return notFound;
}

bool isMessageIDInStream(const char *messageID)
{
    pthread_mutex_lock(&streamLock);

    for (int i = 0; i < 40; i++)
    {
        if (strcmp(stream[i].messageID, messageID) == 0)
        {
            pthread_mutex_unlock(&streamLock);
            return true; // MessageID found in the array
        }
    }

    pthread_mutex_unlock(&streamLock);
    return false; // MessageID not found in the array
}

typedef struct
{
    int client_socket;
    char IP[15];
} Request;

typedef struct
{
    Request *requests[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} RequestQueue;

RequestQueue request_queue;

void init_request_queue()
{
    request_queue.front = 0;
    request_queue.rear = -1;
    request_queue.count = 0;
    pthread_mutex_init(&request_queue.mutex, NULL);
    pthread_cond_init(&request_queue.not_empty, NULL);
    pthread_cond_init(&request_queue.not_full, NULL);
}

void enqueue_request(Request *request)
{
    pthread_mutex_lock(&request_queue.mutex);

    while (request_queue.count >= MAX_QUEUE_SIZE)
    {
        pthread_cond_wait(&request_queue.not_full, &request_queue.mutex);
    }

    request_queue.rear = (request_queue.rear + 1) % MAX_QUEUE_SIZE;
    request_queue.requests[request_queue.rear] = request;
    request_queue.count++;

    pthread_cond_signal(&request_queue.not_empty);
    pthread_mutex_unlock(&request_queue.mutex);
}

Request *dequeue_request()
{
    pthread_mutex_lock(&request_queue.mutex);

    while (request_queue.count <= 0)
    {
        pthread_cond_wait(&request_queue.not_empty, &request_queue.mutex);
    }

    Request *request = request_queue.requests[request_queue.front];
    request_queue.front = (request_queue.front + 1) % MAX_QUEUE_SIZE;
    request_queue.count--;

    pthread_cond_signal(&request_queue.not_full);
    pthread_mutex_unlock(&request_queue.mutex);

    return request;
}

int n;

struct Config
{
    char ip[16];
    int port;
};

typedef struct
{
    pthread_t thread_id;
    int thread_num;
} ThreadInfo;

struct ThreadArgs
{
    struct Node *neighbour;
    int fd;
};

struct ThreadArgs1
{
    char ip1[16];
    int port1;
    QueryMessage *message;
};

void *handle_request(void *arg);

bool searchFile(const char *path, const char *filename)
{
    pthread_mutex_lock(&searchLock);
    struct dirent *entry;
    DIR *dp = opendir(path);

    printf("path %s\n", path);

    if (dp == NULL)
    {
        pthread_mutex_unlock(&searchLock);
        return false;
    }

    while ((entry = readdir(dp)))
    {
        if (entry->d_type == DT_DIR)
        {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
            {
                char subdir[1024];
                snprintf(subdir, sizeof(subdir), "%s/%s", path, entry->d_name);
                if (searchFile(subdir, filename))
                {
                    closedir(dp);
                    pthread_mutex_unlock(&searchLock);
                    return true;
                }
            }
        }
        else if (entry->d_type == DT_REG)
        {
            if (strcmp(entry->d_name, filename) == 0)
            {
                closedir(dp);
                pthread_mutex_unlock(&searchLock);
                return true;
            }
        }
    }
    pthread_mutex_unlock(&searchLock);
    return false;
}

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_message(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    time_t raw_time;
    struct tm *info;
    char timestamp[20];
    time(&raw_time);
    info = localtime(&raw_time);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", info);

    pthread_mutex_lock(&log_mutex);

    FILE *file = fopen("output.txt", "a");
    if (file)
    {
        fprintf(file, "[%s] ", timestamp);
        vfprintf(file, format, args);
        fclose(file);
    }

    pthread_mutex_unlock(&log_mutex);

    va_end(args);
}

// handles incoming request in the queue.
void *handle_request(void *arg)
{
    ThreadInfo *thread_info = (ThreadInfo *)arg;

    while (1)
    {
        Request *request = dequeue_request();

        int valread;
        char buffer[1024] = {0};

        valread = read(request->client_socket, buffer, sizeof(buffer));
        CommonMessage *commonMessage = (CommonMessage *)buffer;

        switch (commonMessage->messageType)
        {
        case 0:
        {
            char currentDir[PATH_MAX];
            if (getcwd(currentDir, sizeof(currentDir)) == NULL)
            {
                log_message("Error getting current working directory\n");
                return NULL;
            }

            QueryMessage *queryMessage = (QueryMessage *)buffer;
            log_message("QueryMessage request from %s with messageID %s for file %s\n", request->IP, queryMessage->messageID, queryMessage->fileName);
             bool found = isMessageIDInStream(queryMessage->messageID);
            if (found == false)
            {
                log_message("QueryMessage request from %s with messageID %s for file %s is a new request\n", request->IP, queryMessage->messageID, queryMessage->fileName);
                queryMessage->TTL = queryMessage->TTL - 1;
                if (queryMessage->TTL > 0)
                {
                    enqueue_query(queryMessage);
                    addUpstream(queryMessage->messageID, request->IP);
                    bool found1 = isMessageIDInStream(queryMessage->messageID);
                    if (chdir(currentDir) == -1)
                    {
                        log_message("Error changing directory\n");
                        return NULL;
                    }

                    if (searchFile(".", queryMessage->fileName))
                    {
                    	log_message("File found in local computer requested by %s with ID %s for file %s\n", request->IP, queryMessage->messageID, queryMessage->fileName);
                        HitQueryMessage *hitMessage = (HitQueryMessage *)malloc(sizeof(HitQueryMessage));
                        hitMessage->commonmessage.messageType = 1;
                        char ip[16];
                        char *ipPointer = getIPAddress();
                        strcpy(ip, ipPointer);
                        free(ipPointer);
                        strcpy(hitMessage->messageID, queryMessage->messageID);
                        strcpy(hitMessage->fileName, queryMessage->fileName);
                        strcpy(hitMessage->peerIP, ip);
                        hitMessage->portNumber = 8080;
                        enqueue_hit(hitMessage);
                    }
                }
            }
            else
            {
                log_message("foudn message %s in stream\n", queryMessage->messageID);
            }
            break;
        }
        case 1:
        {
            HitQueryMessage *hitQueryMessage = (HitQueryMessage *)buffer;
            log_message("HitQueryMessage request from %s with messageID %s for file %s\n", request->IP, hitQueryMessage->messageID, hitQueryMessage->fileName);
            enqueue_hit(hitQueryMessage);
            break;
        }
        case 2:
        {
            char command[1047];
            ObtainMessage *obtMessage = (ObtainMessage *)buffer;
            log_message("Download request from %s for file  %s\n", request->IP, obtMessage->fileName);
            snprintf(command, sizeof(command), "find . -type f -name '%s'", obtMessage->fileName);
            FILE *pipe = popen(command, "r");
            if (pipe == NULL)
            {
                printf("Failed to execute command\n");
                break;
            }
            char path1[1047];
            if (fgets(path1, sizeof(path1), pipe) == NULL)
            {
                printf("File not found\n");
                pclose(pipe);
                break;
            }
            size_t len = strlen(path1);
            if (len > 0 && path1[len - 1] == '\n')
            {
                path1[len - 1] = '\0';
            }
            pclose(pipe);
            FILE *file = fopen(path1, "rb");
            if (file == NULL)
            {
                log_message("File opening failed\n");
            }
            else
            {
                log_message("%s opened for download\n", obtMessage->fileName);
            }

            char buffer2[1024];
            size_t bytes_read;
            while ((bytes_read = fread(buffer2, 1, sizeof(buffer2), file)) > 0)
            {
                if (send(request->client_socket, buffer2, bytes_read, 0) == -1)
                {
                    printf("Failed to send file data\n");
                    fclose(file);
                    break;
                }
            }
            fclose(file);
            break;
        }
        default:
            log_message("unkown message type %d\n", commonMessage->messageType);
            break;
        }
        close(request->client_socket);
        free(request);
    }
    return NULL;
}

// server function
void *server_function(void *args)
{
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)args;
    struct Node *neighbour = threadArgs->neighbour;

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int opt = 1;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        log_message("Socket creation failed\n");
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        log_message("setsockopt");
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        log_message("Binding failed\n");
        close(server_socket);
    }

    if (listen(server_socket, 3) == -1)
    {
        log_message("Listening failed\n");
        close(server_socket);
    }

    ThreadInfo threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
    {
        threads[i].thread_num = i;
        if (pthread_create(&(threads[i].thread_id), NULL, handle_request, &(threads[i])) != 0)
        {
            log_message("Thread creation failed\n");
        }
    }

    init_request_queue();

    log_message("Listening\n");

    while (1)
    {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1)
        {
            log_message("Accepting client connection failed\n");
            close(server_socket);
        }

        Request *request = (Request *)malloc(sizeof(Request));
        if (request == NULL)
        {
            log_message("Failed to allocate memory for request\n");
            close(client_socket);
        }
        request->client_socket = client_socket;
        strcpy(request->IP, inet_ntoa(client_addr.sin_addr));
        log_message("Request from client %s enqueued\n", request->IP);
        enqueue_request(request);
    }
    return NULL;
}

void *client_thread(void *args)
{
    struct ThreadArgs1 *threadArgs1 = (struct ThreadArgs1 *)args;
    char ip[16];
    strcpy(ip, threadArgs1->ip1);
    int port = threadArgs1->port1;
    QueryMessage *message = (QueryMessage *)malloc(sizeof(QueryMessage));
    memcpy(message, threadArgs1->message, sizeof(QueryMessage));
    char buffer[sizeof(QueryMessage)];
    memset(&buffer, '\0', sizeof(buffer));
    memcpy(buffer, message, sizeof(QueryMessage));

    int status, valread, client_socket;
    struct sockaddr_in server_address;

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        free(message);
        log_message("Socket creation failed %s\n", ip);
        return NULL;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_address.sin_addr) <= 0)
    {
        free(message);
        log_message("Invalid address/Address not supported %s\n", ip);
        return NULL;
    }

    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        free(message);
        log_message("failed to connect to ip %s\n", ip);
        return NULL;
    }
    log_message("sending query message %s to neighbour server %s", message->messageID, ip);

    send(client_socket, buffer, sizeof(buffer), 0);
    close(client_socket);
    free(message);

    return NULL;
}

void *client_function(void *args)
{
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)args;
    while (1)
    {
        struct Node *neighbour = threadArgs->neighbour;
        QueryMessage *message = dequeue_query();
        pthread_t cthread;
        while (neighbour != NULL)
        {
            struct ThreadArgs1 threadargs1;
            threadargs1.message = malloc(sizeof(QueryMessage));
            strcpy(threadargs1.ip1, neighbour->ip_address);
            threadargs1.port1 = neighbour->port;
            memcpy(threadargs1.message, message, sizeof(QueryMessage));
            if (pthread_create(&(cthread), NULL, client_thread, (void *)&threadargs1) != 0)
            {
                log_message("Error creating client request thread\n");
                free(threadargs1.message);
                exit(EXIT_FAILURE);
            }
            neighbour = neighbour->next;
        }
    }
    return NULL;
}

void *handle_hit_function(void *args)
{
    char ip[16];
    char *ipPointer = getIPAddress();
    strcpy(ip, ipPointer);
    while (1)
    {
        HitQueryMessage *message = dequeue_hit();
        char id[19];
        strcpy(id, message->messageID);
        printf("id in obtain is %s\n", id);
        char *semicolpos = strchr(id, ':');
        size_t comparelen = semicolpos - id;
        if (strncmp(id, ip, comparelen) == 0)
        {
            log_message("File found requested with message id from local computer: %s\n", message->messageID);
            int last_char = atoi(id + strlen(id) - 2);
            printf("last_char is %d\n", last_char);
            if (last_char == shared_data.id)
            {
                pthread_mutex_lock(&shared_data.mutex);
                strcpy(shared_data.ip, message->peerIP);
                shared_data.port = message->portNumber;
                shared_data.data_ready = 1;
                pthread_cond_signal(&shared_data.cond);
                pthread_mutex_unlock(&shared_data.mutex);
            }
        }
        else
        {
            char *ipCopy = getUpstreamID(message->messageID);
            char ipstream[16];
            strcpy(ipstream, ipCopy);
            char buffer[sizeof(HitQueryMessage)];
            memset(&buffer, '\0', sizeof(buffer));
            memcpy(buffer, message, sizeof(HitQueryMessage));
            int status, valread, client_socket;
            struct sockaddr_in server_address;

            if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
            {
                log_message("Socket creation failed\n");
            }

            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(8080);

            if (inet_pton(AF_INET, ipstream, &server_address.sin_addr) <= 0)
            {
                log_message("Invalid address/Address not supported\n");
            }

            if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
            {
                log_message("Connection failed\n");
            }

            send(client_socket, buffer, sizeof(buffer), 0);
            log_message("hit message %s sent back to %s\n", message->messageID, ipstream);
            free(message);
            close(client_socket);
        }
    }
    return NULL;
}

void *obtain_function(void *args)
{
    while (1)
    {
        printf("obtian entered\n");
        ObtainMessage *message = dequeue_obtain();

        int status, valread, client_socket;
        struct sockaddr_in server_address;

        if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
            log_message("Socket creation failed\n");
        }

        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(message->portNumber);

        if (inet_pton(AF_INET, message->peerIP, &server_address.sin_addr) <= 0)
        {
            log_message("Invalid address/Address not supported\n");
        }

        if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        {
            log_message("Connection failed for download to server %s\n", message->peerIP);
        }
        else
        {
            log_message("connected to %s for direct download of file %s\n", message->peerIP, message->fileName);
        }

        char buffer[sizeof(ObtainMessage)];
        memset(&buffer, '\0', sizeof(buffer));
        memcpy(buffer, message, sizeof(ObtainMessage));

        send(client_socket, buffer, sizeof(buffer), 0);

        FILE *received_file = fopen(message->fileName, "w+");
        if (received_file == NULL)
        {
            log_message("File opening failed\n");
            close(client_socket);
        }

        char buffer2[1024];
        size_t bytes_received;
        while ((bytes_received = read(client_socket, buffer2, sizeof(buffer2))) > 0)
        {
            fwrite(buffer2, 1, bytes_received, received_file);
        }
        log_message("Direct Downlaod of file %s done\n", message->fileName);
        fclose(received_file);
        free(message);
        close(client_socket);
    }
    return NULL;
}

void *interface_function(void *args)
{
    char ip[16];
    char *ipCopy = getIPAddress();
    strcpy(ip, ipCopy);
    freeIPAddress(ipCopy);
    int i = 10;
    while (1)
    {
        if (i == 100)
        {
            i = 10;
        }
        char cid[3];
        sprintf(cid, "%d", i);
        printf("cid is: %s\n", cid);
        int option;
        printf("1. Search for a file\n2. obtain a file\n");
        scanf("%d", &option);
        char filename[255];
        printf("Enter file name\n");
        scanf("%s", filename);
        char messageID[19] = "";
        strcat(messageID, ip);
        strcat(messageID, ":");
        int length = strlen(messageID);
        strcat(messageID, cid);
        printf("id is: %s\n", messageID);
        QueryMessage *message = (QueryMessage *)malloc(sizeof(QueryMessage));
        strcpy(message->messageID, messageID);
        strcpy(message->fileName, filename);
        message->TTL = 17;
        message->commonmessage.messageType = 0;
        printf("file: %s\n", message->fileName);
        addUpstream(message->messageID, ip);
        enqueue_query(message);
        ObtainMessage *obtmessage = (ObtainMessage *)malloc(sizeof(ObtainMessage));

        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 3; // Wait for 2 seconds
        int timedone = 0;
        pthread_mutex_lock(&shared_data.mutex);
        while (shared_data.data_ready == 0)
        {
            int result = pthread_cond_timedwait(&shared_data.cond, &shared_data.mutex, &timeout);

            if (result == ETIMEDOUT)
            {
                printf("File not found\n");
                timedone = 1;
                pthread_mutex_unlock(&shared_data.mutex);
                break;
            }
        }
        if (timedone == 0)
        {
            // Read and print data
            obtmessage->commonmessage.messageType = 2;
            strcpy(obtmessage->peerIP, shared_data.ip);
            obtmessage->portNumber = shared_data.port;
            strcpy(obtmessage->fileName, message->fileName);
            // Reset data_ready flag
            shared_data.data_ready = 0;
            shared_data.id = i + 1;

            pthread_mutex_unlock(&shared_data.mutex);
            switch (option)
            {
            case 1:
                printf("IP: %s, Port: %d\n", obtmessage->peerIP, obtmessage->portNumber);
                break;
            case 2:
                printf("File Found at IP: %s, Port: %d\n", obtmessage->peerIP, obtmessage->portNumber);
                enqueue_obtain(obtmessage);
                break;
            }
        }
        free(message);
        i++;
    }

    return NULL;
}

int main(int argc, char *argv[])
{

    FILE *fp;
    char *line = NULL;
    size_t len = 0;

    fp = popen("/sbin/ifconfig enp0s3 | grep 'inet' | cut -d: -f2 | awk '{ print $2}'", "r");
    if (fp == NULL)
    {
        perror("command run failure");
        exit(EXIT_FAILURE);
    }

    if (getline(&line, &len, fp) != -1)
    {
        char *ipAddress = strtok(line, "\n");
        pclose(fp);
        strcpy(IPbuffer, ipAddress);
    }

    Node *neighbour = NULL;
    // reading config file
    FILE *file = fopen(argv[1], "r");
    if (file == NULL)
    {
        fprintf(stderr, "Error opening the config file.\n");
        return 1;
    }

    struct Config config;
    while (fscanf(file, "%s %d", config.ip, &config.port) == 2)
    {
        neighbour = insertAtEnd(neighbour, config.ip, config.port);
        n++;
    }

    fclose(file);

    init_query_queue();
    init_hit_queue();
    init_obtain_queue();
    initializeMutex();

    // creating threads for server and client function
    pthread_t server;
    pthread_t client;
    pthread_t handle_hit;
    pthread_t obtain;
    pthread_t interface;

    struct ThreadArgs threadArgs;
    threadArgs.neighbour = neighbour;

    // creating server function
    if (pthread_create(&(server), NULL, server_function, (void *)&threadArgs) != 0)
    {
        fprintf(stderr, "Error creating server thread\n");
        return 2;
    }

    // creating client function
    if (pthread_create(&(client), NULL, client_function, (void *)&threadArgs) != 0)
    {
        fprintf(stderr, "Error creating client thread\n");
        return 2;
    }

    // creating client function
    if (pthread_create(&(handle_hit), NULL, handle_hit_function, NULL) != 0)
    {
        fprintf(stderr, "Error creating client thread\n");
        return 2;
    }

    if (pthread_create(&(obtain), NULL, obtain_function, NULL) != 0)
    {
        fprintf(stderr, "Error creating client thread\n");
    }

    if (pthread_create(&(interface), NULL, interface_function, NULL) != 0)
    {
        fprintf(stderr, "Error creating client thread\n");
        return 2;
    }

    pthread_join(server, NULL);
    pthread_join(client, NULL);
    pthread_join(handle_hit, NULL);
    pthread_join(obtain, NULL);
    pthread_join(interface, NULL);
    return 0;
}
