#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h> //FD stuff
#include <time.h>
#include <sqlite3.h>

#define CLIENT_SIZE 20
#define USERNAME_SIZE 20


/*Establishes connection to db, returns sqlite3 db*/
sqlite3 *connect_to_db(){
    sqlite3 *db;
    
    int rc = sqlite3_open("chat.db", &db);
    
    if (rc != SQLITE_OK) {
        
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }
    return db;
}

/*Creates the tables in the db, returns 0 if successful*/
int create_dB(){
    sqlite3 *db = connect_to_db();
    if (db == NULL){
        return -1;
    }
    char *sql =
    "CREATE TABLE IF NOT EXISTS Messages(Private INT NOT NULL, Author TEXT NOT NULL, Recipient TEXT, Timestamp TEXT NOT NULL, Message TEXT NOT NULL);"
    "CREATE TABLE IF NOT EXISTS Users(Username TEXT NOT NULL, Password TEXT NOT NULL, PubKey TEXT);";
    
    char *err_msg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    
    if (rc != SQLITE_OK ) {
        
        fprintf(stderr, "SQL error: %s\n", err_msg);
        
        sqlite3_free(err_msg);
        sqlite3_close(db);
        
        return -1;
    }
    
    sqlite3_close(db);
    return 0;
}

/* Creates a server socket */
int create_server_socket(unsigned short port) {
    int fd, r;
    struct sockaddr_in addr ;
    
    /* create TCP socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("There was an error with the server socket \n\n");
        exit(0);
    }
    
    /* bind socket to specified port on all interfaces */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    r = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
    if (r != 0) {
        /* handle error */
        printf("There was an error with the binding \n\n");
        exit(0);
    }
    
    
    /* start listening for incoming client connections */
    r = listen(fd, 5);
    if (r != 0) {
        printf("There was an error with listening \n\n");
        exit(0);
    }
    
    return fd;
}

/* Accepts connection of new client */
int accept_connection(int serverfd) {
    int connfd ;
    
    /* accept incoming connection */
    connfd = accept(serverfd , NULL, NULL);
    if (connfd < 0) {
        exit(1);
    }
    
    /* server continues here */
    return connfd;
}

/*Inserts user in db, returns 0 if successfull*/
int insert_user_in_db(char *username, char *password, char *pubKey){
    sqlite3 *db = connect_to_db();
    
    if (db == NULL)
    {
        return -1;
    }
    
    char *sql;
    asprintf(&sql,"INSERT INTO Users VALUES('%s','%s','%s');",username,password,pubKey);
    char *err_msg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    
    if (rc != SQLITE_OK )
    {
        
        fprintf(stderr, "SQL error: %s\n", err_msg);
        
        sqlite3_free(err_msg);
        sqlite3_close(db);
        
        return -1;
    }
    
    sqlite3_close(db);
    return 0;
}

/*Inserts message in db, returns 0 if successfull*/
int insert_message_in_db(int private, char author[], char *recipient, char *timestamp, char *message){
    sqlite3 *db = connect_to_db();
    
    if (db == NULL){
        return -1;
    }
    char *sql;
    asprintf(&sql,"INSERT INTO Messages VALUES('%d','%s','%s','%s','%s');",private,author,recipient,timestamp,message);
    char *err_msg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    
    if (rc != SQLITE_OK ) {
        
        fprintf(stderr, "SQL error: %s\n", err_msg);
        
        sqlite3_free(err_msg);
        sqlite3_close(db);
        
        return -1;
    }
    
    sqlite3_close(db);
    
    return 0;
}

struct user
{
    int    sd;
    char   username [USERNAME_SIZE];
};

/* Handels public messages */
void public_message(char buffer[BUFSIZ], char author[], struct user active_users[], int max_clients)
{
    
    time_t time_stamp = time(NULL);
    char buf[BUFSIZ];
    snprintf(buf, sizeof (buf), "%.19s %s: %s", ctime(&time_stamp),author,buffer);
    
    char timestamp[30];
    snprintf(timestamp,sizeof (timestamp),"%.19s",ctime(&time_stamp));
    
    insert_message_in_db(0, author, NULL, timestamp, buffer);
    
    for(int t = 0; t<max_clients;t++)  //loop through fds to pass on msg received
    {
        send(active_users[t].sd,buf,sizeof(buf),0);
    }
    
}

/* Handles output of the SELECT request, called by multiple functions */
static int callback(void *data, int argc, char **argv, char **azColName){
    /* Called by is_username_in_db which was called by register or login */
    if(data==0)
    {
        return 1;
    }
    
    int i;
    int *t = (int*)data;
    char c_sd[2];
    snprintf(c_sd,sizeof(c_sd),"%d",*t);
    int sd = atoi(c_sd);
    
    FILE *stream;
    char *buf;
    size_t len;
    stream = open_memstream(&buf, &len);
    
    for(i = 0; i<argc; i++)
    {
        if(i == argc-1)
        {
            fprintf(stream, "%s\n", argv[i]);
        }
        else
        {
            fprintf(stream, "%s: ", argv[i]);
        }
    }
    fclose(stream);
    send(sd,buf,len,0);
    free(buf);
    return 0;
    
    
}

/* Checks if username is in db and if that's the case also ckecks if pw is correct with help of callback */
int is_username_in_db(char *username, char *password){
    sqlite3 *db = connect_to_db();
    
    if (db == NULL)
    {
        
        return -1;
    }
    
    char *sql;
    char *zErrMsg = 0;
    if(password==0){
        asprintf(&sql,"SELECT Username,Password FROM Users WHERE Username = '%s';",username);
    } else {
        asprintf(&sql,"SELECT Username,Password FROM Users WHERE Username = '%s' AND Password ='%s';",username,password);
    }
    
    int rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    
    
    if( rc != SQLITE_OK ) {
        sqlite3_close(db);
        return -1;
    }
    
    sqlite3_close(db);
    return 0;
}

/* Gets connection to db to get old messages from the db and sends it through with the help of callback */
char get_old_messages(struct user active_users[], char username[], int sd){
    
    char *zErrMsg = 0;
    char *sql;
    int* data = &sd;
    sqlite3 *db = connect_to_db();
    if (db == NULL){
        return '\0';
    }
    asprintf(&sql,"SELECT Timestamp, Author, Message from Messages WHERE Recipient IN ('(null)','%s') OR Author = '%s';",username, username);
    
    /* Execute SQL statement */
    int rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
    
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);
    return '\0';
}

/* Checks if user is already logged in on another client and returns index of a free space in the array */
int add_user(struct user active_users[], char username[], int sd){
    
    for(int i = 0; i<CLIENT_SIZE; i++){
        if(!strcmp(active_users[i].username,username)||active_users[i].sd == sd){
            send(sd, "Only one active session per user allowed.",41,0);
            return -1;
        }
    }
    
    for(int i = 0; i<CLIENT_SIZE; i++){
        if(active_users[i].sd == 0){
            return i;
        }
    }
    
    send(sd, "Something went wrong. Try again later.",38,0);
    return -2;
}

/* Checks if user has been authorized, returns index if that's the case */
int user_authorized(int sd, struct user active_users[]){
    for (int i = 0; i<CLIENT_SIZE;i++){
        if(active_users[i].sd==sd){
            return i;
        }
    }
    return -1;
}

/* Handles private messages */
void send_private_message(char author[USERNAME_SIZE], char recipient[USERNAME_SIZE], char buffer[BUFSIZ],struct user active_users[CLIENT_SIZE]){
    time_t time_stamp = time(NULL);
    char buf[BUFSIZ];
    snprintf(buf, sizeof (buf), "%.19s %s: %s", ctime(&time_stamp),author,buffer);
    
    char timestamp[30];
    snprintf(timestamp,sizeof (timestamp),"%.19s",ctime(&time_stamp));
    
    insert_message_in_db(1, author, recipient, timestamp, buffer);
    
    for(int t = 0; t<CLIENT_SIZE;t++)  //loop through fds to pass on msg received
    {
        if(!strcmp(active_users[t].username,recipient)||!strcmp(active_users[t].username,author)){
            send(active_users[t].sd,buf,sizeof(buf),0);
        }
    }
}

/*Manages the server*/
void run_server(int prov_port){
    struct user active_users[CLIENT_SIZE];
    char buffer [BUFSIZ];
    int server_fd, clients[CLIENT_SIZE], max_clients = CLIENT_SIZE, i, valread, sd, max_sd;
    int port = prov_port;
    fd_set readfds;
    
    for(i = 0; i<CLIENT_SIZE;i++){
        active_users[i].sd = 0;
        for (int j = 0; j<USERNAME_SIZE;j++){
            active_users[i].username[j] = '\0';
        }
    }
    
    for (i=0; i<max_clients;i++)
    { //initialise all client's to 'usable'
        clients[i] = 0;
    }
    
    server_fd = create_server_socket(port); //set up the server socket
    printf("Begin listening on port: %d \n", port);
    while(1){                                           //got help from geeksforgeeks website, help started here
        FD_ZERO(&readfds);                              //start up readfds
        FD_SET(server_fd, &readfds);                    //set servers socket to it
        max_sd = server_fd;
        
        for (i=0; i<max_clients; i++){                  //Assign all clients sd to the readfds
            sd = clients[i];
            
            if (sd> 0){
                FD_SET (sd,&readfds);
            }
            if (sd>max_sd){
                max_sd = sd;
            }
        }
        select(max_sd +1, &readfds, NULL, NULL, NULL);  //see any if there is any activity
        
        if (FD_ISSET(server_fd, &readfds))              //Check if activity was client attempting to connect to server
        {
            for(i = 0; i< max_clients; i++)             //find 'usable' client spot in array
            {
                if(clients[i] ==0)
                {
                    
                    clients[i] = accept_connection(server_fd);  //add client once spot found
                    
                    break;
                }
            }
            
        }
        
        for (i =0; i<max_clients; i++)              //if activity heard but not for connecting clients
        {                                            //looping through to find which socket activity was heard on
            sd =clients[i];
            int index =user_authorized(sd, active_users);
            if(FD_ISSET(sd, &readfds))              //if activity heard with this clients fd
            {
                if ((valread=read(sd,buffer,BUFSIZ)) ==0)  //if client is disconnecting
                {
                    close (sd);
                    char buf[BUFSIZ];
                    snprintf(buf, BUFSIZ,"user %s exits.",active_users[index].username);
                    clients[i] = 0;
                    
                    
                    for(int j = 0; j<max_clients; j++){
                        if(active_users[j].sd == sd) {
                            active_users[j].sd = 0;
                            bzero(active_users[j].username, sizeof(USERNAME_SIZE));
                            break;
                        }
                    }
                
                    public_message(buf, "server", active_users,max_clients);
                    //setting array slot back to 'usable'
                    
                }
                else
                {
                    
                    char *pos;
                    if (((pos=strchr(buffer, '\n')) != NULL))
                    {
                        *pos = '\0';
                    }
                    else
                    {
                        /* input too long for buffer */
                        bzero(buffer, sizeof(buffer));
                        send(sd,"Message is too long.\n",22,0);
                        break;
                    }
                    
                    char *pos1;
                    if (((pos1=strchr(buffer, ';')) != NULL))
                    {
                        bzero(buffer, sizeof(buffer));
                        send(sd,"Invalid charcters.\n",20,0);
                        break;
                    }
                    
                    char buf[BUFSIZ];
                    snprintf(buf, BUFSIZ,"%s",buffer);
                    char *token = strtok(buf, " ");
                    char *start[4];
                    for (int i = 0;i<4;i++)
                    {
                        if(token != NULL)
                        {
                            start[i] = token;
                            token = strtok(NULL, " ");
                        }else
                        {
                            start[i] = NULL;
                            break;
                        }
                    }
                    if(index==-1){
                        if (strcmp(start[0],"/login")==0)
                        {
                            if(start[1]!=0 && start[2]!=0 && start[3]==0){
                                /* check if username is in db and if password is correct*/
                                if(is_username_in_db(start[1], start[2])!=-1)
                                {
                                    send(sd, "Username or password incorrect.\n",33,0);
                                    break;
                                }
                                /* add to active users*/
                                int a = add_user(active_users, start[1], sd);
                                if (a < 0)
                                {
                                    break;
                                }
                                active_users[a].sd = sd;
                                for(int k = 0; k<USERNAME_SIZE;k++){
                                    active_users[a].username[k] = start[1][k];
                                }
                                
                                send(sd, "You've been successfully logged in.\n",36,0);
                                get_old_messages(active_users, start[1], sd);
                            } else {
                                send(sd, "You've used wrong entries.\n",26,0);
                            }
                        }
                        else if(strcmp(start[0],"/register")==0)
                        {
                            if(start[1]!=0 && start[2]!=0 && start[3]==0){
                                
                                /* check if username is in db */
                                if(is_username_in_db(start[1],NULL)==-1)
                                {
                                    send(sd, "Username already taken.\n",25,0);
                                    break;
                                }
                                
                                /* Check if username has 20 characters */
                                if(strlen(start[1])>USERNAME_SIZE){
                                    send(sd, "Username is too long (max 20 characters).\n",43,0);
                                    break;
                                }
                                
                                /* add to active users*/
                                int a = add_user(active_users, start[1], sd);
                                if (a < 0)
                                {
                                    break;
                                }
                                
                                active_users[a].sd = sd;
                                for(int k = 0; k<USERNAME_SIZE;k++){
                                    active_users[a].username[k] = start[1][k];
                                }
                                
                                insert_user_in_db(start[1], start[2], NULL);
                                send(sd, "You've been successfully registered.\n",36,0);
                                get_old_messages(active_users, start[1], sd);
                                
                            } else {
                                send(sd, "You've used wrong entries.\n",26,0);
                            }
                        } else {
                            send(sd, "You need to be logged in.\n",25,0);
                            break;
                        }
                    } else
                    {
                        if(start[0][0] =='/')
                        {
                            
                            if(strcmp(start[0],"/users")==0 && start[1]==0)
                            {
                                int t;
                                FILE *stream;
                                char *buf;
                                size_t len;
                                stream = open_memstream(&buf, &len);
                                fprintf(stream, "Users logged in:\n");
                                
                                for(t = 0; t<CLIENT_SIZE; t++)
                                {
                                    if(active_users[t].sd>0)
                                    {
                                        fprintf(stream, "%s\n", active_users[t].username);
                                    }
                                }
                                
                                fclose(stream);
                                send(sd,buf,len,0);
                                free(buf);
                                break;
                            }
                            else
                            {
                                printf("invalid command\n");
                                break;
                            }
                        }
                        else if(start[0][0] =='@')
                        {
                            int length =strlen(start[0])-1;
                            char recipient [length];
                            for(int j = 0; j<length;j++){
                                recipient[j]=start[0][j+1];
                            }
                            recipient[length]='\0';
                            send_private_message(active_users[index].username,recipient,buffer,active_users);
                            break;
                        }
                        else //public message
                        {
                            buffer[valread] ='\0';
                            public_message(buffer,active_users[index].username, active_users, max_clients);
                            bzero(buffer,sizeof(buffer));
                            break;
                        }
                    }
                }
                break;
            }
        }                                     //help from geeksforgeeks ends here, variations were made
        
    }
}

int main(int argc, char *argv[]){
    
    if(create_dB()==-1)
    {
        printf("Problem with db creation\n");
        exit(1);
    }
    
    int port = atoi(argv[1]);
    run_server(port);
    
    return 0;
    
    
}

