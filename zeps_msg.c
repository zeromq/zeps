/*  =========================================================================
    zeps_msg - ZeroMQ Enterprise Publish-Subscribe Protocol

    Codec class for zeps_msg.

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: zeps_msg.xml, or
     * The code generation script that built this file: zproto_codec_c.gsl
    ************************************************************************
    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of zbroker, the ZeroMQ broker project.           
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

/*
@header
    zeps_msg - ZeroMQ Enterprise Publish-Subscribe Protocol
@discuss
@end
*/

#include "czmq.h"
#include "./zeps_msg.h"

//  Structure of our class

struct _zeps_msg_t {
    zframe_t *routing_id;               //  Routing_id from ROUTER, if any
    int id;                             //  zeps_msg message ID
    byte *needle;                       //  Read/write pointer for serialization
    byte *ceiling;                      //  Valid upper limit for read pointer
    char *protocol;                     //  Constant "ZEPS"
    uint16_t version;                   //  Protocol version 1
    char *stream;                       //  Stream name
    char *pattern;                      //  Subscription pattern
    uint64_t latest;                    //  Last known sequence
    uint64_t credit;                    //  Credit, in bytes
    char *key;                          //  Message routing key
    zchunk_t *body;                     //  Message contents
    uint64_t sequence;                  //  Sequenced per stream
    char *reason;                       //  Printable explanation
};

//  --------------------------------------------------------------------------
//  Network data encoding macros

//  Put a block of octets to the frame
#define PUT_OCTETS(host,size) { \
    memcpy (self->needle, (host), size); \
    self->needle += size; \
}

//  Get a block of octets from the frame
#define GET_OCTETS(host,size) { \
    if (self->needle + size > self->ceiling) \
        goto malformed; \
    memcpy ((host), self->needle, size); \
    self->needle += size; \
}

//  Put a 1-byte number to the frame
#define PUT_NUMBER1(host) { \
    *(byte *) self->needle = (host); \
    self->needle++; \
}

//  Put a 2-byte number to the frame
#define PUT_NUMBER2(host) { \
    self->needle [0] = (byte) (((host) >> 8)  & 255); \
    self->needle [1] = (byte) (((host))       & 255); \
    self->needle += 2; \
}

//  Put a 4-byte number to the frame
#define PUT_NUMBER4(host) { \
    self->needle [0] = (byte) (((host) >> 24) & 255); \
    self->needle [1] = (byte) (((host) >> 16) & 255); \
    self->needle [2] = (byte) (((host) >> 8)  & 255); \
    self->needle [3] = (byte) (((host))       & 255); \
    self->needle += 4; \
}

//  Put a 8-byte number to the frame
#define PUT_NUMBER8(host) { \
    self->needle [0] = (byte) (((host) >> 56) & 255); \
    self->needle [1] = (byte) (((host) >> 48) & 255); \
    self->needle [2] = (byte) (((host) >> 40) & 255); \
    self->needle [3] = (byte) (((host) >> 32) & 255); \
    self->needle [4] = (byte) (((host) >> 24) & 255); \
    self->needle [5] = (byte) (((host) >> 16) & 255); \
    self->needle [6] = (byte) (((host) >> 8)  & 255); \
    self->needle [7] = (byte) (((host))       & 255); \
    self->needle += 8; \
}

//  Get a 1-byte number from the frame
#define GET_NUMBER1(host) { \
    if (self->needle + 1 > self->ceiling) \
        goto malformed; \
    (host) = *(byte *) self->needle; \
    self->needle++; \
}

//  Get a 2-byte number from the frame
#define GET_NUMBER2(host) { \
    if (self->needle + 2 > self->ceiling) \
        goto malformed; \
    (host) = ((uint16_t) (self->needle [0]) << 8) \
           +  (uint16_t) (self->needle [1]); \
    self->needle += 2; \
}

//  Get a 4-byte number from the frame
#define GET_NUMBER4(host) { \
    if (self->needle + 4 > self->ceiling) \
        goto malformed; \
    (host) = ((uint32_t) (self->needle [0]) << 24) \
           + ((uint32_t) (self->needle [1]) << 16) \
           + ((uint32_t) (self->needle [2]) << 8) \
           +  (uint32_t) (self->needle [3]); \
    self->needle += 4; \
}

//  Get a 8-byte number from the frame
#define GET_NUMBER8(host) { \
    if (self->needle + 8 > self->ceiling) \
        goto malformed; \
    (host) = ((uint64_t) (self->needle [0]) << 56) \
           + ((uint64_t) (self->needle [1]) << 48) \
           + ((uint64_t) (self->needle [2]) << 40) \
           + ((uint64_t) (self->needle [3]) << 32) \
           + ((uint64_t) (self->needle [4]) << 24) \
           + ((uint64_t) (self->needle [5]) << 16) \
           + ((uint64_t) (self->needle [6]) << 8) \
           +  (uint64_t) (self->needle [7]); \
    self->needle += 8; \
}

//  Put a string to the frame
#define PUT_STRING(host) { \
    size_t string_size = strlen (host); \
    PUT_NUMBER1 (string_size); \
    memcpy (self->needle, (host), string_size); \
    self->needle += string_size; \
}

//  Get a string from the frame
#define GET_STRING(host) { \
    size_t string_size; \
    GET_NUMBER1 (string_size); \
    if (self->needle + string_size > (self->ceiling)) \
        goto malformed; \
    (host) = (char *) malloc (string_size + 1); \
    memcpy ((host), self->needle, string_size); \
    (host) [string_size] = 0; \
    self->needle += string_size; \
}

//  Put a long string to the frame
#define PUT_LONGSTR(host) { \
    size_t string_size = strlen (host); \
    PUT_NUMBER4 (string_size); \
    memcpy (self->needle, (host), string_size); \
    self->needle += string_size; \
}

//  Get a long string from the frame
#define GET_LONGSTR(host) { \
    size_t string_size; \
    GET_NUMBER4 (string_size); \
    if (self->needle + string_size > (self->ceiling)) \
        goto malformed; \
    (host) = (char *) malloc (string_size + 1); \
    memcpy ((host), self->needle, string_size); \
    (host) [string_size] = 0; \
    self->needle += string_size; \
}


//  --------------------------------------------------------------------------
//  Create a new zeps_msg

zeps_msg_t *
zeps_msg_new (int id)
{
    zeps_msg_t *self = (zeps_msg_t *) zmalloc (sizeof (zeps_msg_t));
    self->id = id;
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zeps_msg

void
zeps_msg_destroy (zeps_msg_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zeps_msg_t *self = *self_p;

        //  Free class properties
        zframe_destroy (&self->routing_id);
        free (self->protocol);
        free (self->stream);
        free (self->pattern);
        free (self->key);
        zchunk_destroy (&self->body);
        free (self->reason);

        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Parse a zeps_msg from zmsg_t. Returns a new object, or NULL if
//  the message could not be parsed, or was NULL. Destroys msg and 
//  nullifies the msg reference.

zeps_msg_t *
zeps_msg_decode (zmsg_t **msg_p)
{
    assert (msg_p);
    zmsg_t *msg = *msg_p;
    if (msg == NULL)
        return NULL;
        
    zeps_msg_t *self = zeps_msg_new (0);
    //  Read and parse command in frame
    zframe_t *frame = zmsg_pop (msg);
    if (!frame) 
        goto empty;             //  Malformed or empty

    //  Get and check protocol signature
    self->needle = zframe_data (frame);
    self->ceiling = self->needle + zframe_size (frame);
    uint16_t signature;
    GET_NUMBER2 (signature);
    if (signature != (0xAAA0 | 5))
        goto empty;             //  Invalid signature

    //  Get message id and parse per message type
    GET_NUMBER1 (self->id);

    switch (self->id) {
        case ZEPS_MSG_ATTACH:
            GET_STRING (self->protocol);
            if (strneq (self->protocol, "ZEPS"))
                goto malformed;
            GET_NUMBER2 (self->version);
            if (self->version != 1)
                goto malformed;
            GET_STRING (self->stream);
            break;

        case ZEPS_MSG_ATTACH_OK:
            break;

        case ZEPS_MSG_SUBSCRIBE:
            GET_STRING (self->pattern);
            GET_NUMBER8 (self->latest);
            break;

        case ZEPS_MSG_SUBSCRIBE_OK:
            break;

        case ZEPS_MSG_CREDIT:
            GET_NUMBER8 (self->credit);
            break;

        case ZEPS_MSG_PUBLISH:
            GET_STRING (self->key);
            {
                size_t chunk_size;
                GET_NUMBER4 (chunk_size);
                if (self->needle + chunk_size > (self->ceiling))
                    goto malformed;
                self->body = zchunk_new (self->needle, chunk_size);
                self->needle += chunk_size;
            }
            break;

        case ZEPS_MSG_DELIVER:
            GET_NUMBER8 (self->sequence);
            GET_STRING (self->key);
            {
                size_t chunk_size;
                GET_NUMBER4 (chunk_size);
                if (self->needle + chunk_size > (self->ceiling))
                    goto malformed;
                self->body = zchunk_new (self->needle, chunk_size);
                self->needle += chunk_size;
            }
            break;

        case ZEPS_MSG_PING:
            break;

        case ZEPS_MSG_PING_OK:
            break;

        case ZEPS_MSG_DETACH:
            break;

        case ZEPS_MSG_DETACH_OK:
            break;

        case ZEPS_MSG_INVALID:
            GET_STRING (self->reason);
            break;

        default:
            goto malformed;
    }
    //  Successful return
    zframe_destroy (&frame);
    zmsg_destroy (msg_p);
    return self;

    //  Error returns
    malformed:
        zsys_error ("malformed message '%d'\n", self->id);
    empty:
        zframe_destroy (&frame);
        zmsg_destroy (msg_p);
        zeps_msg_destroy (&self);
        return (NULL);
}


//  --------------------------------------------------------------------------
//  Encode zeps_msg into zmsg and destroy it. Returns a newly created
//  object or NULL if error. Use when not in control of sending the message.

zmsg_t *
zeps_msg_encode (zeps_msg_t **self_p)
{
    assert (self_p);
    assert (*self_p);
    
    zeps_msg_t *self = *self_p;
    zmsg_t *msg = zmsg_new ();

    size_t frame_size = 2 + 1;          //  Signature and message ID
    switch (self->id) {
        case ZEPS_MSG_ATTACH:
            //  protocol is a string with 1-byte length
            frame_size += 1 + strlen ("ZEPS");
            //  version is a 2-byte integer
            frame_size += 2;
            //  stream is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->stream)
                frame_size += strlen (self->stream);
            break;
            
        case ZEPS_MSG_ATTACH_OK:
            break;
            
        case ZEPS_MSG_SUBSCRIBE:
            //  pattern is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->pattern)
                frame_size += strlen (self->pattern);
            //  latest is a 8-byte integer
            frame_size += 8;
            break;
            
        case ZEPS_MSG_SUBSCRIBE_OK:
            break;
            
        case ZEPS_MSG_CREDIT:
            //  credit is a 8-byte integer
            frame_size += 8;
            break;
            
        case ZEPS_MSG_PUBLISH:
            //  key is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->key)
                frame_size += strlen (self->key);
            //  body is a chunk with 4-byte length
            frame_size += 4;
            if (self->body)
                frame_size += zchunk_size (self->body);
            break;
            
        case ZEPS_MSG_DELIVER:
            //  sequence is a 8-byte integer
            frame_size += 8;
            //  key is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->key)
                frame_size += strlen (self->key);
            //  body is a chunk with 4-byte length
            frame_size += 4;
            if (self->body)
                frame_size += zchunk_size (self->body);
            break;
            
        case ZEPS_MSG_PING:
            break;
            
        case ZEPS_MSG_PING_OK:
            break;
            
        case ZEPS_MSG_DETACH:
            break;
            
        case ZEPS_MSG_DETACH_OK:
            break;
            
        case ZEPS_MSG_INVALID:
            //  reason is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->reason)
                frame_size += strlen (self->reason);
            break;
            
        default:
            zsys_error ("bad message type '%d', not sent\n", self->id);
            //  No recovery, this is a fatal application error
            assert (false);
    }
    //  Now serialize message into the frame
    zframe_t *frame = zframe_new (NULL, frame_size);
    self->needle = zframe_data (frame);
    PUT_NUMBER2 (0xAAA0 | 5);
    PUT_NUMBER1 (self->id);

    switch (self->id) {
        case ZEPS_MSG_ATTACH:
            PUT_STRING ("ZEPS");
            PUT_NUMBER2 (1);
            if (self->stream) {
                PUT_STRING (self->stream);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            break;

        case ZEPS_MSG_ATTACH_OK:
            break;

        case ZEPS_MSG_SUBSCRIBE:
            if (self->pattern) {
                PUT_STRING (self->pattern);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            PUT_NUMBER8 (self->latest);
            break;

        case ZEPS_MSG_SUBSCRIBE_OK:
            break;

        case ZEPS_MSG_CREDIT:
            PUT_NUMBER8 (self->credit);
            break;

        case ZEPS_MSG_PUBLISH:
            if (self->key) {
                PUT_STRING (self->key);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            if (self->body) {
                PUT_NUMBER4 (zchunk_size (self->body));
                memcpy (self->needle,
                        zchunk_data (self->body),
                        zchunk_size (self->body));
                self->needle += zchunk_size (self->body);
            }
            else
                PUT_NUMBER4 (0);    //  Empty chunk
            break;

        case ZEPS_MSG_DELIVER:
            PUT_NUMBER8 (self->sequence);
            if (self->key) {
                PUT_STRING (self->key);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            if (self->body) {
                PUT_NUMBER4 (zchunk_size (self->body));
                memcpy (self->needle,
                        zchunk_data (self->body),
                        zchunk_size (self->body));
                self->needle += zchunk_size (self->body);
            }
            else
                PUT_NUMBER4 (0);    //  Empty chunk
            break;

        case ZEPS_MSG_PING:
            break;

        case ZEPS_MSG_PING_OK:
            break;

        case ZEPS_MSG_DETACH:
            break;

        case ZEPS_MSG_DETACH_OK:
            break;

        case ZEPS_MSG_INVALID:
            if (self->reason) {
                PUT_STRING (self->reason);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            break;

    }
    //  Now send the data frame
    if (zmsg_append (msg, &frame)) {
        zmsg_destroy (&msg);
        zeps_msg_destroy (self_p);
        return NULL;
    }
    //  Destroy zeps_msg object
    zeps_msg_destroy (self_p);
    return msg;
}


//  --------------------------------------------------------------------------
//  Receive and parse a zeps_msg from the socket. Returns new object or
//  NULL if error. Will block if there's no message waiting.

zeps_msg_t *
zeps_msg_recv (void *input)
{
    assert (input);
    zmsg_t *msg = zmsg_recv (input);
    if (!msg)
        return NULL;            //  Interrupted
    //  If message came from a router socket, first frame is routing_id
    zframe_t *routing_id = NULL;
    if (zsocket_type (zsock_resolve (input)) == ZMQ_ROUTER) {
        routing_id = zmsg_pop (msg);
        //  If message was not valid, forget about it
        if (!routing_id || !zmsg_next (msg))
            return NULL;        //  Malformed or empty
    }
    zeps_msg_t *zeps_msg = zeps_msg_decode (&msg);
    if (zeps_msg && zsocket_type (zsock_resolve (input)) == ZMQ_ROUTER)
        zeps_msg->routing_id = routing_id;

    return zeps_msg;
}


//  --------------------------------------------------------------------------
//  Receive and parse a zeps_msg from the socket. Returns new object,
//  or NULL either if there was no input waiting, or the recv was interrupted.

zeps_msg_t *
zeps_msg_recv_nowait (void *input)
{
    assert (input);
    zmsg_t *msg = zmsg_recv_nowait (input);
    if (!msg)
        return NULL;            //  Interrupted
    //  If message came from a router socket, first frame is routing_id
    zframe_t *routing_id = NULL;
    if (zsocket_type (zsock_resolve (input)) == ZMQ_ROUTER) {
        routing_id = zmsg_pop (msg);
        //  If message was not valid, forget about it
        if (!routing_id || !zmsg_next (msg))
            return NULL;        //  Malformed or empty
    }
    zeps_msg_t *zeps_msg = zeps_msg_decode (&msg);
    if (zeps_msg && zsocket_type (zsock_resolve (input)) == ZMQ_ROUTER)
        zeps_msg->routing_id = routing_id;

    return zeps_msg;
}


//  --------------------------------------------------------------------------
//  Send the zeps_msg to the socket, and destroy it
//  Returns 0 if OK, else -1

int
zeps_msg_send (zeps_msg_t **self_p, void *output)
{
    assert (self_p);
    assert (*self_p);
    assert (output);

    //  Save routing_id if any, as encode will destroy it
    zeps_msg_t *self = *self_p;
    zframe_t *routing_id = self->routing_id;
    self->routing_id = NULL;

    //  Encode zeps_msg message to a single zmsg
    zmsg_t *msg = zeps_msg_encode (self_p);
    
    //  If we're sending to a ROUTER, send the routing_id first
    if (zsocket_type (zsock_resolve (output)) == ZMQ_ROUTER) {
        assert (routing_id);
        zmsg_prepend (msg, &routing_id);
    }
    else
        zframe_destroy (&routing_id);
        
    if (msg && zmsg_send (&msg, output) == 0)
        return 0;
    else
        return -1;              //  Failed to encode, or send
}


//  --------------------------------------------------------------------------
//  Send the zeps_msg to the output, and do not destroy it

int
zeps_msg_send_again (zeps_msg_t *self, void *output)
{
    assert (self);
    assert (output);
    self = zeps_msg_dup (self);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Encode ATTACH message

zmsg_t * 
zeps_msg_encode_attach (
    const char *stream)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_ATTACH);
    zeps_msg_set_stream (self, stream);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode ATTACH_OK message

zmsg_t * 
zeps_msg_encode_attach_ok (
)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_ATTACH_OK);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode SUBSCRIBE message

zmsg_t * 
zeps_msg_encode_subscribe (
    const char *pattern,
    uint64_t latest)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_SUBSCRIBE);
    zeps_msg_set_pattern (self, pattern);
    zeps_msg_set_latest (self, latest);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode SUBSCRIBE_OK message

zmsg_t * 
zeps_msg_encode_subscribe_ok (
)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_SUBSCRIBE_OK);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode CREDIT message

zmsg_t * 
zeps_msg_encode_credit (
    uint64_t credit)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_CREDIT);
    zeps_msg_set_credit (self, credit);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode PUBLISH message

zmsg_t * 
zeps_msg_encode_publish (
    const char *key,
    zchunk_t *body)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_PUBLISH);
    zeps_msg_set_key (self, key);
    zchunk_t *body_copy = zchunk_dup (body);
    zeps_msg_set_body (self, &body_copy);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode DELIVER message

zmsg_t * 
zeps_msg_encode_deliver (
    uint64_t sequence,
    const char *key,
    zchunk_t *body)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_DELIVER);
    zeps_msg_set_sequence (self, sequence);
    zeps_msg_set_key (self, key);
    zchunk_t *body_copy = zchunk_dup (body);
    zeps_msg_set_body (self, &body_copy);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode PING message

zmsg_t * 
zeps_msg_encode_ping (
)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_PING);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode PING_OK message

zmsg_t * 
zeps_msg_encode_ping_ok (
)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_PING_OK);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode DETACH message

zmsg_t * 
zeps_msg_encode_detach (
)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_DETACH);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode DETACH_OK message

zmsg_t * 
zeps_msg_encode_detach_ok (
)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_DETACH_OK);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode INVALID message

zmsg_t * 
zeps_msg_encode_invalid (
    const char *reason)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_INVALID);
    zeps_msg_set_reason (self, reason);
    return zeps_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Send the ATTACH to the socket in one step

int
zeps_msg_send_attach (
    void *output,
    const char *stream)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_ATTACH);
    zeps_msg_set_stream (self, stream);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the ATTACH_OK to the socket in one step

int
zeps_msg_send_attach_ok (
    void *output)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_ATTACH_OK);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the SUBSCRIBE to the socket in one step

int
zeps_msg_send_subscribe (
    void *output,
    const char *pattern,
    uint64_t latest)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_SUBSCRIBE);
    zeps_msg_set_pattern (self, pattern);
    zeps_msg_set_latest (self, latest);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the SUBSCRIBE_OK to the socket in one step

int
zeps_msg_send_subscribe_ok (
    void *output)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_SUBSCRIBE_OK);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the CREDIT to the socket in one step

int
zeps_msg_send_credit (
    void *output,
    uint64_t credit)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_CREDIT);
    zeps_msg_set_credit (self, credit);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the PUBLISH to the socket in one step

int
zeps_msg_send_publish (
    void *output,
    const char *key,
    zchunk_t *body)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_PUBLISH);
    zeps_msg_set_key (self, key);
    zchunk_t *body_copy = zchunk_dup (body);
    zeps_msg_set_body (self, &body_copy);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the DELIVER to the socket in one step

int
zeps_msg_send_deliver (
    void *output,
    uint64_t sequence,
    const char *key,
    zchunk_t *body)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_DELIVER);
    zeps_msg_set_sequence (self, sequence);
    zeps_msg_set_key (self, key);
    zchunk_t *body_copy = zchunk_dup (body);
    zeps_msg_set_body (self, &body_copy);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the PING to the socket in one step

int
zeps_msg_send_ping (
    void *output)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_PING);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the PING_OK to the socket in one step

int
zeps_msg_send_ping_ok (
    void *output)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_PING_OK);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the DETACH to the socket in one step

int
zeps_msg_send_detach (
    void *output)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_DETACH);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the DETACH_OK to the socket in one step

int
zeps_msg_send_detach_ok (
    void *output)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_DETACH_OK);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the INVALID to the socket in one step

int
zeps_msg_send_invalid (
    void *output,
    const char *reason)
{
    zeps_msg_t *self = zeps_msg_new (ZEPS_MSG_INVALID);
    zeps_msg_set_reason (self, reason);
    return zeps_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Duplicate the zeps_msg message

zeps_msg_t *
zeps_msg_dup (zeps_msg_t *self)
{
    if (!self)
        return NULL;
        
    zeps_msg_t *copy = zeps_msg_new (self->id);
    if (self->routing_id)
        copy->routing_id = zframe_dup (self->routing_id);
    switch (self->id) {
        case ZEPS_MSG_ATTACH:
            copy->protocol = self->protocol? strdup (self->protocol): NULL;
            copy->version = self->version;
            copy->stream = self->stream? strdup (self->stream): NULL;
            break;

        case ZEPS_MSG_ATTACH_OK:
            break;

        case ZEPS_MSG_SUBSCRIBE:
            copy->pattern = self->pattern? strdup (self->pattern): NULL;
            copy->latest = self->latest;
            break;

        case ZEPS_MSG_SUBSCRIBE_OK:
            break;

        case ZEPS_MSG_CREDIT:
            copy->credit = self->credit;
            break;

        case ZEPS_MSG_PUBLISH:
            copy->key = self->key? strdup (self->key): NULL;
            copy->body = self->body? zchunk_dup (self->body): NULL;
            break;

        case ZEPS_MSG_DELIVER:
            copy->sequence = self->sequence;
            copy->key = self->key? strdup (self->key): NULL;
            copy->body = self->body? zchunk_dup (self->body): NULL;
            break;

        case ZEPS_MSG_PING:
            break;

        case ZEPS_MSG_PING_OK:
            break;

        case ZEPS_MSG_DETACH:
            break;

        case ZEPS_MSG_DETACH_OK:
            break;

        case ZEPS_MSG_INVALID:
            copy->reason = self->reason? strdup (self->reason): NULL;
            break;

    }
    return copy;
}


//  --------------------------------------------------------------------------
//  Print contents of message to stdout

void
zeps_msg_print (zeps_msg_t *self)
{
    assert (self);
    switch (self->id) {
        case ZEPS_MSG_ATTACH:
            zsys_debug ("ZEPS_MSG_ATTACH:");
            zsys_debug ("    protocol=zeps");
            zsys_debug ("    version=1");
            if (self->stream)
                zsys_debug ("    stream='%s'", self->stream);
            else
                zsys_debug ("    stream=");
            break;
            
        case ZEPS_MSG_ATTACH_OK:
            zsys_debug ("ZEPS_MSG_ATTACH_OK:");
            break;
            
        case ZEPS_MSG_SUBSCRIBE:
            zsys_debug ("ZEPS_MSG_SUBSCRIBE:");
            if (self->pattern)
                zsys_debug ("    pattern='%s'", self->pattern);
            else
                zsys_debug ("    pattern=");
            zsys_debug ("    latest=%ld", (long) self->latest);
            break;
            
        case ZEPS_MSG_SUBSCRIBE_OK:
            zsys_debug ("ZEPS_MSG_SUBSCRIBE_OK:");
            break;
            
        case ZEPS_MSG_CREDIT:
            zsys_debug ("ZEPS_MSG_CREDIT:");
            zsys_debug ("    credit=%ld", (long) self->credit);
            break;
            
        case ZEPS_MSG_PUBLISH:
            zsys_debug ("ZEPS_MSG_PUBLISH:");
            if (self->key)
                zsys_debug ("    key='%s'", self->key);
            else
                zsys_debug ("    key=");
            zsys_debug ("    body=[ ... ]");
            break;
            
        case ZEPS_MSG_DELIVER:
            zsys_debug ("ZEPS_MSG_DELIVER:");
            zsys_debug ("    sequence=%ld", (long) self->sequence);
            if (self->key)
                zsys_debug ("    key='%s'", self->key);
            else
                zsys_debug ("    key=");
            zsys_debug ("    body=[ ... ]");
            break;
            
        case ZEPS_MSG_PING:
            zsys_debug ("ZEPS_MSG_PING:");
            break;
            
        case ZEPS_MSG_PING_OK:
            zsys_debug ("ZEPS_MSG_PING_OK:");
            break;
            
        case ZEPS_MSG_DETACH:
            zsys_debug ("ZEPS_MSG_DETACH:");
            break;
            
        case ZEPS_MSG_DETACH_OK:
            zsys_debug ("ZEPS_MSG_DETACH_OK:");
            break;
            
        case ZEPS_MSG_INVALID:
            zsys_debug ("ZEPS_MSG_INVALID:");
            if (self->reason)
                zsys_debug ("    reason='%s'", self->reason);
            else
                zsys_debug ("    reason=");
            break;
            
    }
}


//  --------------------------------------------------------------------------
//  Get/set the message routing_id

zframe_t *
zeps_msg_routing_id (zeps_msg_t *self)
{
    assert (self);
    return self->routing_id;
}

void
zeps_msg_set_routing_id (zeps_msg_t *self, zframe_t *routing_id)
{
    if (self->routing_id)
        zframe_destroy (&self->routing_id);
    self->routing_id = zframe_dup (routing_id);
}


//  --------------------------------------------------------------------------
//  Get/set the zeps_msg id

int
zeps_msg_id (zeps_msg_t *self)
{
    assert (self);
    return self->id;
}

void
zeps_msg_set_id (zeps_msg_t *self, int id)
{
    self->id = id;
}

//  --------------------------------------------------------------------------
//  Return a printable command string

const char *
zeps_msg_command (zeps_msg_t *self)
{
    assert (self);
    switch (self->id) {
        case ZEPS_MSG_ATTACH:
            return ("ATTACH");
            break;
        case ZEPS_MSG_ATTACH_OK:
            return ("ATTACH_OK");
            break;
        case ZEPS_MSG_SUBSCRIBE:
            return ("SUBSCRIBE");
            break;
        case ZEPS_MSG_SUBSCRIBE_OK:
            return ("SUBSCRIBE_OK");
            break;
        case ZEPS_MSG_CREDIT:
            return ("CREDIT");
            break;
        case ZEPS_MSG_PUBLISH:
            return ("PUBLISH");
            break;
        case ZEPS_MSG_DELIVER:
            return ("DELIVER");
            break;
        case ZEPS_MSG_PING:
            return ("PING");
            break;
        case ZEPS_MSG_PING_OK:
            return ("PING_OK");
            break;
        case ZEPS_MSG_DETACH:
            return ("DETACH");
            break;
        case ZEPS_MSG_DETACH_OK:
            return ("DETACH_OK");
            break;
        case ZEPS_MSG_INVALID:
            return ("INVALID");
            break;
    }
    return "?";
}

//  --------------------------------------------------------------------------
//  Get/set the stream field

const char *
zeps_msg_stream (zeps_msg_t *self)
{
    assert (self);
    return self->stream;
}

void
zeps_msg_set_stream (zeps_msg_t *self, const char *format, ...)
{
    //  Format stream from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->stream);
    self->stream = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the pattern field

const char *
zeps_msg_pattern (zeps_msg_t *self)
{
    assert (self);
    return self->pattern;
}

void
zeps_msg_set_pattern (zeps_msg_t *self, const char *format, ...)
{
    //  Format pattern from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->pattern);
    self->pattern = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the latest field

uint64_t
zeps_msg_latest (zeps_msg_t *self)
{
    assert (self);
    return self->latest;
}

void
zeps_msg_set_latest (zeps_msg_t *self, uint64_t latest)
{
    assert (self);
    self->latest = latest;
}


//  --------------------------------------------------------------------------
//  Get/set the credit field

uint64_t
zeps_msg_credit (zeps_msg_t *self)
{
    assert (self);
    return self->credit;
}

void
zeps_msg_set_credit (zeps_msg_t *self, uint64_t credit)
{
    assert (self);
    self->credit = credit;
}


//  --------------------------------------------------------------------------
//  Get/set the key field

const char *
zeps_msg_key (zeps_msg_t *self)
{
    assert (self);
    return self->key;
}

void
zeps_msg_set_key (zeps_msg_t *self, const char *format, ...)
{
    //  Format key from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->key);
    self->key = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get the body field without transferring ownership

zchunk_t *
zeps_msg_body (zeps_msg_t *self)
{
    assert (self);
    return self->body;
}

//  Get the body field and transfer ownership to caller

zchunk_t *
zeps_msg_get_body (zeps_msg_t *self)
{
    zchunk_t *body = self->body;
    self->body = NULL;
    return body;
}

//  Set the body field, transferring ownership from caller

void
zeps_msg_set_body (zeps_msg_t *self, zchunk_t **chunk_p)
{
    assert (self);
    assert (chunk_p);
    zchunk_destroy (&self->body);
    self->body = *chunk_p;
    *chunk_p = NULL;
}


//  --------------------------------------------------------------------------
//  Get/set the sequence field

uint64_t
zeps_msg_sequence (zeps_msg_t *self)
{
    assert (self);
    return self->sequence;
}

void
zeps_msg_set_sequence (zeps_msg_t *self, uint64_t sequence)
{
    assert (self);
    self->sequence = sequence;
}


//  --------------------------------------------------------------------------
//  Get/set the reason field

const char *
zeps_msg_reason (zeps_msg_t *self)
{
    assert (self);
    return self->reason;
}

void
zeps_msg_set_reason (zeps_msg_t *self, const char *format, ...)
{
    //  Format reason from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->reason);
    self->reason = zsys_vprintf (format, argptr);
    va_end (argptr);
}



//  --------------------------------------------------------------------------
//  Selftest

int
zeps_msg_test (bool verbose)
{
    printf (" * zeps_msg: ");

    //  @selftest
    //  Simple create/destroy test
    zeps_msg_t *self = zeps_msg_new (0);
    assert (self);
    zeps_msg_destroy (&self);

    //  Create pair of sockets we can send through
    zsock_t *input = zsock_new (ZMQ_ROUTER);
    assert (input);
    zsock_connect (input, "inproc://selftest-zeps_msg");

    zsock_t *output = zsock_new (ZMQ_DEALER);
    assert (output);
    zsock_bind (output, "inproc://selftest-zeps_msg");

    //  Encode/send/decode and verify each message type
    int instance;
    zeps_msg_t *copy;
    self = zeps_msg_new (ZEPS_MSG_ATTACH);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    zeps_msg_set_stream (self, "Life is short but Now lasts for ever");
    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        assert (streq (zeps_msg_stream (self), "Life is short but Now lasts for ever"));
        zeps_msg_destroy (&self);
    }
    self = zeps_msg_new (ZEPS_MSG_ATTACH_OK);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        zeps_msg_destroy (&self);
    }
    self = zeps_msg_new (ZEPS_MSG_SUBSCRIBE);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    zeps_msg_set_pattern (self, "Life is short but Now lasts for ever");
    zeps_msg_set_latest (self, 123);
    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        assert (streq (zeps_msg_pattern (self), "Life is short but Now lasts for ever"));
        assert (zeps_msg_latest (self) == 123);
        zeps_msg_destroy (&self);
    }
    self = zeps_msg_new (ZEPS_MSG_SUBSCRIBE_OK);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        zeps_msg_destroy (&self);
    }
    self = zeps_msg_new (ZEPS_MSG_CREDIT);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    zeps_msg_set_credit (self, 123);
    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        assert (zeps_msg_credit (self) == 123);
        zeps_msg_destroy (&self);
    }
    self = zeps_msg_new (ZEPS_MSG_PUBLISH);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    zeps_msg_set_key (self, "Life is short but Now lasts for ever");
    zchunk_t *publish_body = zchunk_new ("Captcha Diem", 12);
    zeps_msg_set_body (self, &publish_body);
    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        assert (streq (zeps_msg_key (self), "Life is short but Now lasts for ever"));
        assert (memcmp (zchunk_data (zeps_msg_body (self)), "Captcha Diem", 12) == 0);
        zeps_msg_destroy (&self);
    }
    self = zeps_msg_new (ZEPS_MSG_DELIVER);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    zeps_msg_set_sequence (self, 123);
    zeps_msg_set_key (self, "Life is short but Now lasts for ever");
    zchunk_t *deliver_body = zchunk_new ("Captcha Diem", 12);
    zeps_msg_set_body (self, &deliver_body);
    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        assert (zeps_msg_sequence (self) == 123);
        assert (streq (zeps_msg_key (self), "Life is short but Now lasts for ever"));
        assert (memcmp (zchunk_data (zeps_msg_body (self)), "Captcha Diem", 12) == 0);
        zeps_msg_destroy (&self);
    }
    self = zeps_msg_new (ZEPS_MSG_PING);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        zeps_msg_destroy (&self);
    }
    self = zeps_msg_new (ZEPS_MSG_PING_OK);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        zeps_msg_destroy (&self);
    }
    self = zeps_msg_new (ZEPS_MSG_DETACH);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        zeps_msg_destroy (&self);
    }
    self = zeps_msg_new (ZEPS_MSG_DETACH_OK);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        zeps_msg_destroy (&self);
    }
    self = zeps_msg_new (ZEPS_MSG_INVALID);
    
    //  Check that _dup works on empty message
    copy = zeps_msg_dup (self);
    assert (copy);
    zeps_msg_destroy (&copy);

    zeps_msg_set_reason (self, "Life is short but Now lasts for ever");
    //  Send twice from same object
    zeps_msg_send_again (self, output);
    zeps_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = zeps_msg_recv (input);
        assert (self);
        assert (zeps_msg_routing_id (self));
        
        assert (streq (zeps_msg_reason (self), "Life is short but Now lasts for ever"));
        zeps_msg_destroy (&self);
    }

    zsock_destroy (&input);
    zsock_destroy (&output);
    //  @end

    printf ("OK\n");
    return 0;
}
