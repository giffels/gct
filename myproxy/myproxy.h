/*
 * myproxy.h
 *
 * Defines protocol for communciation with myproxy-server
 *
 */

#ifndef __MYPROXY_H
#define __MYPROXY_H

#include "gsi_socket.h"

/* Maximum and minimum lengths for password */
#define MAX_PASS_LEN  10
#define MIN_PASS_LEN  5

/* Define default myproxy-server */
#define MYPROXYSERVER_PORT     7512
#define MYPROXYSERVER_HOST     "localhost"

/* Default proxy lifetime */
#define MYPROXY_DEFAULT_HOURS  84

/* Location of default proxy */
#define MYPROXY_DEFAULT_PROXY  "/tmp/myproxy-proxy"

/* myproxy client protocol information */
#define MYPROXY_VERSION_STRING      "VERSION="
#define MYPROXY_COMMAND_STRING      "COMMAND="
#define MYPROXY_USERNAME_STRING     "USERNAME="
#define MYPROXY_PASSPHRASE_STRING   "PASSPHRASE="
#define MYPROXY_LIFETIME_STRING     "LIFETIME="


/* myproxy server protocol information */
#define MYPROXY_RESPONSE_STRING     "RESPONSE="
#define MYPROXY_ERROR_STRING        "ERROR="

/* Protocol commands */
typedef enum
{
    MYPROXY_GET_PROXY,
    MYPROXY_PUT_PROXY,
    MYPROXY_INFO_PROXY,
    MYPROXY_DESTROY_PROXY
} myproxy_proto_request_type_t;

/* server response codes */
typedef enum
{
    MYPROXY_OK_RESPONSE,
    MYPROXY_ERROR_RESPONSE 
} myproxy_proto_response_type_t;

/* client/server socket attributes */
typedef struct 
{
  char *pshost;	
  int psport;
  int socket_fd;
  GSI_SOCKET *gsi_socket; 
} myproxy_socket_attrs_t;

/* A client request object */
typedef struct
{
    char                         *version;
    char                         *username;
    char                         passphrase[MAX_PASS_LEN+1];
    myproxy_proto_request_type_t command_type;
    int                          hours;
} myproxy_request_t;

/* A server response object */
typedef struct
{
  char                          *version;
  myproxy_proto_response_type_t response_type;
  char                          *error_string;
} myproxy_response_t;


/*
 * myproxy_init_client()
 *
 * Create a generic client by craeting a GSI socket and connecting to a a host 
 *
 * returns the file descriptor of the connected socket or -1 if an error occurred  
 */
int myproxy_init_client(myproxy_socket_attrs_t *attrs);

/*
 * myproxy_init_server()
 *
 * Create a generic server by creating a GSI socket and a bind, listen, accept  
 *
 * returns the file descriptor of the connected socket or -1 if an error occurred  
 */
int myproxy_init_server(myproxy_socket_attrs_t *attrs);

/*
 * myproxy_authenticate_init()
 * 
 * Uses gssapi to perform client-side authentication
 *
 * returns -1 if unable to authenticate, 0 if authentication successful
 */ 
int myproxy_authenticate_init(myproxy_socket_attrs_t *attr);

/*
 * myproxy_authenticate_accept()
 * 
 * Uses gssapi to perform server-side authentication and retrieve the client's DN
 *
 * returns -1 if unable to authenticate, 0 if authentication successful
 */ 
int myproxy_authenticate_accept(myproxy_socket_attrs_t *attr, 
                                char *client_name, const int namelen);

/*
 * myproxy_serialize_request()
 * 
 * Serialize a request object into a buffer to be sent over the network.
 *
 * returns the number of characters put into the buffer 
 * (not including the trailing NULL)
 */
int  myproxy_serialize_request(const myproxy_request_t *request, 
			    char *data, const int datalen);


/*
 * myproxy_deserialize_request()
 * 
 * Deserialize a buffer into a request object.
 *
 * returns 0 if succesful, otherwise -1
 */
int  myproxy_deserialize_request(const char *data, const int datalen, 
                                 myproxy_request_t *request);

/*
 * myproxy_serialize_response()
 * 
 * Serialize a response object into a buffer to be sent over the network.
 *
 * returns the number of characters put into the buffer 
 * (not including the trailing NULL)
 */
int
myproxy_serialize_response(const myproxy_response_t *response, 
                           char *data, const int datalen); 

/*
 * myproxy_deserialize_response()
 *
 * Serialize a response object into a buffer to be sent over the network.
 *
 * returns the number of characters put into the buffer 
 * (not including the trailing NULL)
 */
int myproxy_deserialize_response(myproxy_response_t *response, 
			    const char *data, const int datalen);

/*
 * myproxy_send()
 * 
 * Sends a buffer with possible encryption done via GSI
 *
 * returns -1 if GSI_SOCKET_write_buffer failed or 0 on success
 */
int myproxy_send(myproxy_socket_attrs_t *attrs,
                 const char *data, const int datalen);

/*
 * myproxy_recv()
 *
 * Receives a buffer with possible encryption done via GSI 
 *
 * returns GSI_SOCKET_read_buffer()
 * 
 */
int  myproxy_recv(myproxy_socket_attrs_t *attrs,
			   char *data, const int datalen);

/*
 * myproxy_init_delegation()
 *
 * Delegates a proxy based on the credentials found in file location delegfile
 *
 * returns 0 on success, -1 on error 
 */
int myproxy_init_delegation(myproxy_socket_attrs_t *attrs, const char *delegfile);

/*
 * myproxy_accept_delegation()
 *
 * Accepts delegated credentials into file location data
 *
 * returns 0 on success, -1 on error 
 */
int myproxy_accept_delegation(myproxy_socket_attrs_t *attrs, char *data, const int datalen);

/*
 * myproxy_destroy()
 * 
 * Frees up memory used for creating request, response and socket objects 
 */
void myproxy_destroy(myproxy_socket_attrs_t *attrs, myproxy_request_t *request, myproxy_response_t *response);

/*---------------------------- Helper functions ----------------------------*/ 

/*
 * convert_message()
 *
 * Searches a buffer and locates varname. Stores contents of varname into line
 * e.g. convert_message(buf, "VERSION=", version, sizeof(version));
 * If multiple varnames exist, the contents are concatenated following a newline
 *
 * return the number of characters copied into the line 
 * (not including the terminating '\0'), or -1 if varname not found or error
 */
int  convert_message(const char *buffer, const char *varname, 
		     char *line, const int linelen); 


#endif /* __MYPROXY_H */
