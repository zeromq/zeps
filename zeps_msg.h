/*  =========================================================================
    zeps_msg - ZeroMQ Enterprise Publish-Subscribe Protocol
    
    Codec header for zeps_msg.

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

#ifndef __ZEPS_MSG_H_INCLUDED__
#define __ZEPS_MSG_H_INCLUDED__

/*  These are the zeps_msg messages:

    ATTACH - Client opens stream
        protocol            string      Constant "ZEPS"
        version             number 2    Protocol version 1
        stream              string      Stream name

    ATTACH_OK - Server grants the client access

    SUBSCRIBE - Client subscribes to a set of topics
        pattern             string      Subscription pattern
        latest              number 8    Last known sequence

    SUBSCRIBE_OK - Server confirms the subscription

    CREDIT - Client sends credit to the server
        credit              number 8    Credit, in bytes

    PUBLISH - Client publishes a new message to the open stream
        key                 string      Message routing key
        body                chunk       Message contents

    DELIVER - Server delivers a message to the client
        sequence            number 8    Sequenced per stream
        key                 string      Message routing key
        body                chunk       Message contents

    PING - Client or server sends a heartbeat

    PING_OK - Client or server answers a heartbeat

    DETACH - Client closes the peering

    DETACH_OK - Server handshake the close

    INVALID - Server tells client it sent an invalid message
        reason              string      Printable explanation
*/


#define ZEPS_MSG_ATTACH                     1
#define ZEPS_MSG_ATTACH_OK                  2
#define ZEPS_MSG_SUBSCRIBE                  3
#define ZEPS_MSG_SUBSCRIBE_OK               4
#define ZEPS_MSG_CREDIT                     5
#define ZEPS_MSG_PUBLISH                    6
#define ZEPS_MSG_DELIVER                    7
#define ZEPS_MSG_PING                       8
#define ZEPS_MSG_PING_OK                    9
#define ZEPS_MSG_DETACH                     10
#define ZEPS_MSG_DETACH_OK                  11
#define ZEPS_MSG_INVALID                    12

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _zeps_msg_t zeps_msg_t;

//  @interface
//  Create a new zeps_msg
zeps_msg_t *
    zeps_msg_new (int id);

//  Destroy the zeps_msg
void
    zeps_msg_destroy (zeps_msg_t **self_p);

//  Parse a zeps_msg from zmsg_t. Returns a new object, or NULL if
//  the message could not be parsed, or was NULL. Destroys msg and 
//  nullifies the msg reference.
zeps_msg_t *
    zeps_msg_decode (zmsg_t **msg_p);

//  Encode zeps_msg into zmsg and destroy it. Returns a newly created
//  object or NULL if error. Use when not in control of sending the message.
zmsg_t *
    zeps_msg_encode (zeps_msg_t **self_p);

//  Receive and parse a zeps_msg from the socket. Returns new object, 
//  or NULL if error. Will block if there's no message waiting.
zeps_msg_t *
    zeps_msg_recv (void *input);

//  Receive and parse a zeps_msg from the socket. Returns new object, 
//  or NULL either if there was no input waiting, or the recv was interrupted.
zeps_msg_t *
    zeps_msg_recv_nowait (void *input);

//  Send the zeps_msg to the output, and destroy it
int
    zeps_msg_send (zeps_msg_t **self_p, void *output);

//  Send the zeps_msg to the output, and do not destroy it
int
    zeps_msg_send_again (zeps_msg_t *self, void *output);

//  Encode the ATTACH 
zmsg_t *
    zeps_msg_encode_attach (
        const char *stream);

//  Encode the ATTACH_OK 
zmsg_t *
    zeps_msg_encode_attach_ok (
);

//  Encode the SUBSCRIBE 
zmsg_t *
    zeps_msg_encode_subscribe (
        const char *pattern,
        uint64_t latest);

//  Encode the SUBSCRIBE_OK 
zmsg_t *
    zeps_msg_encode_subscribe_ok (
);

//  Encode the CREDIT 
zmsg_t *
    zeps_msg_encode_credit (
        uint64_t credit);

//  Encode the PUBLISH 
zmsg_t *
    zeps_msg_encode_publish (
        const char *key,
        zchunk_t *body);

//  Encode the DELIVER 
zmsg_t *
    zeps_msg_encode_deliver (
        uint64_t sequence,
        const char *key,
        zchunk_t *body);

//  Encode the PING 
zmsg_t *
    zeps_msg_encode_ping (
);

//  Encode the PING_OK 
zmsg_t *
    zeps_msg_encode_ping_ok (
);

//  Encode the DETACH 
zmsg_t *
    zeps_msg_encode_detach (
);

//  Encode the DETACH_OK 
zmsg_t *
    zeps_msg_encode_detach_ok (
);

//  Encode the INVALID 
zmsg_t *
    zeps_msg_encode_invalid (
        const char *reason);


//  Send the ATTACH to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_attach (void *output,
        const char *stream);
    
//  Send the ATTACH_OK to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_attach_ok (void *output);
    
//  Send the SUBSCRIBE to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_subscribe (void *output,
        const char *pattern,
        uint64_t latest);
    
//  Send the SUBSCRIBE_OK to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_subscribe_ok (void *output);
    
//  Send the CREDIT to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_credit (void *output,
        uint64_t credit);
    
//  Send the PUBLISH to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_publish (void *output,
        const char *key,
        zchunk_t *body);
    
//  Send the DELIVER to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_deliver (void *output,
        uint64_t sequence,
        const char *key,
        zchunk_t *body);
    
//  Send the PING to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_ping (void *output);
    
//  Send the PING_OK to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_ping_ok (void *output);
    
//  Send the DETACH to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_detach (void *output);
    
//  Send the DETACH_OK to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_detach_ok (void *output);
    
//  Send the INVALID to the output in one step
//  WARNING, this call will fail if output is of type ZMQ_ROUTER.
int
    zeps_msg_send_invalid (void *output,
        const char *reason);
    
//  Duplicate the zeps_msg message
zeps_msg_t *
    zeps_msg_dup (zeps_msg_t *self);

//  Print contents of message to stdout
void
    zeps_msg_print (zeps_msg_t *self);

//  Get/set the message routing id
zframe_t *
    zeps_msg_routing_id (zeps_msg_t *self);
void
    zeps_msg_set_routing_id (zeps_msg_t *self, zframe_t *routing_id);

//  Get the zeps_msg id and printable command
int
    zeps_msg_id (zeps_msg_t *self);
void
    zeps_msg_set_id (zeps_msg_t *self, int id);
const char *
    zeps_msg_command (zeps_msg_t *self);

//  Get/set the stream field
const char *
    zeps_msg_stream (zeps_msg_t *self);
void
    zeps_msg_set_stream (zeps_msg_t *self, const char *format, ...);

//  Get/set the pattern field
const char *
    zeps_msg_pattern (zeps_msg_t *self);
void
    zeps_msg_set_pattern (zeps_msg_t *self, const char *format, ...);

//  Get/set the latest field
uint64_t
    zeps_msg_latest (zeps_msg_t *self);
void
    zeps_msg_set_latest (zeps_msg_t *self, uint64_t latest);

//  Get/set the credit field
uint64_t
    zeps_msg_credit (zeps_msg_t *self);
void
    zeps_msg_set_credit (zeps_msg_t *self, uint64_t credit);

//  Get/set the key field
const char *
    zeps_msg_key (zeps_msg_t *self);
void
    zeps_msg_set_key (zeps_msg_t *self, const char *format, ...);

//  Get a copy of the body field
zchunk_t *
    zeps_msg_body (zeps_msg_t *self);
//  Get the body field and transfer ownership to caller
zchunk_t *
    zeps_msg_get_body (zeps_msg_t *self);
//  Set the body field, transferring ownership from caller
void
    zeps_msg_set_body (zeps_msg_t *self, zchunk_t **chunk_p);

//  Get/set the sequence field
uint64_t
    zeps_msg_sequence (zeps_msg_t *self);
void
    zeps_msg_set_sequence (zeps_msg_t *self, uint64_t sequence);

//  Get/set the reason field
const char *
    zeps_msg_reason (zeps_msg_t *self);
void
    zeps_msg_set_reason (zeps_msg_t *self, const char *format, ...);

//  Self test of this class
int
    zeps_msg_test (bool verbose);
//  @end

//  For backwards compatibility with old codecs
#define zeps_msg_dump       zeps_msg_print

#ifdef __cplusplus
}
#endif

#endif
