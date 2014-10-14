/*  =========================================================================
    zeps_server - ZeroMQ Enterprise Publish-Subscribe Protocol Server

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

    * The XML model used for this code generation: zeps_server.xml
    * The code generation script that built this file: zproto_server_c.gsl
    ************************************************************************

    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of zbroker, the ZeroMQ broker project.           
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

#ifndef __ZEPS_SERVER_H_INCLUDED__
#define __ZEPS_SERVER_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  To work with zeps_server, use the CZMQ zactor API:
//
//  Create new zeromq enterprise publish-subscribe protocol server instance, passing logging prefix:
//
//      zactor_t *zeromq_enterprise_publish_subscribe_protocol_server = zactor_new (zeps_server, "myname");
//  
//  Destroy zeromq enterprise publish-subscribe protocol server instance
//
//      zactor_destroy (&zeromq_enterprise_publish_subscribe_protocol_server);
//  
//  Enable verbose logging of commands and activity:
//
//      zstr_send (server, "VERBOSE");
//
//  Bind zeromq enterprise publish-subscribe protocol server to specified endpoint. TCP endpoints may specify
//  the port number as "*" to aquire an ephemeral port:
//
//      zstr_sendx (zeromq_enterprise_publish_subscribe_protocol_server, "BIND", endpoint, NULL);
//
//  Return assigned port number, specifically when BIND was done using an
//  an ephemeral port:
//
//      zstr_sendx (zeromq_enterprise_publish_subscribe_protocol_server, "PORT", NULL);
//      char *command, *port_str;
//      zstr_recvx (zeromq_enterprise_publish_subscribe_protocol_server, &command, &port_str, NULL);
//      assert (streq (command, "PORT"));
//
//  Specify configuration file to load, overwriting any previous loaded
//  configuration file or options:
//
//      zstr_sendx (zeromq_enterprise_publish_subscribe_protocol_server, "CONFIGURE", filename, NULL);
//
//  Set configuration path value:
//
//      zstr_sendx (zeromq_enterprise_publish_subscribe_protocol_server, "SET", path, value, NULL);
//    
//  Send zmsg_t instance to zeromq enterprise publish-subscribe protocol server:
//
//      zactor_send (zeromq_enterprise_publish_subscribe_protocol_server, &msg);
//
//  Receive zmsg_t instance from zeromq enterprise publish-subscribe protocol server:
//
//      zmsg_t *msg = zactor_recv (zeromq_enterprise_publish_subscribe_protocol_server);
//
//  This is the zeps_server constructor as a zactor_fn:
//
void
    zeps_server (zsock_t *pipe, void *args);

//  Self test of this class
void
    zeps_server_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
