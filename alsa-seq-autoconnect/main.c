/*
rpi-midi-ble: Raspberry Pi 3 as a USB-MIDI over BLE-MIDI device
Copyright (C) 2017 Daniel Moura <oxe@oxesoft.com>

This code is originally hosted at https://github.com/oxesoft/rpi-midi-ble

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <memory.h>
#include <alsa/asoundlib.h>

#define MAX_SENDERS 16
#define APP_NAME "alsa-seq-autoconnect"
static char *dest_name = "BLE-MIDI Device";
static bool stop       =  false;
typedef struct snd_seq_connect_ports {
    bool           senders_connected[MAX_SENDERS];
    bool           senders_found    [MAX_SENDERS];
    snd_seq_addr_t senders          [MAX_SENDERS];
    bool           dest_found;
    snd_seq_addr_t dest;
} snd_seq_connect_ports_t;
static snd_seq_connect_ports_t ports;

static void sighandler(int sig)
{
    stop = true;
}

static void connect_ports(snd_seq_t *seq, int index)
{
    snd_seq_port_subscribe_t *subs;

    if (index >= MAX_SENDERS)
    {
        return;
    }

    if (ports.senders_connected[index] == true)
    {
        return;
    }

    printf("Connecting %d,%d to %d,%d\n",
        ports.senders[index].client,
        ports.senders[index].port,
        ports.dest.client,
        ports.dest.port);

    snd_seq_port_subscribe_alloca(&subs);
    snd_seq_port_subscribe_set_sender(subs, &ports.senders[index]);
    snd_seq_port_subscribe_set_dest(subs, &ports.dest);
    snd_seq_subscribe_port(seq, subs);

    ports.senders_connected[index] == true;
}

static bool verify_port(snd_seq_t *seq, int client, int port)
{
    snd_seq_port_info_t *pinfo;
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_get_any_port_info(seq, client, port, pinfo);
    const char* pname = snd_seq_port_info_get_name(pinfo);
    unsigned int caps = snd_seq_port_info_get_capability(pinfo);
    unsigned int type = snd_seq_port_info_get_type(pinfo);
    int i;

    if (strcmp(pname, dest_name) == 0)
    {
        printf("Found destination port \"%s\"\n", pname);
        ports.dest.client = client;
        ports.dest.port   = port;
        ports.dest_found  = true;
    }
    else if (caps & SND_SEQ_PORT_CAP_READ      &&
             caps & SND_SEQ_PORT_CAP_SUBS_READ &&
             type & SND_SEQ_PORT_TYPE_HARDWARE)
    {
        bool already_used = false;
        for (i = 0; i < MAX_SENDERS; i++)
        {
            if (ports.senders[i].client == client &&
                ports.senders[i].port   == port   &&
                ports.senders_found[i]  == true)
            {
                already_used = true;
                break;
            }
        }

        if (already_used == false)
        {
            int available_index = -1;
            for (i = 0; i < MAX_SENDERS; i++)
            {
                if (ports.senders_found[i] == false)
                {
                    available_index = i;
                    break;
                }
            }
            if (available_index != -1)
            {
                printf("Found source port \"%s\"\n", pname);
                ports.senders[available_index].client    = client;
                ports.senders[available_index].port      = port;
                ports.senders_found[available_index]     = true;
                ports.senders_connected[available_index] = false;
            }
        }
    }

    if (ports.dest_found == false)
    {
        return false;
    }

    for (i = 0; i < MAX_SENDERS; i++)
    {
        if (ports.senders_found[i] == true && ports.senders_connected[i] == false)
        {
            connect_ports(seq, i);
        }
    }

    return true;
}

static int list_ports(snd_seq_t *seq)
{
    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t *pinfo;
    int found = false;

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0 && found == false)
    {
        int client = snd_seq_client_info_get_client(cinfo);
        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(seq, pinfo) >= 0 && found == false)
        {
            int port = snd_seq_port_info_get_port(pinfo);
            found = verify_port(seq, client, port);
        }
    }
}

int main(int argc, char *argv[])
{
    int err;
    snd_seq_t *seq;
    struct pollfd *pfds;
    int npfds;
    int i;

    memset(&ports, 0, sizeof(ports));

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
    if (err < 0)
    {
        fprintf(stderr, "Error creating port\n");
        goto _err;
    }

    err = snd_seq_set_client_name(seq, APP_NAME);
    if (err < 0)
    {
        fprintf(stderr, "Error naming port: %s\n", snd_strerror(err));
        goto _err_seq_close;
    }

    err = snd_seq_create_simple_port(seq, APP_NAME,
                     SND_SEQ_PORT_CAP_WRITE |
                     SND_SEQ_PORT_CAP_SUBS_WRITE,
                     SND_SEQ_PORT_TYPE_APPLICATION);
    if (err < 0)
    {
        fprintf(stderr, "Error creating port: %s\n", snd_strerror(err));
        goto _err_seq_close;
    }

    err = snd_seq_nonblock(seq, 1);
    if (err < 0)
    {
        fprintf(stderr, "Error setting non block mode: %s\n", snd_strerror(err));
        goto _err_seq_close;
    }

    err = snd_seq_connect_from(seq, 0, 0, 1);
    if (err < 0)
    {
        fprintf(stderr, "Cannot connect from announce port: %s\n", snd_strerror(err));
        goto _err_seq_close;
    }

    list_ports(seq);

    /* listens to the events announcements */
    npfds = snd_seq_poll_descriptors_count(seq, POLLIN);
    pfds = alloca(sizeof(*pfds) * npfds);
    while (stop == false)
    {
        snd_seq_poll_descriptors(seq, pfds, npfds, POLLIN);
        err = poll(pfds, npfds, -1);
        if (err < 0)
        {
            err = 0;
            break;
        }
        do
        {
            snd_seq_event_t *event;
            err = snd_seq_event_input(seq, &event);
            if (err < 0)
            {
                err = 0;
                break;
            }
            if (event)
            {
                switch (event->type)
                {
                    case SND_SEQ_EVENT_PORT_START:
                        verify_port(seq, event->data.addr.client, event->data.addr.port);
                        break;
                    case SND_SEQ_EVENT_PORT_EXIT:
                        if (event->data.addr.client == ports.dest.client &&
                            event->data.addr.port   == ports.dest.port   &&
                            ports.dest_found == true)
                        {
                            printf("Exited destination port\n");
                            ports.dest_found = false;
                            for (i = 0; i < MAX_SENDERS; i++)
                            {
                                ports.senders_connected[i] = false;
                            }
                        }
                        else
                        {
                            for (i = 0; i < MAX_SENDERS; i++)
                            {
                                if (event->data.addr.client == ports.senders[i].client &&
                                    event->data.addr.port   == ports.senders[i].port   &&
                                    ports.senders_found[i] == true)
                                {
                                    printf("Exited source port\n");
                                    ports.senders_found[i]     = false;
                                    ports.senders_connected[i] = false;
                                    break;
                                }
                            }
                        }
                        break;
                }
            }
        } while (err > 0);
        fflush(stdout);
    }

_err_seq_close:
    snd_seq_close(seq);
_err:
    return err;
}
