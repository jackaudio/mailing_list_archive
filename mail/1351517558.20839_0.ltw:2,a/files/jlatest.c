// gcc -o jlatest jlatest.c `pkg-config --cflags --libs jack`

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <jack/jack.h>

jack_port_t*       j_output_port = NULL;
jack_client_t*     j_client = NULL;
jack_nframes_t     j_bufsiz = 0;

void cleanup(int sig) {
  if (j_client) {
    jack_deactivate(j_client);
    jack_client_close (j_client);
  }
  printf("bye.\n");
  exit(0);
}

int process (jack_nframes_t nframes, void *arg) {
  jack_default_audio_sample_t *out = jack_port_get_buffer (j_output_port, nframes);
  memset(out, 0, sizeof(float) * nframes);
  return 0;
}

int jack_graph_cb(void *arg) {
  jack_latency_range_t jlty;
  jack_port_get_latency_range(j_output_port, JackPlaybackLatency, &jlty);
  printf("GRAPH-CB: port latency: min: %d max: %d\n", jlty.min, jlty.max);
  return 0;
}

void jack_latency_cb(jack_latency_callback_mode_t mode, void *arg) {
  if (mode != JackPlaybackLatency) return;
  jack_latency_range_t jlty;
  jack_port_get_latency_range(j_output_port, JackPlaybackLatency, &jlty);
#if 0
  // test,debug
  jlty.min=jlty.max=4096;
  jack_port_set_latency_range(j_output_port, mode, &jlty);
#endif
  printf("LATENCY-CB: port latency: min: %d max: %d\n", jlty.min, jlty.max);
}

void init_jack() {
  jack_status_t status;
  jack_options_t options = JackNullOption;
  j_client = jack_client_open("jltest", options, &status);
  if (j_client == NULL) {
    fprintf (stderr, "jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      fprintf (stderr, "Unable to connect to JACK server\n");
    }
    exit(1);
  }
  jack_set_process_callback (j_client, process, 0);
#if 0
  /* NB. the latency callback is not supposed to be used to query
   * but only to set the latency.
   * This is used for the purpose of debugging.
   */
  jack_set_latency_callback (j_client, jack_latency_cb, NULL);
#endif
  jack_set_graph_order_callback (j_client, jack_graph_cb, NULL);

  if ((j_output_port = jack_port_register (j_client, "ltc", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0)) == 0) {
    fprintf (stderr, "cannot register jack output port \"ltc\".\n");
    cleanup(0);
  }

  j_bufsiz = jack_get_buffer_size(j_client);

  if (jack_activate (j_client)) {
    fprintf (stderr, "cannot activate client");
    cleanup(0);
  }
}

void jconnect(char * port) {
  const char **ports = jack_get_ports(j_client, port, NULL, JackPortIsInput);
  if (ports == NULL) {
    fprintf(stderr, "port '%s' not found\n", port);
    return;
  }
  if (jack_connect (j_client, jack_port_name(j_output_port), ports[0])) {
    fprintf (stderr, "cannot connect output port %s to %s\n", jack_port_name (j_output_port), ports[0]);
    return;
  }
  jack_free (ports);
}

void jdconnect(char * port) {
  const char **ports = jack_get_ports(j_client, port, NULL, JackPortIsInput);
  if (ports == NULL) {
    fprintf(stderr, "port '%s' not found\n", port);
    return;
  }
  if (jack_disconnect (j_client, jack_port_name(j_output_port), ports[0])) {
    fprintf (stderr, "cannot disconnect output port %s to %s\n", jack_port_name (j_output_port), ports[0]);
    return;
  }
  jack_free (ports);
}

int main (int argc, char **argv) {
  printf("---- Starting..\n");
  printf("---- Port latency should be 0\n");
  init_jack();

  sleep(1);
  printf("---- connecting to playback1");
  printf(" - Port latency should be N*%d\n", j_bufsiz);
  jconnect("system:playback_1");
  sleep(1);
  printf("---- connecting to playback2");
  printf(" - Port latency should be N*%d\n", j_bufsiz);
  jconnect("system:playback_2");
  sleep(1);
  printf("---- disconnecting from playback2");
  printf(" - Port latency should be N*%d\n", j_bufsiz);
  jdconnect("system:playback_2");
  sleep(1);
  printf("---- disconnecting from playback1");
  printf(" - Port latency should be 0\n");
  jdconnect("system:playback_1");
  sleep(1);
  printf("---- connecting to playback1");
  printf(" - Port latency should be N*%d\n", j_bufsiz);
  jconnect("system:playback_1");
  sleep(1);
  printf("---- disconnecting from playback1");
  printf(" - Port latency should be 0\n");
  jdconnect("system:playback_1");
  sleep(1);

  cleanup(0);
  return(0);
}

/* vi:set ts=8 sts=2 sw=2: */
