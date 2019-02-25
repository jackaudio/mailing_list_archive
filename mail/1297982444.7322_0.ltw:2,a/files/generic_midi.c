/*
 * Generic MIDI <-> JACK MIDI bridge
 *
 * Copyright (c) 2011 Hans Petter Selasky
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <jack/driver.h>
#include <jack/internal.h>
#include <jack/engine.h>
#include <jack/thread.h>
#include <jack/ringbuffer.h>
#include <jack/midiport.h>

struct gmidi {
	JACK_DRIVER_DECL

	char   *j_read_name;
	char   *j_write_name;

	jack_client_t *j_client;
	jack_engine_t *j_engine;

	jack_port_t *j_read_port;
	jack_port_t *j_write_port;

	int	j_read_fd;
	int	j_write_fd;
};

static int
gmidi_attach(jack_driver_t *driver, jack_engine_t *engine)
{
	struct gmidi *pgm = (struct gmidi *)driver;
	char devname[128];

	engine->set_buffer_size(engine, 32);
	engine->set_sample_rate(engine, 1000);

	if (pgm->j_read_name != NULL) {
		snprintf(devname, sizeof(devname), "%s.rd", pgm->j_read_name);
		pgm->j_read_port = jack_port_register(pgm->j_client, devname,
		    JACK_DEFAULT_MIDI_TYPE, JackPortIsInput |
		    JackPortIsPhysical | JackPortIsTerminal, 0);
	}
	if (pgm->j_write_name != NULL) {
		snprintf(devname, sizeof(devname), "%s.wr", pgm->j_write_name);
		pgm->j_write_port = jack_port_register(pgm->j_client, devname,
		    JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput |
		    JackPortIsPhysical | JackPortIsTerminal, 0);
	}

	pgm->j_engine = engine;

	return (jack_activate(pgm->j_client));
}

static int
gmidi_detach(jack_driver_t *driver, jack_engine_t *engine)
{
	struct gmidi *pgm = (struct gmidi *)driver;

	pgm->j_engine = NULL;

	if (pgm->j_read_port != NULL) {
		jack_port_unregister(pgm->j_client, pgm->j_read_port);
		pgm->j_read_port = NULL;
	}
	if (pgm->j_write_port != NULL) {
		jack_port_unregister(pgm->j_client, pgm->j_write_port);
		pgm->j_write_port = NULL;
	}
	return (0);
}

static int
gmidi_start(jack_driver_t *driver)
{
	struct gmidi *pgm = (struct gmidi *)driver;

	if (pgm->j_read_name != NULL) {
		if (pgm->j_read_fd < 0)
			pgm->j_read_fd = open(pgm->j_read_name, O_RDONLY | O_NONBLOCK);
	} else {
		pgm->j_read_fd = -1;
	}
	if (pgm->j_write_name != NULL) {
		if (pgm->j_write_fd < 0)
			pgm->j_write_fd = open(pgm->j_write_name, O_WRONLY);
	} else {
		pgm->j_write_fd = -1;
	}
	return (0);
}

static int
gmidi_stop(jack_driver_t *driver)
{
	struct gmidi *pgm = (struct gmidi *)driver;

	if (pgm->j_read_fd > -1) {
		close(pgm->j_read_fd);
		pgm->j_read_fd = -1;
	}
	if (pgm->j_write_fd > -1) {
		close(pgm->j_write_fd);
		pgm->j_write_fd = -1;
	}
	return (0);
}

static int
gmidi_bufsize(jack_driver_t *driver, jack_nframes_t nframes)
{
	return (0);
}

static int
gmidi_null_cycle(jack_driver_t *driver, jack_nframes_t nframes)
{
	return (0);
}

static int
gmidi_read(jack_driver_t *driver, jack_nframes_t nframes)
{
	struct gmidi *pgm = (struct gmidi *)driver;
	void *buf;
	uint32_t data[1];
	int error;

	if (pgm->j_read_fd < 0)
		return (-1);

	error = read(pgm->j_read_fd, data, sizeof(data));
	if (error < 0) {
		if (errno != EWOULDBLOCK)
			exit(0);		/* the device is gone */
		return (0);
	}

	buf = jack_port_get_buffer(pgm->j_read_port, nframes);

	jack_midi_event_write(buf, nframes, (unsigned char *)data, error);
	return (0);
}

static int
gmidi_write(jack_driver_t *driver, jack_nframes_t nframes)
{
	struct gmidi *pgm = (struct gmidi *)driver;
	jack_midi_event_t event;
	void *buf;
	int i;
	int error;

	if (pgm->j_write_fd < 0)
		return (-1);

	buf = jack_port_get_buffer(pgm->j_write_port, nframes);

	for (i = 0; i < nframes; i++) {
		jack_midi_event_get(&event, buf, i);

		error = write(pgm->j_write_fd, event.buffer, event.size);
		if (error < 0)
			exit(0);	/* the device is gone */
	}
	return (0);
}

/* DRIVER "PLUGIN" INTERFACE */

const char driver_client_name[] = "gmidi";

const jack_driver_desc_t *
driver_get_descriptor()
{
	jack_driver_desc_t *desc;
	jack_driver_param_desc_t *params;
	unsigned int i;

	desc = calloc(1, sizeof(jack_driver_desc_t));

	strcpy(desc->name, "gmidi");
	desc->nparams = 3;

	params = calloc(desc->nparams, sizeof(jack_driver_param_desc_t));

	i = 0;
	strcpy(params[i].name, "capture");
	params[i].character = 'C';
	params[i].type = JackDriverParamString;
	strcpy(params[i].value.str, "/dev/umidi0.0");
	strcpy(params[i].short_desc,
	    "Provide capture device.");
	strcpy(params[i].long_desc, params[i].short_desc);

	i++;
	strcpy(params[i].name, "playback");
	params[i].character = 'P';
	params[i].type = JackDriverParamString;
	strcpy(params[i].value.str, "/dev/umidi0.0");
	strcpy(params[i].short_desc,
	    "Provide playback device.");
	strcpy(params[i].long_desc, params[i].short_desc);

	i++;
	strcpy(params[i].name, "device");
	params[i].character = 'd';
	params[i].type = JackDriverParamString;
	strcpy(params[i].value.str, "/dev/umidi0.0");
	strcpy(params[i].short_desc,
	    "Provide capture and playback device.");
	strcpy(params[i].long_desc, params[i].short_desc);

	desc->params = params;

	return (desc);
}

jack_driver_t *
driver_initialize(jack_client_t *client, const JSList * params)
{
	char *midi_rec_dev = NULL;
	char *midi_play_dev = NULL;
	struct gmidi *pgm;
	const JSList *node;
	const jack_driver_param_t *param;

	for (node = params; node; node = jack_slist_next(node)) {
		param = (const jack_driver_param_t *)node->data;

		switch (param->character) {
		case 'C':
			free(midi_rec_dev);
			midi_rec_dev = strdup(param->value.str);
			break;
		case 'P':
			free(midi_play_dev);
			midi_play_dev = strdup(param->value.str);
			break;
		case 'd':
			free(midi_rec_dev);
			free(midi_play_dev);
			midi_rec_dev = strdup(param->value.str);
			midi_play_dev = strdup(param->value.str);
			break;
		default:
			break;
		}
	}

	if (midi_rec_dev == NULL && midi_play_dev == NULL)
		return (NULL);

	pgm = calloc(1, sizeof(struct gmidi));
	if (pgm == NULL) {
		free(midi_rec_dev);
		free(midi_play_dev);
		return (NULL);
	}

	jack_driver_init((jack_driver_t *)pgm);

	pgm->j_read_fd = -1;
	pgm->j_write_fd = -1;
	pgm->j_read_name = midi_rec_dev;
	pgm->j_write_name = midi_play_dev;
	pgm->j_client = client;
	pgm->attach = &gmidi_attach;
	pgm->detach = &gmidi_detach;
	pgm->start = &gmidi_start;
	pgm->start = &gmidi_stop;
	pgm->bufsize = &gmidi_bufsize;
	pgm->null_cycle = &gmidi_null_cycle;
	pgm->read = &gmidi_read;
	pgm->write = &gmidi_write;

	return ((jack_driver_t *)pgm);
}

void
driver_finish(jack_driver_t *driver)
{
	struct gmidi *pgm = (struct gmidi *)driver;

	gmidi_detach(driver, NULL);

	gmidi_stop(driver);

	free(pgm->j_read_name);
	free(pgm->j_write_name);
	free(driver);
}
