// jack_midi_1.cpp
//
// no timing math to eliminate all possible sources of error aside from the midi output
// send a midi clock message once per frame
// length of time for a frame in seconds is: 1 / 44100 * jack_buffer_size  ( .00145125 at 64 sample buffer size )
// midi clock messages should have 24 PPQ clock messages
// thus sending a clock message once per 15 frames means one clock per 0.0217687074829932 sec
// 0.0217687074829932 * 15 is the period for 1 quarter note = 0.5224489795918368
// therefor tempo if clocking properly should be 114.843 period of clock messages should be 22ms wt

// I have verified on my system both duplicated and dropped clock messages when sending out
// on the IAC buffer, OS X 10.6.7, Jack 0.89. This was detected by syncing Ableton over midi sync,
// playing the same notes and listening to the lag. Occasionally a lurch will happen, and looking at
// the midi monitor, there is either a dropped or duplicated clock message at that time. There is not
// a message out saying that the jack midi buffer was full. Comparing against Reaper providing the sync
// with the same test, the lurches do not ever happen.


#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <signal.h>
#include <iostream>

using std::cout;

/*********************************************************************************/
// Jack globals

#define NUM_AUDIO_PORTS 1
#define MIDI_OUT_BUF_SIZE 1024

// convenience typedef
typedef jack_default_audio_sample_t sample_t;

// create ports, these need to be accessible from the audio callback, hence here 
jack_port_t **audio_out_ports;
jack_port_t *jack_midi_in_port;
jack_port_t *jack_midi_out_port;

// pointer to the jack client that wil be opened for this app
// this is global so the signal hanlder has access to it
jack_client_t *client;

// our audio callback, called once per output buffer samples
int process_audio_jack (jack_nframes_t nframes, void *data){
    // cast the void pointer to our engine
    // Engine *engine = (Engine *)data;
    long int *jack_buffer_index;
    jack_buffer_index = (long int *)data;

    //cout << "frame counter: " << *jack_buffer_index << "\n";    

    // get our output buffers, needs to happen each time process is called
    int num_audio_ports = NUM_AUDIO_PORTS;
    sample_t *audio_output_bufs[ NUM_AUDIO_PORTS ];
    for( int port_index=0; port_index < NUM_AUDIO_PORTS; port_index++ ){
        audio_output_bufs[ port_index ] = (sample_t *) jack_port_get_buffer( audio_out_ports[port_index], nframes);
    }


    // get the midi port buffers
    void* jack_midi_in_port_buf = jack_port_get_buffer(jack_midi_in_port, nframes);
    void* jack_midi_out_port_buf = jack_port_get_buffer(jack_midi_out_port, nframes);

    // pointer that will be assinged to the midi out buffer
    unsigned char *midi_out_msg_buf;  
    jack_midi_clear_buffer(jack_midi_out_port_buf); 
    
    for(jack_nframes_t frame_index=0; frame_index < nframes; frame_index++){
        // write silence to the audio out ports for this test app
        for( int port_index=0; port_index < NUM_AUDIO_PORTS; port_index++ ){
            audio_output_bufs[port_index][frame_index] = (sample_t) 0;   
        }
      
        // to isolate errors, all action happens ONLY on the first frame of the buffer
        //  wait a while before start to hook up ports and know app is settled in
        if( (frame_index == 0) && (*jack_buffer_index >= (15 * 24 * 20 ) ) ){
            // this section ONLY executes for the first frame the buffer

            // XXX: something wrong, occasionally sends two clocks 1ms apart, see bottom
            // also occasionally drops a message

            // first active pass, send start and note
            if( *jack_buffer_index == (15 * 24 * 20) ){
            // send MTC start and note ( one 1 byte message, and 1 3 byte message ) 
                //cout << "Sending start and note, jack_buffer_index: " << *jack_buffer_index << "\n";
                midi_out_msg_buf = jack_midi_event_reserve(jack_midi_out_port_buf, frame_index, 4);
                if( midi_out_msg_buf != 0 ){
                    // clock start message
                    midi_out_msg_buf[0] = (unsigned char) 0xFA;
                    // note out, C3 on channel 1
                    midi_out_msg_buf[1] = (unsigned char) 0x90;
                    midi_out_msg_buf[2] = (unsigned char) 0x24;
                    midi_out_msg_buf[3] = (unsigned char) 0x7F;
                }else{
                    cout << "\nERROR: jack_midi_event_reserve returned NULL buffer\n";
                }
            
            }else if( *jack_buffer_index % (15 * 24) == 0 ){
            // send note offs, notes, and MTC clock every 15 * 24 frames, about 22ms apart, 114 bpm
                //cout << "Sending clock and note, jack_buffer_index: " << *jack_buffer_index << "\n";
                if( midi_out_msg_buf != 0 ){
                    midi_out_msg_buf = jack_midi_event_reserve(jack_midi_out_port_buf, frame_index, 7);
                    midi_out_msg_buf[0] = (unsigned char) 0xF8;
                    // note off to precede the note
                    midi_out_msg_buf[1] = (unsigned char) 0x90;
                    midi_out_msg_buf[2] = (unsigned char) 0x24;
                    midi_out_msg_buf[3] = (unsigned char) 0x00;
                    // note on                
                    midi_out_msg_buf[4] = (unsigned char) 0x90;
                    midi_out_msg_buf[5] = (unsigned char) 0x24;
                    midi_out_msg_buf[6] = (unsigned char) 0x7F;
                }else{
                    cout << "\nERROR: jack_midi_event_reserve returned NULL buffer\n";
                }
            
            }else if( *jack_buffer_index % 15 == 0 ){
            // send clock only every 15 frames
                
                // XXX sometimes this happens only 1 ms apart, or at least appears to from the midi monitor
                // and the hiccup in the sync slave
                if( midi_out_msg_buf != 0 ){
                    //cout << "Sending clock, frame count:" << *jack_buffer_index << "\n";
                    midi_out_msg_buf = jack_midi_event_reserve(jack_midi_out_port_buf, frame_index, 1);
                    midi_out_msg_buf[0] = (unsigned char) 0xF8;
                }else{
                    cout << "\nERROR: jack_midi_event_reserve returned NULL buffer\n";
                }
            }
        }
    }
    // increment the buffer counter only once per buffer render
    *jack_buffer_index += 1;

    // let jack know all is a-ok
    return 0;            
}


void jack_error (const char *desc){
    fprintf (stderr, "JACK error: %s\n", desc);
}

void jack_shutdown (void *arg){
    printf("jack shutting down");    
    exit (1);
}

void signal_handler (int sig){
    jack_client_close (client);
    fprintf (stderr, "\nSignal received, exiting ...\n");
    exit (0);
}

void jack_midi_init(){
    jack_midi_in_port = jack_port_register(client, "midi_in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0); 
    jack_midi_out_port = jack_port_register(client, "midi_out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0); 
}


int main (int argc, char *argv[]){
    
    printf("jack_midi_1 starting.\n");
    
    /* tell the JACK server to call error() whenever it */
    jack_set_error_function (jack_error);
    
    /* try to become a client of the JACK server */
    if( (client = jack_client_new("jack_midi_test_1") ) == 0) {
        fprintf (stderr, "Error creating client, is jack server running?\n");
        return 1;
    }
    printf("Created jack client\n");

    // our frame counter, on -1 we send clock start, after that it counts frames
    long int *jack_buffer_index = new long int;
    *jack_buffer_index = 0;
    // register the jack audio callback, pass pointer to counter as data 
    jack_set_process_callback (client, process_audio_jack, (void *)jack_buffer_index );
    
    // register the signal handlers so we can close the app with signals
    signal (SIGQUIT, signal_handler);
    signal (SIGTERM, signal_handler);
    signal (SIGHUP, signal_handler);
    signal (SIGINT, signal_handler);

    /* register shut down call back */
    jack_on_shutdown (client, jack_shutdown, 0);

    jack_midi_init();

    // register all the output ports, copied from http://jackit.sourceforge.net/cgi-bin/lxr/http/source/example-clients/capture_client.c
    unsigned int num_audio_ports = NUM_AUDIO_PORTS;
    audio_out_ports = (jack_port_t **) malloc (sizeof (jack_port_t *) * num_audio_ports);
    for( int port_index=0; port_index < num_audio_ports; port_index++ ){
        char port_name[64];
        sprintf(port_name, "output %d", port_index+1);
        if( (audio_out_ports[port_index] = jack_port_register(client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0) ) == 0) {
             fprintf (stderr, "cannot register output port \"%s\"!\n", port_name);
             jack_client_close (client);
             exit (1);
        }
    }
    /* tell the JACK server that we are ready to roll */
    printf("Activating jack client\n");
    if (jack_activate (client)) {
        fprintf (stderr, "Error activating jack client");
        return 1;
    }

    // idle until a kill signal calls our handler and exits
    while( 1 ){
        // this is where the QT apps main loop normally goes
    }


}


