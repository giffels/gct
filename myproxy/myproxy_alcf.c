/*
 * myproxy_alcf.c
 *
 * admin load credential from file
 *
 */

#include "myproxy_common.h"	/* all needed headers included here */

#define MYPROXY_DEFAULT_PROXY  "/tmp/myproxy-proxy"
#define	SECONDS_PER_HOUR (60 * 60)
static int dn_as_username = 0;

static char usage[] = \
"\n"\
"Syntax: myproxy-admin-load-credential [-l username] [-r retrievers] [-R renewers] ...\n"\
"        myproxy-admin-load-credential [-usage|-help] [-version]\n"\
"\n"\
"   Options\n"\
"       -h | --help                       Displays usage\n"
"       -u | --usage                                    \n"
"                                                      \n"
"       -v | --verbose                    Display debugging messages\n"
"       -V | --version                    Displays version\n"
"       -s | --storage        <directory> Specifies the credential storage directory\n"
"       -c | --certfile       <filename>  Certificate file name\n"
"       -y | --keyfile        <filename>  Key file name\n"
"       -l | --username       <username>  Username for the delegated proxy\n"
"       -t | --proxy_lifetime  <hours>    Lifetime of proxies delegated by\n" 
"                                         server (default 12 hours)\n"
"       -a | --allow_anonymous_retrievers Allow credentials to be retrieved\n"
"                                         with just username/passphrase\n"
"       -A | --allow_anonymous_renewers   Allow credentials to be renewed by\n"
"                                         any client (not recommended)\n"
"       -x | --regex_dn_match             Set regular expression matching mode\n"
"                                         for following policy options\n"
"       -X | --match_cn_only              Set CN matching mode (default)\n"
"                                         for following policy options\n"
"       -r | --retrievable_by <dn>        Allow specified entity to retrieve\n"
"                                         credential\n"
"       -R | --renewable_by   <dn>        Allow specified entity to renew\n"
"                                         credential\n"
"       -d | --dn_as_username             Use the proxy certificate subject\n"
"                                         (DN) as the default username,\n"
"                                         instead of the LOGNAME env. var.\n"
"       -k | --credname       <name>      Specifies credential name\n"
"       -K | --creddesc       <desc>      Specifies credential description\n"
"\n";

struct option long_options[] =
{
  {"help",                  no_argument, NULL, 'h'},
  {"usage",                 no_argument, NULL, 'u'},
  {"certfile",	      required_argument, NULL, 'c'},
  {"keyfile",	      required_argument, NULL, 'y'},
  {"proxy_lifetime",  required_argument, NULL, 't'},
  {"storage",         required_argument, NULL, 's'},
  {"username",        required_argument, NULL, 'l'},
  {"verbose",               no_argument, NULL, 'v'},
  {"version",               no_argument, NULL, 'V'},
  {"dn_as_username",        no_argument, NULL, 'd'},
  {"allow_anonymous_retrievers", no_argument, NULL, 'a'},
  {"allow_anonymous_renewers", no_argument, NULL, 'A'},
  {"retrievable_by",  required_argument, NULL, 'r'},
  {"renewable_by",    required_argument, NULL, 'R'},
  {"regex_dn_match",        no_argument, NULL, 'x'},
  {"match_cn_only", 	    no_argument, NULL, 'X'},
  {"credname",	      required_argument, NULL, 'k'},
  {"creddesc",	      required_argument, NULL, 'K'},
  {0, 0, 0, 0}
};

/*colon following an option indicates option takes an argument */

static char short_options[] = "uhl:vVdr:R:xXaAk:K:t:c:y:s:";

static char *certfile   = NULL;  /* certificate file name */
static char *keyfile    = NULL;  /* key file name */

static char version[] =
"myproxy-alcf version " MYPROXY_VERSION " (" MYPROXY_VERSION_DATE ") "  "\n";

void init_arguments(int argc, char *argv[], myproxy_creds_t *my_creds);
int file2buf(const char filename[], char **buf);
int makeproxy(const char certfile[], const char keyfile[],
	      const char proxyfile[]);

int main(int argc, char *argv[])
{
    SSL_CREDENTIALS *creds;
    myproxy_creds_t my_creds = {0};
    char proxyfile[64] = "";
    int rval=1;

    myproxy_log_use_stream (stderr);

    creds = ssl_credentials_new();
    init_arguments (argc, argv, &my_creds);

    if (certfile == NULL) {
	fprintf (stderr, "Specify certificate file with -c option\n");
	fprintf(stderr, usage);
	goto cleanup;
    }

    if (keyfile == NULL) {
	fprintf (stderr, "Specify key file with -y option\n");
	fprintf(stderr, usage);
	goto cleanup;
    }

    sprintf(proxyfile, "%s.%u", MYPROXY_DEFAULT_PROXY, (unsigned)getpid());
    /* Remove proxyfile if it already exists. */
    ssl_proxy_file_destroy(proxyfile);
    verror_clear();

    if (makeproxy(certfile, keyfile, proxyfile) < 0) {
	fprintf(stderr, "Failed to create temporary credentials file.\n");
	goto cleanup;
    }
		
    if (my_creds.username == NULL) { /* set default username */
	if (dn_as_username) {
	    if (ssl_get_base_subject_file(proxyfile,
					  &my_creds.username)) {
		fprintf(stderr,
			"Cannot get subject name from your certificate\n");
		goto cleanup;
	    }
	} else {
	    char *username = NULL;
	    if (!(username = getenv("LOGNAME"))) {
		fprintf(stderr, "Please specify a username.\n");
		goto cleanup;
	    }
	    my_creds.username = strdup(username);
	}
    }

    if (ssl_get_base_subject_file(proxyfile,
				  &my_creds.owner_name)) {
	fprintf(stderr,
		"Cannot get subject name from certificate.\n");
	goto cleanup;
    }
    my_creds.location = strdup(proxyfile);

    if (myproxy_creds_store(&my_creds) < 0) {
	myproxy_log_verror();
	fprintf (stderr, "Unable to store credentials. %s\n",
		 verror_get_string()); 
    } else {
	fprintf (stdout, "Credential stored successfully\n");
    }

    rval = 0;
 cleanup:
    if (proxyfile[0]) ssl_proxy_file_destroy(proxyfile);
    return rval;
}


void 
init_arguments(int argc, 
	       char *argv[], myproxy_creds_t *my_creds)
{
    extern char *gnu_optarg;
    int arg;
    int expr_type = MATCH_CN_ONLY;  /*default */

    my_creds->lifetime = SECONDS_PER_HOUR * MYPROXY_DEFAULT_DELEG_HOURS;

    while((arg = gnu_getopt_long(argc, argv, short_options, 
				 long_options, NULL)) != EOF) 
    {
        switch(arg) 
        {  
        case 's': /* set the credential storage directory */
	    myproxy_set_storage_dir(gnu_optarg);
	    break;
	
	case 'c': /* credential file name*/
	    certfile = strdup (gnu_optarg);
	    break;
	case 'y': /* key file name */
	    keyfile = strdup (gnu_optarg);
	    break;
        case 'u': 	/* print help and exit */
            fprintf(stderr, usage);
            exit(1);
       	    break;
	case 't': 	/* Specify proxy lifetime in hours */
	    my_creds->lifetime = SECONDS_PER_HOUR * atoi(gnu_optarg);
	    break;        
	case 'h': 	/* print help and exit */
            fprintf(stderr, usage);
            exit(1);
            break;
        case 'l':	/* username */
	    my_creds->username = strdup (gnu_optarg);
	    break;
	case 'v':	/* verbose */
	    myproxy_debug_set_level(1);
	    break;
        case 'V':       /* print version and exit */
            fprintf(stderr, version);
            exit(1);
            break;
	

	case 'r':   /* retrievers list */
	    if (my_creds->renewers) {
		fprintf(stderr, "-r is incompatible with -A and -R.  A credential may not be used for both\nretrieval and renewal.  If both are desired, upload multiple credentials with\ndifferent names, using the -k option.\n");
		exit(1);
	    }
	    if (my_creds->retrievers) {
		fprintf(stderr, "Only one -a or -r option may be specified.\n");
		exit(1);
	    }
	    if (expr_type == REGULAR_EXP)  /*copy as is */
		my_creds->retrievers = strdup (gnu_optarg);
	    else
	    {
		my_creds->retrievers = (char *)malloc(strlen(gnu_optarg)+5);
		strcpy (my_creds->retrievers, "*/CN=");
		myproxy_debug("authorized retriever %s",
			      my_creds->retrievers);
		my_creds->retrievers = strcat(my_creds->retrievers,
					      gnu_optarg);
	    }
	    break;
	case 'R':   /* renewers list */
	    if (my_creds->retrievers) {
		fprintf(stderr, "-R is incompatible with -a and -r.  A credential may not be used for both\nretrieval and renewal.  If both are desired, upload multiple credentials with\ndifferent names, using the -k option.\n");
		exit(1);
	    }
	    if (my_creds->renewers) {
		fprintf(stderr, "Only one -A or -R option may be specified.\n");
		exit(1);
	    }
	    if (expr_type == REGULAR_EXP)  /*copy as is */
		my_creds->renewers = strdup (gnu_optarg);
	    else
	    {
		my_creds->renewers = (char *)malloc(strlen(gnu_optarg)+6);
		strcpy (my_creds->renewers, "*/CN=");
		myproxy_debug("authorized renewer %s",
			      my_creds->renewers);
		my_creds->renewers = strcat (my_creds->renewers,gnu_optarg);
	    }
	    break;
	case 'd':   /* use the certificate subject (DN) as the default
		       username instead of LOGNAME */
	    dn_as_username = 1;
	    break;
	case 'x':   /*set expression type to regex*/
	    expr_type = REGULAR_EXP;
	    myproxy_debug("expr-type = regex");
	    break;
	case 'X':   /*set expression type to common name*/
	    expr_type = MATCH_CN_ONLY;
	    myproxy_debug("expr-type = CN");
	    break;
	case 'a':  /*allow anonymous retrievers*/
	    if (my_creds->renewers) {
		fprintf(stderr, "-a is incompatible with -A and -R.  A credential may not be used for both\nretrieval and renewal.  If both are desired, upload multiple credentials with\ndifferent names, using the -k option.\n");
		exit(1);
	    }
	    if (my_creds->retrievers) {
		fprintf(stderr, "Only one -a or -r option may be specified.\n");
		exit(1);
	    }
	    my_creds->retrievers = strdup ("*");
	    myproxy_debug("anonymous retrievers allowed");
	    break;
	case 'A':  /*allow anonymous renewers*/
	    if (my_creds->retrievers) {
		fprintf(stderr, "-A is incompatible with -a and -r.  A credential may not be used for both\nretrieval and renewal.  If both are desired, upload multiple credentials with\ndifferent names, using the -k option.\n");
		exit(1);
	    }
	    if (my_creds->renewers) {
		fprintf(stderr, "Only one -A or -R option may be specified.\n");
		exit(1);
	    }
	    my_creds->renewers = strdup ("*");
	    myproxy_debug("anonymous renewers allowed");
	    break;
	case 'k':  /*credential name*/
	    my_creds->credname = strdup (gnu_optarg);
	    break;
	case 'K':  /*credential description*/
	    my_creds->creddesc = strdup (gnu_optarg);
	    break;

        default:        /* print usage and exit */ 
            fprintf(stderr, usage);
	    exit(1);
            break;	
        }
    }
}

int file2buf(const char filename[], char **buf)
{
    int fd, size, rval;
    char *b;

    if ((fd = open(filename, O_RDONLY)) < 0) {
	perror("open"); return -1;
    }
    if ((size = (int)lseek(fd, 0, SEEK_END)) < 0) {
	perror("lseek"); return -1;
    }
    if (lseek(fd, 0, SEEK_SET) < 0) {
	perror("lseek"); return -1;
    }
    *buf = b = (char *)malloc(size+1);
    if (b == NULL) {
	perror("malloc"); return -1;
    }
    while (size) {
	if ((rval = read(fd, b, size)) < 0) {
	    perror("read"); return -1;
	}
	size -= rval;
	b += rval;
    }
    *b = '\0';
    
    return 0;
}

int makeproxy(const char certfile[], const char keyfile[],
	      const char proxyfile[]) 
{
    static char BEGINCERT[] = "-----BEGIN CERTIFICATE-----";
    static char ENDCERT[] = "-----END CERTIFICATE-----";
    static char BEGINKEY[] = "-----BEGIN RSA PRIVATE KEY-----";
    static char ENDKEY[] = "-----END RSA PRIVATE KEY-----";
    char *certbuf=NULL, *keybuf=NULL, *certstart, *certend, *keystart, *keyend;
    int return_value = -1, size, rval, fd=0;

    /* Read the certificate(s) into a buffer. */
    if (file2buf(certfile, &certbuf) < 0) {
	fprintf(stderr, "Failed to read %s\n", certfile);
	goto cleanup;
    }

    /* Read the key into a buffer. */
    if (file2buf(keyfile, &keybuf) < 0) {
	fprintf(stderr, "Failed to read %s\n", keyfile);
	goto cleanup;
    }

    /* Open the output file. */
    if ((fd = open(proxyfile, O_CREAT | O_EXCL | O_WRONLY,
		   S_IRUSR | S_IWUSR)) < 0) {
	fprintf(stderr, "open(%s) failed: %s\n", proxyfile, strerror(errno));
	goto cleanup;
    }

    /* Write the first certificate. */
    if ((certstart = strstr(certbuf, BEGINCERT)) == NULL) {
	fprintf(stderr, "%s doesn't contain '%s'.\n", certfile, BEGINCERT);
	goto cleanup;
    }

    if ((certend = strstr(certstart, ENDCERT)) == NULL) {
	fprintf(stderr, "%s doesn't contain '%s'.\n", certfile, ENDCERT);
	goto cleanup;
    }
    certend += strlen(ENDCERT);
    size = certend-certstart;

    while (size) {
	if ((rval = write(fd, certstart, size)) < 0) {
	    perror("write");
	    goto cleanup;
	}
	size -= rval;
	certstart += rval;
    }
    if (write(fd, "\n", 1) < 0) {
	perror("write");
	goto cleanup;
    }

    /* Write the key. */
    if ((keystart = strstr(keybuf, BEGINKEY)) == NULL) {
	fprintf(stderr, "%s doesn't contain '%s'.\n", keyfile, BEGINKEY);
	goto cleanup;
    }

    if ((keyend = strstr(keystart, ENDKEY)) == NULL) {
	fprintf(stderr, "%s doesn't contain '%s'.\n", keyfile, ENDKEY);
	goto cleanup;
    }
    keyend += strlen(ENDKEY);
    size = keyend-keystart;

    while (size) {
	if ((rval = write(fd, keystart, size)) < 0) {
	    perror("write");
	    goto cleanup;
	}
	size -= rval;
	keystart += rval;
    }
    if (write(fd, "\n", 1) < 0) {
	perror("write");
	goto cleanup;
    }

    /* Write any remaining certificates. */
    while ((certstart = strstr(certstart, BEGINCERT)) != NULL) {

	if ((certend = strstr(certstart, ENDCERT)) == NULL) {
	    fprintf(stderr, "Can't find matching '%s' in %s.\n", ENDCERT,
		    certfile);
	    goto cleanup;
	}
	certend += strlen(ENDCERT);
	size = certend-certstart;

	while (size) {
	    if ((rval = write(fd, certstart, size)) < 0) {
		perror("write");
		goto cleanup;
	    }
	    size -= rval;
	    certstart += rval;
	}
	if (write(fd, "\n", 1) < 0) {
	    perror("write");
	    goto cleanup;
	}
    }

    return_value = 0;

 cleanup:
    if (certbuf) free(certbuf);
    if (keybuf) free(keybuf);
    if (fd) close(fd);

    return return_value;
}
