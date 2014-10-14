/*  =========================================================================
    zeps_server - zeps server

    Copyright (c) the Contributors as noted in the AUTHORS file.
    This file is part of zeps, the ZeroMQ Enterprise Publish-Subscribe project.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

/*
@header
    Sample implementation of the zeps_server specification.
@discuss
    Limitations / assumptions
     - a client is connected to exactly one stream
     -
@end
*/

#include "czmq.h"
//  TODO: Change these to match your project's needs
#include "./zeps_msg.h"
#include "./zeps_server.h"


//  ---------------------------------------------------------------------------
//  Forward declarations for the two main classes we use here

typedef struct _server_t server_t;
typedef struct _client_t client_t;


//  This structure defines the context for each running server. Store
//  whatever properties and structures you need for the server.

struct _server_t {
    //  These properties must always be present in the server_t
    //  and are set by the generated engine; do not modify them!
    zsock_t *pipe;              //  Actor pipe back to caller
    zconfig_t *config;          //  Current loaded configuration

    bool verbose;               //  Set from config
    char *base_dir;             //  Base directory for streams
    zhash_t *streams;           //  Collection of streams
    char *pub_endpoint;         //  The endpoint to bind publish socket to
    zsock_t *pub_socket;        //  The publish socket
    zsock_t *sub_socket;        //  The subscribe socket
    zactor_t *pub_beacon;       //  beacon publishing pub endpoint
    zactor_t *sub_beacon;       //  beacon receiving pub endpoints
};


//  ---------------------------------------------------------------------------
//  This structure defines the state for each client connection. It will
//  be passed to each action in the 'self' argument.

struct _client_t {
    //  These properties must always be present in the client_t
    //  and are set by the generated engine; do not modify them!
    server_t *server;           //  Reference to parent server
    zeps_msg_t *request;        //  Last received request
    zeps_msg_t *reply;          //  Reply to send out, if any

    char *stream_name;          //  The name of the stream attached to
    size_t credit;              //  The number of bytes of active credit
    uint64_t last_seq;          //  The last sequence number delivered
    zhash_t *subscriptions;     //  Active subscription patterns (TODO: more adv matching)
};


//  Include the generated server engine
#include "zeps_server_engine.inc"


//  ---------------------------------------------------------------------------
//  base64 with URL & filename friendly alphabet (RFC4648, no padding)
//  Using this to generate safe filenames for stream data files
//
//                               1    1    2    2    3    3    4    4    5    5    6
static char  //        0----5----0----5----0----5----0----5----0----5----0----5----0---
s_base64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static char *
s_base64_encode (byte *data, int length)
{
    char *str = zmalloc (4 * (length / 3) + ((length % 3)? length % 3 + 1: 0) + 1);
    char *enc = str;
    byte *needle = data, *ceiling = data + length;
    while (needle < ceiling) {
        *enc++ = s_base64_alphabet[(*needle) >> 2];
        if (needle + 1 < ceiling) {
            *enc++ = s_base64_alphabet[((*needle << 4) & 0x30) | (*(needle + 1) >> 4)];
            if (needle + 2 < ceiling) {
                *enc++ = s_base64_alphabet[((*(needle + 1) << 2) & 0x3c) | (*(needle + 2) >> 6)];
                *enc++ = s_base64_alphabet[*(needle + 2) & 0x3f];
            }
            else
                *enc++ = s_base64_alphabet[(*(needle + 1) << 2) & 0x3c];
        }
        else
            *enc++ = s_base64_alphabet[(*needle << 4) & 0x30];
        needle += 3;
    }
    *enc = 0;
    return str;
}

static byte *
s_base64_decode (const char *data, int length, size_t *size)
{
    *size = 3 * (length / 4) + ((length % 4)? length % 4 - 1 : 0) + 1;
    byte *bytes = zmalloc (*size);
    byte *dec = bytes;
    byte *needle = (byte *) data, *ceiling = (byte *) (data + length);
    byte i1, i2, i3, i4;
    while (needle < ceiling) {
        i1 = (byte) (strchr (s_base64_alphabet, *needle) - s_base64_alphabet);
        i2 = (byte) (strchr (s_base64_alphabet, *(needle + 1)) - s_base64_alphabet);
        *dec++ = i1 << 2 | i2 >> 4;
        if (needle + 2 < ceiling) {
            i3 = (byte) (strchr (s_base64_alphabet, *(needle + 2)) - s_base64_alphabet);
            *dec++ = i2 << 4 | i3 >> 2;
        }
        if (needle + 3 < ceiling) {
            i4 = (byte) (strchr (s_base64_alphabet, *(needle + 3)) - s_base64_alphabet);
            *dec++ = i3 << 6 | i4;
        }
        needle += 4;
    }
    *dec = 0;
    return bytes;
}


//  ---------------------------------------------------------------------------
//  This structure defines the state for each active stream. A stream can
//  be created by a client ATTACHing or by receiving data on the sub socket.
//  A client removes itself from the stream's client list when the client
//  terminates.

typedef struct {
    char *name;
    char *dir_name;
    char *journal_dir;
    uint64_t seq;
    zlist_t *clients;
} stream_t;


//  ---------------------------------------------------------------------------
//  create stream

static stream_t *
s_stream_new (server_t *server, const char *name)
{
    assert (server);
    assert (name);

    stream_t *self = (stream_t *) zmalloc (sizeof (stream_t));
    assert (self);

    //  Init stream object properties
    self->name = strdup (name);
    self->dir_name = s_base64_encode ((byte *) name, strlen (name));
    self->journal_dir = zsys_sprintf ("%s/%s", server->base_dir, self->dir_name);
    self->seq = 0;
    self->clients = zlist_new ();

    //  Create stream dir if it doesn't exist
    if (!zsys_file_exists (self->journal_dir)) {
        int rc = zsys_dir_create (self->journal_dir);
        assert (rc == 0);
    }

    if (server->verbose)
        zsys_debug ("Stream created: %s", self->journal_dir);

    return self;
}


//  ---------------------------------------------------------------------------
//  stream destructor function

static void
s_stream_destroy (void **stream)
{
    assert (stream);

    if (*stream)
    {
        stream_t *s = (stream_t *) *stream;

        free (s->name);
        free (s->dir_name);
        free (s->journal_dir);
        zlist_destroy (&s->clients);
        free (s);

        *stream = NULL;
    }
}


//  ---------------------------------------------------------------------------
//  ensure stream exists

static stream_t *
s_ensure_stream (server_t *self, const char *stream_name)
{
    assert (self);
    assert (stream_name);

    //  Lookup stream
    stream_t *stream = (stream_t *) zhash_lookup (self->streams, stream_name);

    if (!stream)
    {
        //  Create stream object
        stream = s_stream_new (self, stream_name);
        assert (stream);

        zhash_insert (self->streams, stream_name, stream);
    }

    return stream;
}


//  --------------------------------------------------------------------------
//  save a message to file

static void
s_save_message (const char *dir, uint64_t seq, zmsg_t *msg)
{
    assert (dir);
    assert (seq > 0);
    assert (msg);

    byte *buffer;
    size_t size = zmsg_encode (msg, &buffer);

    if (*buffer && size > 0) {
        char *filename = zsys_sprintf ("%s/%ld", dir, seq);
        assert (filename);

        FILE *fp = fopen (filename, "wb");
        if (fp) {
            fwrite (buffer, 1, size, fp);
            fclose (fp);
        }
        else
            zsys_warning ("Unable to save message to %s", filename);

        free (buffer);
        free (filename);
    }
    else
        zsys_warning ("Unable to encode message %ld to buffer.", seq);
}


//  --------------------------------------------------------------------------
//  load a message from file

static zmsg_t *
s_load_message (const char *dir, uint64_t seq)
{
    assert (dir);
    assert (seq > 0);

    zmsg_t *msg = NULL;
    char *filename = zsys_sprintf ("%s/%ld", dir, seq);
    assert (filename);

    if (zsys_file_exists (filename)) {
        size_t size = zsys_file_size (filename);
        byte *buffer = (byte *) zmalloc (size);
        FILE *fp = fopen (filename, "rb");
        if (fp) {
            fread (buffer, 1, size, fp);
            fclose (fp);

            msg = zmsg_decode (buffer, size);
            if (!msg)
                zsys_warning ("Unable to decode message %ld", seq);
        }
        else
            zsys_warning ("Unable to load message from %s", filename);

        free (buffer);
    }
    else
        zsys_warning ("File %s does not exist.", filename);

    free (filename);
    return msg;
}


//  --------------------------------------------------------------------------
//  deliver messages on stream to connected clients

static void
s_deliver_messages (server_t * self, const char *stream_name)
{
    assert (self);
    assert (stream_name);

    stream_t *stream = (stream_t *) zhash_lookup (self->streams, stream_name);
    if (stream) {
        zhash_t *messages = zhash_new ();    //  local cache
        zhash_set_destructor (messages, (czmq_destructor *) zmsg_destroy);

        client_t *client = (client_t *) zlist_first (stream->clients);
        while (client) {
            bool credit_exhausted = false;
            while (!credit_exhausted && client->last_seq < stream->seq) {
                uint64_t seq = client->last_seq + 1;    //  current sequence number

                //  lookup cache, load and update cache if not found
                char *key = zsys_sprintf ("%ld", seq);
                zmsg_t *msg = (zmsg_t *) zhash_lookup (messages, key);
                if (!msg) {
                    msg = s_load_message (stream->journal_dir, seq);
                    if (msg) {
                        zhash_insert (messages, key, msg);

                        if (self->verbose) {
                            zsys_debug ("s_deliver_messages: loaded message %s/%d", stream->journal_dir, seq);
                            zmsg_print (msg);
                        }
                    }
                }
                free (key);

                if (msg) {
                    char *pattern = zframe_strdup (zmsg_first (msg));
                    assert (pattern);
                    zframe_t *body_frame = zmsg_next (msg);
                    assert (body_frame);
                    zchunk_t *body = zchunk_new (zframe_data (body_frame), zframe_size (body_frame));
                    assert (body);

                    if (zhash_lookup (client->subscriptions, pattern)) {    //  TODO: More advanced pattern matching
                        size_t size = 8 + strlen (pattern) + 1 + zchunk_size (body);

                        if (size <= client->credit) {
                            zeps_msg_set_sequence (client->reply, seq);
                            zeps_msg_set_key (client->reply, pattern);
                            zeps_msg_set_body (client->reply, &body);
                            engine_send_event (client, data_recvd_event);
                            client->credit -= size;
                            client->last_seq = seq;

                            if (self->verbose)
                                zsys_debug ("Delivering on stream %s to client sequence %ld on topic %s",
                                            stream_name, seq, pattern);
                        }
                        else
                            credit_exhausted = true;    //  client has insufficient credit available
                    }
                    else
                        client->last_seq = seq; //  client not subscribed; mark as delivered

                    zchunk_destroy (&body);
                    free (pattern);
                }
            }

            client = (client_t *) zlist_next (stream->clients);
        }

        zhash_destroy (&messages);
    }
}


//  --------------------------------------------------------------------------
//  save incoming message to journal, then deliver to clients

static void
s_journal_incoming (server_t *self, zmsg_t *msg)
{
    assert (self);
    assert (msg);

    //  Get stream name
    char *stream_name = zmsg_popstr (msg);
    assert (stream_name);

    //  Get stream object
    stream_t *stream = s_ensure_stream (self, stream_name);
    assert (stream);

    //  Increment and get sequence number
    uint64_t seq = ++stream->seq;

    //  Save message to file store
    s_save_message (stream->journal_dir, seq, msg);

    //  DELIVER to subscribed clients
    s_deliver_messages (self, stream->name);
}


//  --------------------------------------------------------------------------
//  save outbound message from a client to file store, publish, then deliver to clients.

static void
s_journal_outbound (client_t *self, zeps_msg_t *msg)
{
    assert (self);
    assert (msg);

    //  Get stream name
    const char *stream_name = self->stream_name;
    assert (stream_name);

    //  Get stream object
    stream_t *stream = s_ensure_stream (self->server, stream_name);
    assert (stream);

    //  Get and increment sequence number
    uint64_t seq = ++stream->seq;

    //  Create outbound message
    zmsg_t *pub_msg = zmsg_new ();
    assert (pub_msg);

    //  Add pattern frame
    zmsg_addstr (pub_msg, zeps_msg_key (msg));

    //  Add body frame
    zchunk_t *body = zeps_msg_body (msg);
    assert (body);
    zframe_t *body_frame = zframe_new (zchunk_data (body), zchunk_size (body));
    assert (body_frame);
    zmsg_append (pub_msg, &body_frame);

    //  Save message to file store
    s_save_message (stream->journal_dir, seq, pub_msg);

    //  Prepend stream name
    zmsg_pushstr (pub_msg, stream_name);

    //  Publish
    zmsg_send (&pub_msg, self->server->pub_socket);

    //  DELIVER to subscribed clients
    s_deliver_messages (self->server, stream_name);
}


//  --------------------------------------------------------------------------
//  Sub socket handler

static int
s_handle_sub_socket (zloop_t *loop, zsock_t *reader, void *args)
{
    server_t *self = (server_t *) args;

    zmsg_t *msg = zmsg_recv (reader);
    s_journal_incoming (self, msg);
    zmsg_destroy (&msg);

    return 0;
}


//  Allocate properties and structures for a new server instance.
//  Return 0 if OK, or -1 if there was an error.

static int
server_initialize (server_t *self)
{
    self->verbose = (bool) atoi (zconfig_resolve (self->config, "server/verbose", "1"));
    self->base_dir = strdup (zconfig_resolve (self->config, "server/base_dir", "/tmp"));
    self->streams = zhash_new ();
    zhash_set_destructor (self->streams, s_stream_destroy);
    self->pub_beacon = NULL;
    self->sub_beacon = NULL;
    self->pub_endpoint = NULL;
    self->pub_socket = zsock_new (ZMQ_PUB);
    self->sub_socket = zsock_new (ZMQ_SUB);
    zsock_set_subscribe (self->sub_socket, "");  //  Subscribe to all
    engine_handle_socket (self, self->sub_socket, s_handle_sub_socket);

    return 0;
}

//  Free properties and structures for a server instance

static void
server_terminate (server_t *self)
{
    //  Destroy properties here
    free (self->base_dir);
    self->base_dir = NULL;
    free (self->pub_endpoint);
    self->pub_endpoint = NULL;
    zhash_destroy (&self->streams);
    zsock_destroy (&self->pub_socket);
    zsock_destroy (&self->sub_socket);
    zactor_destroy (&self->pub_beacon);
    zactor_destroy (&self->sub_beacon);
}


//  --------------------------------------------------------------------------
//  Sub beacon handler; connect to a newly discovered agent

static int
s_handle_sub_beacon (zloop_t *loop, zsock_t *reader, void *args)
{
    server_t *self = (server_t *) args;
    assert (self);

    char *address, *endpoint;
    size_t len;
    int rc = zsock_recv (reader, "sb", &address, &endpoint, &len);
    assert (rc == 0);

    if (address && endpoint) {
        rc = zsock_connect(self->sub_socket, endpoint, NULL);
        if (rc != 0)
            zsys_warning ("Unable to connect to %s", endpoint);
        free (address);
        free (endpoint);
    }

    return 0;
}


//  Process server API method, return reply message if any

static zmsg_t *
server_method (server_t *self, const char *method, zmsg_t *msg)
{
    if (streq (method, "PUB")) {
        char *command = zmsg_popstr (msg);
        if (streq (command, "BIND")) {
            self->pub_endpoint = zmsg_popstr (msg);
            int rc = zsock_bind (self->pub_socket, self->pub_endpoint, NULL);
            if (rc == -1)
                zsys_warning ("%s %s %s failed.", method, command, self->pub_endpoint);
        }
        else
        if (streq (command, "BEACON")) {
            if (self->pub_endpoint) {
                int port = atoi (zmsg_popstr (msg));
                int interval = atoi (zmsg_popstr (msg));
                self->pub_beacon = zactor_new (zbeacon, NULL);
                int rc = zsock_send (self->pub_beacon, "si", "CONFIGURE", port);
                if (rc == -1)
                    zsys_warning ("%s %s %i %i configure failed.", method, command, port, interval);
                rc = zsock_send (self->pub_beacon, "sbi", "PUBLISH",
                                 self->pub_endpoint, strlen (self->pub_endpoint) + 1, interval);
                if (rc == -1)
                    zsys_warning ("%s %s %i %i publish failed.", method, command, port, interval);
            }
            else {
                zsys_warning ("%s %s invalid until BIND.", method, command);
            }
        }
        else {
            zsys_warning ("Unknown command: %s %s", method, command);
        }
    }
    else
    if (streq (method, "SUB")) {
        char *command = zmsg_popstr (msg);
        if (streq (command, "CONNECT")) {
            char *endpoint = zmsg_popstr (msg);
            int rc = zsock_connect (self->sub_socket, endpoint, NULL);
            if (rc == -1)
                zsys_warning ("%s %s %s failed.", method, command, endpoint);
        }
        else
        if (streq (command, "BEACON")) {
            int port = atoi (zmsg_popstr (msg));
            self->sub_beacon = zactor_new (zbeacon, NULL);
            int rc = zsock_send (self->pub_beacon, "si", "CONFIGURE", port);
            if (rc == -1)
                zsys_warning ("%s %s %i %i configure failed.", method, command, port);
            rc = zsock_send (self->pub_beacon, "sb", "SUBSCRIBE", NULL, 0);
            if (rc == -1)
                zsys_warning ("%s %s %i %i publish failed.", method, command, port);
            engine_handle_socket (self, zactor_resolve (self->sub_beacon), s_handle_sub_beacon);
        }
        else {
            zsys_warning ("Unknown command: %s %s", method, command);
        }
    }
    else {
        zsys_warning ("Unhandled method: %s", method);
    }

    return NULL;
}


//  Allocate properties and structures for a new client connection and
//  optionally engine_set_next_event (). Return 0 if OK, or -1 on error.

static int
client_initialize (client_t *self)
{
    //  Construct properties here
    self->stream_name = NULL;
    self->credit = 0;
    self->subscriptions = zhash_new ();

    return 0;
}


//  Free properties and structures for a client connection

static void
client_terminate (client_t *self)
{
    //  remove self from streams
    stream_t *stream = zhash_first (self->server->streams);
    while (stream) {
        zlist_remove (stream->clients, self);
        stream = zhash_next (self->server->streams);
    }

    //  destroy properties
    free (self->stream_name);
    self->stream_name = NULL;
    zhash_destroy (&self->subscriptions);
}


//  ---------------------------------------------------------------------------
//  base64 test utility

static void
s_base64_test (const char *test_string, const char *expected_result, bool verbose)
{
    assert (test_string);

    if (verbose)
        zsys_debug (" * base64 encoding '%s'", test_string);

    char *encoded = s_base64_encode ((byte *) test_string, strlen (test_string));
    assert (encoded);
    assert (strlen (encoded) == strlen (expected_result));
    assert (streq (encoded, expected_result));

    if (verbose)
        zsys_debug (" * base64 encoded '%s' into '%s'", test_string, encoded);

    size_t size;
    char *decoded = (char *) s_base64_decode (encoded, strlen (encoded), &size);
    assert (decoded);
    assert (size == strlen (decoded) + 1);
    assert (streq (decoded, test_string));

    if (verbose)
        zsys_debug (" * base64 decoded '%s' back into '%s'", encoded, decoded);

    free (encoded);
    free (decoded);
}


//  ---------------------------------------------------------------------------
//  Selftest

void
zeps_server_test (bool verbose)
{
    printf (" * zeps_server: \n");
    if (verbose)
        printf ("\n");

    //  Unit test of base 64 encoding/decoding
    //  Test against test vectors from RFC4648.
    s_base64_test ("", "", verbose);
    s_base64_test ("f", "Zg", verbose);
    s_base64_test ("fo", "Zm8", verbose);
    s_base64_test ("foo", "Zm9v", verbose);
    s_base64_test ("foob", "Zm9vYg", verbose);
    s_base64_test ("fooba", "Zm9vYmE", verbose);
    s_base64_test ("foobar", "Zm9vYmFy", verbose);

    //  @selftest
    zactor_t *server = zactor_new (zeps_server, "server");
    if (verbose)
        zstr_send (server, "VERBOSE");
    zstr_sendx (server, "BIND", "ipc://@/zeps_server", NULL);

    zsock_t *client = zsock_new (ZMQ_DEALER);
    assert (client);
    zsock_set_rcvtimeo (client, 2000);
    zsock_connect (client, "ipc://@/zeps_server");

    zeps_msg_t *request, *reply;

    //  PING test
    request = zeps_msg_new (ZEPS_MSG_PING);
    zeps_msg_send (&request, client);
    reply = zeps_msg_recv (client);
    assert (reply);
    assert (zeps_msg_id (reply) == ZEPS_MSG_PING_OK);
    zeps_msg_destroy (&reply);

    //  ATTACH test
    request = zeps_msg_new (ZEPS_MSG_ATTACH);
    zeps_msg_set_stream (request, "test_stream");
    zeps_msg_send (&request, client);
    reply = zeps_msg_recv (client);
    assert (reply);
    assert (zeps_msg_id (reply) == ZEPS_MSG_ATTACH_OK);
    zeps_msg_destroy (&reply);

    //  CREDIT test
    request = zeps_msg_new (ZEPS_MSG_CREDIT);
    zeps_msg_set_credit (request, 1024);
    zeps_msg_send (&request, client);

    //  SUBSCRIBE test
    request = zeps_msg_new (ZEPS_MSG_SUBSCRIBE);
    zeps_msg_set_pattern (request, "test_pattern");
    zeps_msg_set_latest (request, 0);
    zeps_msg_send (&request, client);
    reply = zeps_msg_recv (client);
    assert (reply);
    assert (zeps_msg_id (reply) == ZEPS_MSG_SUBSCRIBE_OK);
    zeps_msg_destroy (&reply);

    //  PUBLISH test
    request = zeps_msg_new (ZEPS_MSG_PUBLISH);
    zeps_msg_set_key (request, "test_pattern");
    byte data[] = {0, 1, 2, 3, 4, 5};
    zchunk_t *body = zchunk_new (data, sizeof (data));
    zeps_msg_set_body (request, &body);
    zeps_msg_send (&request, client);

    //  DELIVER test
    reply = zeps_msg_recv (client);
    assert (reply);
    assert (zeps_msg_id (reply) == ZEPS_MSG_DELIVER);
    assert (streq (zeps_msg_key (reply), "test_pattern"));
    uint64_t seq = zeps_msg_sequence (reply);
    body = zeps_msg_body (reply);
    assert (body);
    assert (zchunk_size (body) == 6);
    if (verbose) {
        zsys_debug ("DELIVERed message: sequence %ld", seq);
        zchunk_print (body);
    }
    zeps_msg_destroy (&reply);

    //  DETACH test
    request = zeps_msg_new (ZEPS_MSG_DETACH);
    zeps_msg_send (&request, client);
    reply = zeps_msg_recv (client);
    assert (reply);
    assert (zeps_msg_id (reply) == ZEPS_MSG_DETACH_OK);
    zeps_msg_destroy (&reply);

    //  send CREDIT when detached; should receive INVALID reply
    request = zeps_msg_new (ZEPS_MSG_CREDIT);
    zeps_msg_set_credit (request, 1024);
    zeps_msg_send (&request, client);
    reply = zeps_msg_recv (client);
    assert (reply);
    assert (zeps_msg_id (reply) == ZEPS_MSG_INVALID);
    zeps_msg_destroy (&reply);

    zsock_destroy (&client);
    zactor_destroy (&server);
    //  @end
    printf ("OK\n");
}


//  ---------------------------------------------------------------------------
//  setup_stream
//

static void
setup_stream (client_t *self)
{
    assert (self);
    assert (zeps_msg_id (self->request) == ZEPS_MSG_ATTACH);

    const char *stream_name = zeps_msg_stream (self->request);
    assert (stream_name);

    if (self->server->verbose)
        zsys_debug ("client attaching to stream %s", stream_name);

    stream_t *stream = s_ensure_stream (self->server, stream_name);
    assert (stream);

    zlist_push (stream->clients, self);
    self->stream_name = strdup (stream_name);
}


//  ---------------------------------------------------------------------------
//  setup_subscription
//

static void
setup_subscription (client_t *self)
{
    assert (self);
    assert (zeps_msg_id (self->request) == ZEPS_MSG_SUBSCRIBE);

    const char *pattern = zeps_msg_pattern (self->request);
    assert (pattern);

    self->last_seq = zeps_msg_latest (self->request);

    if (self->server->verbose)
        zsys_debug ("Client requesting subscription to %s (latest %ld)", pattern, self->last_seq);

    zhash_update (self->subscriptions, pattern, (void *) 1);
    s_deliver_messages (self->server, self->stream_name);
}


//  ---------------------------------------------------------------------------
//  accept_credit
//

static void
accept_credit (client_t *self)
{
    assert (self);
    assert (zeps_msg_id (self->request) == ZEPS_MSG_CREDIT);

    self->credit += zeps_msg_credit (self->request);

    if (self->server->verbose)
        zsys_debug ("Client sent %d credits, total now %d", zeps_msg_credit (self->request), self->credit);
}


//  ---------------------------------------------------------------------------
//  publish_message
//

static void
publish_message (client_t *self)
{
    assert (self);
    assert (zeps_msg_id (self->request) == ZEPS_MSG_PUBLISH);

    if (self->server->verbose)
        zsys_debug ("Client requests to publish %d bytes with key %s",
                    zchunk_size (zeps_msg_body (self->request)),
                    zeps_msg_key (self->request));

    s_journal_outbound (self, self->request);
}
