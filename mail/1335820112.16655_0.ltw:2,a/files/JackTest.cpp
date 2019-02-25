#include "StdAfx.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <jack\jack.h>
#include <jack\control.h>

#ifdef WIN32
#define snprintf sprintf_s
#endif
volatile int done = 0;
//////////////////////JACK SERVER STUFF////////////////////////////////

// To put in the control.h interface ??
static jackctl_driver_t * jackctl_server_get_driver(jackctl_server_t *server, const char *driver_name)
{
    const JSList * node_ptr = jackctl_server_get_drivers_list(server);

    while (node_ptr) {
        if (strcmp(jackctl_driver_get_name((jackctl_driver_t *)node_ptr->data), driver_name) == 0) {
            return (jackctl_driver_t *)node_ptr->data;
        }
        node_ptr = jack_slist_next(node_ptr);
    }

    return NULL;
}

static jackctl_internal_t * jackctl_server_get_internal(jackctl_server_t *server, const char *internal_name)
{
    const JSList * node_ptr = jackctl_server_get_internals_list(server);

    while (node_ptr) {
        if (strcmp(jackctl_internal_get_name((jackctl_internal_t *)node_ptr->data), internal_name) == 0) {
            return (jackctl_internal_t *)node_ptr->data;
        }
        node_ptr = jack_slist_next(node_ptr);
    }

    return NULL;
}

static jackctl_parameter_t * jackctl_get_parameter(const JSList * parameters_list, const char * parameter_name)
{
    while (parameters_list) {
        if (strcmp(jackctl_parameter_get_name((jackctl_parameter_t *)parameters_list->data), parameter_name) == 0) {
            return (jackctl_parameter_t *)parameters_list->data;
        }
        parameters_list = jack_slist_next(parameters_list);
    }

    return NULL;
}

static void notify_server_start(const char* server_name)
{}
static void notify_server_stop(const char* server_name)
{}

////////////////////////////END SERVER STUFF///////////////////////////////////

void port_connect_callback(jack_port_id_t a, jack_port_id_t b, int connect, void* arg)
{
    done = 1;
}

jack_port_t *output_port, *input_port_speaker, *input_port_mic;
jack_port_t *output_testPort;
jack_client_t *client;

#ifndef M_PI
#define M_PI  (3.14159265)
#endif

#define TABLE_SIZE   (200)
typedef struct
{
    float sine[TABLE_SIZE];
    int left_phase;
    int right_phase;
}
paTestData;

static void signal_handler(int sig)
{
	jack_client_close(client);
	fprintf(stderr, "signal received, exiting ...\n");
	Sleep(5000); //So I can see it
	exit(0);
}

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 * This client follows a simple rule: when the JACK transport is
 * running, copy the input port to the output.  When it stops, exit.
 */

int
process (jack_nframes_t nframes, void *arg)
{
    printf(".");
	return 0;      
}

int
main (int argc, char *argv[])
{
	const char *client_name = "SimpleJack.exe";
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;
	paTestData data;
	int i;

	//Jack Server Stuff//
	jackctl_server_t * server_ctl = NULL;
	const JSList * server_parameters;
    sigset_t signals;
    //const char* driver_name = "dummy";
    //const char* client_name = "audioadapter";
	char* master_driver_name = "portaudio";
	char* args[2] = {"-d","DirectSound::PTT Headset (PTT Headset Adapter)"};
	//DirectSound::PTT Headset (PTT Headset Adapter)
	//DirectSound::Internal (IDT High Definition Audio CODEC)
	//DirectSound::Primary Sound Driver
	char** master_driver_args = NULL;
	int master_driver_nargs = 3;
	jackctl_parameter_t* param;
	union jackctl_parameter_value value;
	jackctl_driver_t * master_driver_ctl;
	// Assume that we fail.
    int return_value = -1;
    bool notify_sent = false;
	////////////////////////////

	for( i=0; i<TABLE_SIZE; i++ )
    {
        data.sine[i] = 0.025 * (float) sin( ((double)i/(double)TABLE_SIZE) * M_PI * 4. );
    }
    data.left_phase = data.right_phase = 0;

	//JACK SERVER STUFF//
	server_ctl = jackctl_server_create(NULL, NULL);
	if (server_ctl == NULL) 
	{
        fprintf(stderr, "Failed to create server object\n");
        return -1;
    }
    server_parameters = jackctl_server_get_parameters(server_ctl);

 	//Set to Realitime//
	param = jackctl_get_parameter(server_parameters, "realtime");
    if (param != NULL) {
        value.b = true;
        jackctl_parameter_set_value(param, &value);
    }

	//Set to Sync//
	param = jackctl_get_parameter(server_parameters, "sync");
    if (param != NULL) {
        value.b = true;
        jackctl_parameter_set_value(param, &value);
    }

	// Master driver
    master_driver_ctl = jackctl_server_get_driver(server_ctl, master_driver_name);
	master_driver_args = (char **) malloc(sizeof(char *) * master_driver_nargs);
    master_driver_args[0] = master_driver_name;

    for (i = 1; i < master_driver_nargs; i++) {
        master_driver_args[i] = args[i-1];
    }
    if (master_driver_ctl == NULL)
	{
        fprintf(stderr, "Unknown driver \"%s\"\n", master_driver_name);
        jackctl_server_destroy(server_ctl);
		if (notify_sent) {
			notify_server_stop(server_name);
		}
    }

	 if(jackctl_driver_params_parse(master_driver_ctl, master_driver_nargs, master_driver_args))
	 {
		fprintf(stderr, "Unknown driver params \"%s\"\n", master_driver_args);
        jackctl_server_destroy(server_ctl);
		if (notify_sent) {
			notify_server_stop(server_name);
		}
	 }

	signals = jackctl_setup_signals(0);

    // Open server
    if (! jackctl_server_open(server_ctl, master_driver_ctl)) {
        fprintf(stderr, "Failed to open server\n");
        jackctl_server_destroy(server_ctl);
		if (notify_sent) {
			notify_server_stop(server_name);
		}
    }
	
	// Start the server
    if (!jackctl_server_start(server_ctl)) {
        fprintf(stderr, "Failed to start server\n");
        jackctl_server_close(server_ctl);
    }

	notify_server_start(server_name);
    notify_sent = true;
    return_value = 0;

    // Waits for signal
    //jackctl_wait_signals(signals); //this blocks
	Sleep(1000);
	/////////////////////////



	/* open a client connection to the JACK server */

	client = jack_client_open (client_name, options, &status, server_name);
	if (client == NULL) {
		fprintf (stderr, "jack_client_open() failed, "
			 "status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			fprintf (stderr, "Unable to connect to JACK server\n");
		}
	}
	if (status & JackServerStarted) {
		fprintf (stderr, "JACK server started\n");
	}
	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		fprintf (stderr, "unique name `%s' assigned\n", client_name);
	}

	/* tell the JACK server to call `process()' whenever
	   there is work to be done.
	*/

	jack_set_process_callback (client, process, &data);

	/* tell the JACK server to call `jack_shutdown()' if
	   it ever shuts down, either entirely, or if it
	   just decides to stop calling us.
	*/

	//jack_on_shutdown (client, jack_shutdown, 0);

	/* create the ports */

	output_port = jack_port_register (client, "output_processed",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);

	output_testPort = jack_port_register (client, "output_test",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);

	input_port_speaker = jack_port_register (client, "input_speaker",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsInput, 0);

	input_port_mic = jack_port_register (client, "input_mic",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsInput, 0);

	if ((output_port == NULL) || (input_port_speaker == NULL) || (input_port_mic == NULL)) {
		fprintf(stderr, "no more JACK ports available\n");
		exit (1);
	}

	/* Tell the JACK server that we are ready to roll.  Our
	 * process() callback will start running now. */

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		exit (1);
	}

	//NOW THAT MY SERVER AND CLIENT ARE RUNNING LETS MAKE A NEW CLIENT AND MAKE CONNECTIONS USING IT//
	
	jack_client_t *connectClient;
	jack_status_t status2;
	char *theServerName = "server_jack_default";
	jack_options_t options2 = JackNoStartServer;
	char *my_name = "jack_connect";
	jack_port_t *src_port = 0;
	jack_port_t *dst_port = 0;
	jack_port_t *port1 = 0;
	jack_port_t *port2 = 0;
	char portA[300];
	char portB[300];
	int use_uuid=0;
	int connecting, disconnecting;
	int port1_flags, port2_flags;
	int rc = 1;

	connecting = 1;
	disconnecting = 0;

	
	//Notice that things are running fine at this point while we sleep
	Sleep(5000);

	/* try to become a client of the JACK server */
	if ((connectClient = jack_client_open (my_name, options2, &status2, theServerName)) == 0) {
		fprintf (stderr, "JACK server not running?\n");
		return 1;
	}

    jack_set_port_connect_callback(connectClient, port_connect_callback, NULL);

	/* find the two ports */
	snprintf( portA, sizeof(portA), "%s", "SimpleJack.exe:output_processed");
	snprintf( portB, sizeof(portB), "%s", "system:playback_1" );

	if ((port1 = jack_port_by_name(connectClient, portA)) == 0) {
		fprintf (stderr, "ERROR %s not a valid port\n", portA);
    }
	if ((port2 = jack_port_by_name(connectClient, portB)) == 0) {
		fprintf (stderr, "ERROR %s not a valid port\n", portB);
    }

	port1_flags = jack_port_flags (port1);
	port2_flags = jack_port_flags (port2);

	if (port1_flags & JackPortIsInput) {
		if (port2_flags & JackPortIsOutput) {
			src_port = port2;
			dst_port = port1;
		}
	} else {
		if (port2_flags & JackPortIsInput) {
			src_port = port1;
			dst_port = port2;
		}
	}

	if (!src_port || !dst_port) {
		fprintf (stderr, "arguments must include 1 input port and 1 output port\n");
	}

    /* tell the JACK server that we are ready to roll */
    if (jack_activate (connectClient)) {
        fprintf (stderr, "cannot activate client");
    }

	/* connect the ports. Note: you can't do this before
	   the client is activated (this may change in the future).
	*/

	if (connecting) {
		if (jack_connect(connectClient, jack_port_name(src_port), jack_port_name(dst_port))) {
            fprintf (stderr, "cannot connect client, already connected?\n");
		}
	}
	if (disconnecting) {
		if (jack_disconnect(connectClient, jack_port_name(src_port), jack_port_name(dst_port))) {
            fprintf (stderr, "cannot disconnect client, already disconnected?\n");
		}
	}

    // Wait for connection/disconnection to be effective
    while(!done) {
    #ifdef WIN32
        Sleep(10);
    #else
        usleep(10000);
    #endif
    }

	////////////////////////////////////////////////////////////////////////////////////////
    /* install a signal handler to properly quits jack client */
#ifdef WIN32
	signal(SIGINT, signal_handler);
    signal(SIGABRT, signal_handler);
	signal(SIGTERM, signal_handler);
#else
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
#endif

	/* keep running until the Ctrl+C */

	while (1) {
	#ifdef WIN32 
		Sleep(1000);
	#else
		sleep (1);
	#endif
	}

	jack_client_close (client);
	exit (0);
}
