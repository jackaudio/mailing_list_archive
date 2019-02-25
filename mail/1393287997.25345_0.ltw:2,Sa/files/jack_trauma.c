#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include </usr/local/include/jack/jack.h>

#define SOCKET_ERROR   ((int)-1)
#define UDPLITE_SEND_CSCOV  10
#define UDPLITE_RECV_CSCOV  11
#define SOL_UDPLITE  136
#define IP_DONTFRAG 1
#define IPPROTO_UDPLITE

char *remote_address = "127.0.0.1";
int remote_port = 12345;
int socketfd;
int addr_size = sizeof(struct sockaddr_in);
struct sockaddr_in *Remote;
pthread_t comm_thread;

int udp_payload_bytes;
int udp_payload_samples;
int n_packets;

int amiserver = 0;
int current_stage = 0;

const char *default_in_port_basename = "in";
const char *default_out_port_basename = "out";
char *default_client_name = "jack_trauma";
const char *default_server_name = "jack_srv";
short n_channels = 4;
short channels_offset = 0;
short *received;
jack_default_audio_sample_t *silence;
short current_channels = 0;
jack_port_t **jack_ports;
jack_client_t *client;
jack_options_t options = JackNullOption;
jack_nframes_t bufsize;
jack_default_audio_sample_t **packets;
int jack_active = 0;


void jack_shutdown (void *arg){
    printf("Kicked out by JACK server\n");
}

void jack_error_handler(const char *arg){
    if(jack_active){
        printf("JACK died\n");
        jack_active = 0;
        current_stage = 0;
    }
}

// - - - - - - - - - - -
// process() for sender
// - - - - - - - - - - -
int process_server(jack_nframes_t nframes, void *arg){

    int i;
    jack_default_audio_sample_t *tmp;

    for(i=0;i<n_channels;i++){
        tmp = jack_port_get_buffer(jack_ports[i], nframes);
        memcpy(packets[i]+1, tmp, nframes*sizeof(jack_default_audio_sample_t));
        sendto(socketfd, (void*)packets[i], udp_payload_bytes, 0, (struct sockaddr*)&Remote[i], addr_size);
    }

    return 0;
}

// - - - - - - - - - - - -
// process() for receiver
// - - - - - - - - - - - -

int process_client(jack_nframes_t nframes, void *arg){

    int i = 0;
	int r;
    int packet_ind;
    unsigned int addr_size = sizeof(struct sockaddr_in);
    jack_default_audio_sample_t *tmp;

	memset(received, 0, n_channels*sizeof(short));

	do {
       	r = recvfrom(socketfd, (void*)packets[i], udp_payload_bytes, MSG_PEEK, (struct sockaddr*)&Remote[i], &addr_size); 
		if (r>0){
			if(current_channels < n_channels){
				current_channels++;
			}
       		memset(packets[i]+1, 0, bufsize*sizeof(jack_default_audio_sample_t));
       		r = recvfrom(socketfd, (void*)packets[i], udp_payload_bytes, 0, (struct sockaddr*)&Remote[i], &addr_size);
       		packet_ind = (int)packets[i][0] % n_channels;
			received[packet_ind] = 1;
       		tmp = jack_port_get_buffer(jack_ports[packet_ind], nframes);
       		memcpy(tmp, packets[i]+1, nframes*sizeof(jack_default_audio_sample_t));
       	}
		else {
			if(current_channels > 0){
				current_channels--;
			}
		}
		i++;
    } while(i < current_channels);

	for(i=0; i<n_channels; i++){
		if(!received[i]){
       		tmp = jack_port_get_buffer(jack_ports[i], nframes);
       		memcpy(tmp, silence, nframes*sizeof(jack_default_audio_sample_t));
			
		}
	}

    return 0;
}


// stage 4 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// endless loop, all ok
int roll(){
    while(jack_active){
        sleep(1);
    }
    return 1;
}


// stage 3 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// initialization of data structure for incoming/outcoming packages
// and JACK client enabling
int process_init(){

    int i;

    n_packets = n_channels;

    packets = malloc((n_packets)*sizeof(jack_default_audio_sample_t*));
    for(i=0; i<n_packets; i++){
        packets[i] = calloc((bufsize + 1), sizeof(jack_default_audio_sample_t));
        packets[i][0] = (jack_default_audio_sample_t)(i + channels_offset);
    }

    jack_on_shutdown(client, jack_shutdown, 0);
    jack_set_error_function(jack_error_handler);

    if(amiserver){
        jack_set_process_callback(client, process_server, Remote);
    }
    else{
		received = calloc(n_channels, sizeof(short));
		silence = calloc(bufsize, sizeof(jack_default_audio_sample_t));
        jack_set_process_callback(client, process_client, 0);
    }

    if(jack_activate(client)){
        fprintf ( stderr, "cannot activate client" );
        return 1;
    }

    jack_active = 1;

    printf("JACK Client activated\n");

    return 0;

}


// stage 2 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// UDP Lite socket initialization
int udp_init(){

    struct sockaddr_in Local;
    int ris, i;


    socketfd == socket(AF_INET, SOCK_DGRAM, IPPROTO_UDPLITE);
    if (socketfd == SOCKET_ERROR) {
        printf ("socket() failed, Err: %d \"%s\"\n", errno,strerror(errno));
        exit(1);
    }

    int tos = 0x10;
    if (setsockopt(socketfd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos))) {
        printf ("setting tos failed, Err: %d \"%s\"\n", errno,strerror(errno));
        exit(1);
    }

    int reuseaddr = 1;
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr))) {
        printf ("setting reuseaddr failed, Err: %d \"%s\"\n", errno,strerror(errno));
        exit(1);
    }

    int coverage = 12;
    if (amiserver) {
        if (setsockopt(socketfd, SOL_UDPLITE, UDPLITE_SEND_CSCOV, &coverage, sizeof(int))) {
            printf ("setting udplite SENDER coverage to %d failed, Err: %d \"%s\"\n", coverage, errno,strerror(errno));
            exit(1);
        }
    } else {
        if (setsockopt(socketfd, SOL_UDPLITE, UDPLITE_RECV_CSCOV, &coverage, sizeof(int))) {
            printf ("setting udplite RECEIVER coverage to %d failed, Err: %d \"%s\"\n", coverage, errno,strerror(errno));
            exit(1);
        }
    }

    int nofrag = 1;
    if (setsockopt(socketfd, IPPROTO_IP, IP_DONTFRAG, &nofrag, sizeof(nofrag))) {
            printf ("setting nofrag option failed, Err: \"%d\" %s \n", errno,strerror(errno));
            exit(1);
    }

    int flags;
    flags = fcntl(socketfd,F_GETFL,0);
    fcntl(socketfd, F_SETFL, flags | O_NONBLOCK);

    Local.sin_family = AF_INET;
    Local.sin_addr.s_addr = htonl(INADDR_ANY);

    Remote = calloc(n_channels, sizeof(struct sockaddr_in));
    if(amiserver){
        for(i=0; i<n_channels; i++){
            Remote[i].sin_family = AF_INET;
            Remote[i].sin_addr.s_addr = inet_addr(remote_address);
            Remote[i].sin_port = htons(remote_port);
        }
    }

    else{
        Local.sin_port = htons(remote_port);
    }

    ris = bind(socketfd, (struct sockaddr*) &Local, sizeof(Local));
    if (ris == SOCKET_ERROR)  {
        printf ("bind() failed, Err: %d \"%s\"\n",errno,strerror(errno));
        exit(1);
    }

    // 20 IP + 8 UDP LITE
    udp_payload_bytes = (bufsize + 1) * sizeof(jack_default_audio_sample_t);
    udp_payload_samples = bufsize;

    return 0;
}


// stage 1 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// JACK client initialization
int jack_init(){

    short i;
    char* client_name;
    jack_status_t status;

    client = jack_client_open (default_client_name, options, &status, default_server_name);

    if (client == NULL){
        return 1;
    }

    if (status & JackServerStarted){
        fprintf(stderr, "JACK server started\n");
    }

    if (status & JackNameNotUnique){
        client_name = jack_get_client_name ( client );
        fprintf ( stderr, "unique name `%s' assigned\n", client_name );
    }

    char port_name[16];
    jack_ports = (jack_port_t**)calloc(n_channels, sizeof(jack_port_t*));

    if(amiserver){
        for(i=0;i<n_channels;i++){
                sprintf(port_name, "%s%d", default_in_port_basename, i+1);
                jack_ports[i] = jack_port_register(client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
                if(jack_ports[i] == NULL){
                    fprintf ( stderr, "no more JACK ports available\n" );
                    return 1;
                }
        }
    }

    else{
        for(i=0;i<n_channels;i++){
            sprintf(port_name, "%s%d", default_out_port_basename, i+1);
            jack_ports[i] = jack_port_register(client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            if(jack_ports[i] == NULL){
                fprintf ( stderr, "no more JACK ports available\n" );
                return 1;
            }
        }
    }

    bufsize = jack_get_buffer_size(client);
    printf("JACK Client initialized\n");
    printf("JACK Buffer size = %d\n", bufsize);
    printf("Audio Channels %hd\n", n_channels);

    return 0;
}


// stage 0 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int cleanall(){

    if(client){
        jack_deactivate(client);
        jack_client_close(client);
        printf("JACK client closed\n");
    }

    free(jack_ports);
    free(packets);
	free(received);
	free(silence);

    return 0;
}


// parse CLI options - - - - - - - - - - - - - - - - - - - - - - -

int print_help_and_quit(){
    printf("\nUsage: jacknet2 [OPTIONS]\n\n");
    printf("-s\t\t run in send mode\n");
    printf("-c CHANNELS\t number of input channels (default=4)\n");
    printf("-a ADDR\t\t set remote address (default=localhost)\n");
    printf("-o OFFSET\t set offset for channels numbers\n");
    printf("-p PORT\t\t set UDP Lite port in use (default=12345)\n");
	printf("-n NAME\t\t set JACK client name\n");
    printf("\n");
    exit(0);
}

parse_options(int argc, char** argv){

    int opt;
    extern char *optarg;
    const char* optstring = "sc:a:o:p:n:h";

    while((opt = getopt(argc, argv, optstring)) != -1){

        switch(opt){

            case 's':
                amiserver = 1;
                break;
            case 'c':
                n_channels = atoi(optarg);
                break;
            case 'a':
                if(amiserver){
                    remote_address = optarg;
                }
            case 'p':
                remote_port = atoi(optarg);
                break;
			case 'n':
				default_client_name = optarg;
				break;
            case 'o':
                if(amiserver){
                    channels_offset = atoi(optarg);
                }
                break;
            case 'h':
                print_help_and_quit();
                break;
            case ':':
                print_help_and_quit();
                break;
            case '?':
                print_help_and_quit();
                break;

        }
    }
    return 0;
}


int main(int argc, char** argv){

    const int n_stages = 5;
    int (*stage[n_stages])();
    stage[0] = cleanall;
    stage[1] = jack_init;
    stage[2] = udp_init;
    stage[3] = process_init;
    stage[4] = roll;

    parse_options(argc, argv);

    while(1){

        if((*stage[current_stage])() == 0){
            current_stage = (current_stage + 1) % n_stages;
        }
        else{
            current_stage = 0;
        }
    }

    return 0;
}
