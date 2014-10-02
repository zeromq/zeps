/*  =========================================================================
    ZepsMsg - ZeroMQ Enterprise Publish-Subscribe Protocol

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

    * The XML model used for this code generation: zeps_msg.xml
    * The code generation script that built this file: zproto_codec_java.gsl
    ************************************************************************
    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of zbroker, the ZeroMQ broker project.           
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

/*  These are the ZepsMsg messages:

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

package org.zproto;

import java.util.*;
import java.nio.ByteBuffer;

import org.zeromq.ZFrame;
import org.zeromq.ZMsg;
import org.zeromq.ZMQ;
import org.zeromq.ZMQ.Socket;

public class ZepsMsg implements java.io.Closeable
{

    public static final int ATTACH                = 1;
    public static final int ATTACH_OK             = 2;
    public static final int SUBSCRIBE             = 3;
    public static final int SUBSCRIBE_OK          = 4;
    public static final int CREDIT                = 5;
    public static final int PUBLISH               = 6;
    public static final int DELIVER               = 7;
    public static final int PING                  = 8;
    public static final int PING_OK               = 9;
    public static final int DETACH                = 10;
    public static final int DETACH_OK             = 11;
    public static final int INVALID               = 12;

    //  Structure of our class
    private ZFrame routingId;           // Routing_id from ROUTER, if any
    private int id;                     //  ZepsMsg message ID
    private ByteBuffer needle;          //  Read/write pointer for serialization

    private String protocol;
    private int version;
    private String stream;
    private String pattern;
    private long latest;
    private long credit;
    private String key;
    private byte[] body;
    private long sequence;
    private String reason;

    public ZepsMsg( int id )
    {
        this.id = id;
    }

    public void destroy()
    {
        close();
    }

    @Override
    public void close()
    {
        //  Destroy frame fields
    }
    //  --------------------------------------------------------------------------
    //  Network data encoding macros


    //  Put a 1-byte number to the frame
    private final void putNumber1 (int value)
    {
        needle.put ((byte) value);
    }

    //  Get a 1-byte number to the frame
    //  then make it unsigned
    private int getNumber1 ()
    {
        int value = needle.get ();
        if (value < 0)
            value = (0xff) & value;
        return value;
    }

    //  Put a 2-byte number to the frame
    private final void putNumber2 (int value)
    {
        needle.putShort ((short) value);
    }

    //  Get a 2-byte number to the frame
    private int getNumber2 ()
    {
        int value = needle.getShort ();
        if (value < 0)
            value = (0xffff) & value;
        return value;
    }

    //  Put a 4-byte number to the frame
    private final void putNumber4 (long value)
    {
        needle.putInt ((int) value);
    }

    //  Get a 4-byte number to the frame
    //  then make it unsigned
    private long getNumber4 ()
    {
        long value = needle.getInt ();
        if (value < 0)
            value = (0xffffffff) & value;
        return value;
    }

    //  Put a 8-byte number to the frame
    public void putNumber8 (long value)
    {
        needle.putLong (value);
    }

    //  Get a 8-byte number to the frame
    public long getNumber8 ()
    {
        return needle.getLong ();
    }


    //  Put a block to the frame
    private void putBlock (byte [] value, int size)
    {
        needle.put (value, 0, size);
    }

    private byte [] getBlock (int size)
    {
        byte [] value = new byte [size];
        needle.get (value);

        return value;
    }

    //  Put a string to the frame
    public void putString (String value)
    {
        needle.put ((byte) value.length ());
        needle.put (value.getBytes());
    }

    //  Get a string from the frame
    public String getString ()
    {
        int size = getNumber1 ();
        byte [] value = new byte [size];
        needle.get (value);

        return new String (value);
    }

        //  Put a string to the frame
    public void putLongString (String value)
    {
        needle.putInt (value.length ());
        needle.put (value.getBytes());
    }

    //  Get a string from the frame
    public String getLongString ()
    {
        long size = getNumber4 ();
        byte [] value = new byte [(int) size];
        needle.get (value);

        return new String (value);
    }
    //  --------------------------------------------------------------------------
    //  Receive and parse a ZepsMsg from the socket. Returns new object or
    //  null if error. Will block if there's no message waiting.

    public static ZepsMsg recv (Socket input)
    {
        assert (input != null);
        ZepsMsg self = new ZepsMsg (0);
        ZFrame frame = null;

        try {
            //  Read valid message frame from socket; we loop over any
            //  garbage data we might receive from badly-connected peers
            while (true) {
                //  If we're reading from a ROUTER socket, get routingId
                if (input.getType () == ZMQ.ROUTER) {
                    self.routingId = ZFrame.recvFrame (input);
                    if (self.routingId == null)
                        return null;         //  Interrupted
                    if (!input.hasReceiveMore ())
                        throw new IllegalArgumentException ();
                }
                //  Read and parse command in frame
                frame = ZFrame.recvFrame (input);
                if (frame == null)
                    return null;             //  Interrupted

                //  Get and check protocol signature
                self.needle = ByteBuffer.wrap (frame.getData ());
                int signature = self.getNumber2 ();
                if (signature == (0xAAA0 | 5))
                    break;                  //  Valid signature

                //  Protocol assertion, drop message
                while (input.hasReceiveMore ()) {
                    frame.destroy ();
                    frame = ZFrame.recvFrame (input);
                }
                frame.destroy ();
            }

            //  Get message id, which is first byte in frame
            self.id = self.getNumber1 ();
            int listSize;
            int hashSize;

            switch (self.id) {
            case ATTACH:
                self.protocol = self.getString ();
                if (!self.protocol.equals( "ZEPS"))
                    throw new IllegalArgumentException ();
                self.version = self.getNumber2 ();
                if (self.version != 1)
                    throw new IllegalArgumentException ();
                self.stream = self.getString ();
                break;

            case ATTACH_OK:
                break;

            case SUBSCRIBE:
                self.pattern = self.getString ();
                self.latest = self.getNumber8 ();
                break;

            case SUBSCRIBE_OK:
                break;

            case CREDIT:
                self.credit = self.getNumber8 ();
                break;

            case PUBLISH:
                self.key = self.getString ();
                self.body = self.getBlock((int) self.getNumber4());
                break;

            case DELIVER:
                self.sequence = self.getNumber8 ();
                self.key = self.getString ();
                self.body = self.getBlock((int) self.getNumber4());
                break;

            case PING:
                break;

            case PING_OK:
                break;

            case DETACH:
                break;

            case DETACH_OK:
                break;

            case INVALID:
                self.reason = self.getString ();
                break;

            default:
                throw new IllegalArgumentException ();
            }

            return self;

        } catch (Exception e) {
            //  Error returns
            System.out.printf ("E: malformed message '%d'\n", self.id);
            self.destroy ();
            return null;
        } finally {
            if (frame != null)
                frame.destroy ();
        }
    }

    //  --------------------------------------------------------------------------
    //  Send the ZepsMsg to the socket, and destroy it

    public boolean send (Socket socket)
    {
        assert (socket != null);

        ZMsg msg = new ZMsg();

        int frameSize = 2 + 1;          //  Signature and message ID
        switch (id) {
        case ATTACH:
            //  protocol is a string with 1-byte length
            frameSize += 1 + "ZEPS".length());
            //  version is a 2-byte integer
            frameSize += 2;
            //  stream is a string with 1-byte length
            frameSize ++;
            frameSize += (stream != null) ? stream.length() : 0;
            break;

        case ATTACH_OK:
            break;

        case SUBSCRIBE:
            //  pattern is a string with 1-byte length
            frameSize ++;
            frameSize += (pattern != null) ? pattern.length() : 0;
            //  latest is a 8-byte integer
            frameSize += 8;
            break;

        case SUBSCRIBE_OK:
            break;

        case CREDIT:
            //  credit is a 8-byte integer
            frameSize += 8;
            break;

        case PUBLISH:
            //  key is a string with 1-byte length
            frameSize ++;
            frameSize += (key != null) ? key.length() : 0;
            //  body is a chunk with 4-byte length
            frameSize += 4;
            frameSize += (body != null) ? body.length : 0;
            break;

        case DELIVER:
            //  sequence is a 8-byte integer
            frameSize += 8;
            //  key is a string with 1-byte length
            frameSize ++;
            frameSize += (key != null) ? key.length() : 0;
            //  body is a chunk with 4-byte length
            frameSize += 4;
            frameSize += (body != null) ? body.length : 0;
            break;

        case PING:
            break;

        case PING_OK:
            break;

        case DETACH:
            break;

        case DETACH_OK:
            break;

        case INVALID:
            //  reason is a string with 1-byte length
            frameSize ++;
            frameSize += (reason != null) ? reason.length() : 0;
            break;

        default:
            System.out.printf ("E: bad message type '%d', not sent\n", id);
            assert (false);
        }
        //  Now serialize message into the frame
        ZFrame frame = new ZFrame (new byte [frameSize]);
        needle = ByteBuffer.wrap (frame.getData ());
        int frameFlags = 0;
        putNumber2 (0xAAA0 | 5);
        putNumber1 ((byte) id);

        switch (id) {
        case ATTACH:
            putString ("ZEPS");
            putNumber2 (1);
            if (stream != null)
                putString (stream);
            else
                putNumber1 ((byte) 0);      //  Empty string
            break;

        case ATTACH_OK:
            break;

        case SUBSCRIBE:
            if (pattern != null)
                putString (pattern);
            else
                putNumber1 ((byte) 0);      //  Empty string
            putNumber8 (latest);
            break;

        case SUBSCRIBE_OK:
            break;

        case CREDIT:
            putNumber8 (credit);
            break;

        case PUBLISH:
            if (key != null)
                putString (key);
            else
                putNumber1 ((byte) 0);      //  Empty string
              if(body != null) {
                  putNumber4(body.length);
                  needle.put(body, 0, body.length);
              } else {
                  putNumber4(0);
              }
            break;

        case DELIVER:
            putNumber8 (sequence);
            if (key != null)
                putString (key);
            else
                putNumber1 ((byte) 0);      //  Empty string
              if(body != null) {
                  putNumber4(body.length);
                  needle.put(body, 0, body.length);
              } else {
                  putNumber4(0);
              }
            break;

        case PING:
            break;

        case PING_OK:
            break;

        case DETACH:
            break;

        case DETACH_OK:
            break;

        case INVALID:
            if (reason != null)
                putString (reason);
            else
                putNumber1 ((byte) 0);      //  Empty string
            break;

        }
        //  Now send the data frame
        msg.add(frame);

        //  Now send any frame fields, in order
        switch (id) {
        }
        switch (id) {
        }
        //  Destroy ZepsMsg object
        msg.send(socket);
        destroy ();
        return true;
    }


//  --------------------------------------------------------------------------
//  Send the ATTACH to the socket in one step

    public static void sendAttach (
        Socket output,
        String stream)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.ATTACH);
        self.setStream (stream);
        self.send (output);
    }

//  --------------------------------------------------------------------------
//  Send the ATTACH_OK to the socket in one step

    public static void sendAttach_Ok (
        Socket output)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.ATTACH_OK);
        self.send (output);
    }

//  --------------------------------------------------------------------------
//  Send the SUBSCRIBE to the socket in one step

    public static void sendSubscribe (
        Socket output,
        String pattern,
        long latest)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.SUBSCRIBE);
        self.setPattern (pattern);
        self.setLatest (latest);
        self.send (output);
    }

//  --------------------------------------------------------------------------
//  Send the SUBSCRIBE_OK to the socket in one step

    public static void sendSubscribe_Ok (
        Socket output)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.SUBSCRIBE_OK);
        self.send (output);
    }

//  --------------------------------------------------------------------------
//  Send the CREDIT to the socket in one step

    public static void sendCredit (
        Socket output,
        long credit)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.CREDIT);
        self.setCredit (credit);
        self.send (output);
    }

//  --------------------------------------------------------------------------
//  Send the PUBLISH to the socket in one step

    public static void sendPublish (
        Socket output,
        String key,
        byte[] body)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.PUBLISH);
        self.setKey (key);
        self.send (output);
    }

//  --------------------------------------------------------------------------
//  Send the DELIVER to the socket in one step

    public static void sendDeliver (
        Socket output,
        long sequence,
        String key,
        byte[] body)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.DELIVER);
        self.setSequence (sequence);
        self.setKey (key);
        self.send (output);
    }

//  --------------------------------------------------------------------------
//  Send the PING to the socket in one step

    public static void sendPing (
        Socket output)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.PING);
        self.send (output);
    }

//  --------------------------------------------------------------------------
//  Send the PING_OK to the socket in one step

    public static void sendPing_Ok (
        Socket output)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.PING_OK);
        self.send (output);
    }

//  --------------------------------------------------------------------------
//  Send the DETACH to the socket in one step

    public static void sendDetach (
        Socket output)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.DETACH);
        self.send (output);
    }

//  --------------------------------------------------------------------------
//  Send the DETACH_OK to the socket in one step

    public static void sendDetach_Ok (
        Socket output)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.DETACH_OK);
        self.send (output);
    }

//  --------------------------------------------------------------------------
//  Send the INVALID to the socket in one step

    public static void sendInvalid (
        Socket output,
        String reason)
    {
        ZepsMsg self = new ZepsMsg (ZepsMsg.INVALID);
        self.setReason (reason);
        self.send (output);
    }


    //  --------------------------------------------------------------------------
    //  Duplicate the ZepsMsg message

    public ZepsMsg dup ()
    {
        ZepsMsg copy = new ZepsMsg (this.id);
        if (this.routingId != null)
            copy.routingId = this.routingId.duplicate ();
        switch (this.id) {
        case ATTACH:
            copy.protocol = this.protocol;
            copy.version = this.version;
            copy.stream = this.stream;
        break;
        case ATTACH_OK:
        break;
        case SUBSCRIBE:
            copy.pattern = this.pattern;
            copy.latest = this.latest;
        break;
        case SUBSCRIBE_OK:
        break;
        case CREDIT:
            copy.credit = this.credit;
        break;
        case PUBLISH:
            copy.key = this.key;
        break;
        case DELIVER:
            copy.sequence = this.sequence;
            copy.key = this.key;
        break;
        case PING:
        break;
        case PING_OK:
        break;
        case DETACH:
        break;
        case DETACH_OK:
        break;
        case INVALID:
            copy.reason = this.reason;
        break;
        }
        return copy;
    }


    //  --------------------------------------------------------------------------
    //  Print contents of message to stdout

    public void dump ()
    {
        switch (id) {
        case ATTACH:
            System.out.println ("ATTACH:");
            System.out.printf ("    protocol=zeps\n");
            System.out.printf ("    version=1\n");
            if (stream != null)
                System.out.printf ("    stream='%s'\n", stream);
            else
                System.out.printf ("    stream=\n");
            break;

        case ATTACH_OK:
            System.out.println ("ATTACH_OK:");
            break;

        case SUBSCRIBE:
            System.out.println ("SUBSCRIBE:");
            if (pattern != null)
                System.out.printf ("    pattern='%s'\n", pattern);
            else
                System.out.printf ("    pattern=\n");
            System.out.printf ("    latest=%d\n", (long)latest);
            break;

        case SUBSCRIBE_OK:
            System.out.println ("SUBSCRIBE_OK:");
            break;

        case CREDIT:
            System.out.println ("CREDIT:");
            System.out.printf ("    credit=%d\n", (long)credit);
            break;

        case PUBLISH:
            System.out.println ("PUBLISH:");
            if (key != null)
                System.out.printf ("    key='%s'\n", key);
            else
                System.out.printf ("    key=\n");
            break;

        case DELIVER:
            System.out.println ("DELIVER:");
            System.out.printf ("    sequence=%d\n", (long)sequence);
            if (key != null)
                System.out.printf ("    key='%s'\n", key);
            else
                System.out.printf ("    key=\n");
            break;

        case PING:
            System.out.println ("PING:");
            break;

        case PING_OK:
            System.out.println ("PING_OK:");
            break;

        case DETACH:
            System.out.println ("DETACH:");
            break;

        case DETACH_OK:
            System.out.println ("DETACH_OK:");
            break;

        case INVALID:
            System.out.println ("INVALID:");
            if (reason != null)
                System.out.printf ("    reason='%s'\n", reason);
            else
                System.out.printf ("    reason=\n");
            break;

        }
    }


    //  --------------------------------------------------------------------------
    //  Get/set the message routing id

    public ZFrame routingId ()
    {
        return routingId;
    }

    public void setRoutingId (ZFrame routingId)
    {
        if (this.routingId != null)
            this.routingId.destroy ();
        this.routingId = routingId.duplicate ();
    }


    //  --------------------------------------------------------------------------
    //  Get/set the zeps_msg id

    public int id ()
    {
        return id;
    }

    public void setId (int id)
    {
        this.id = id;
    }

    //  --------------------------------------------------------------------------
    //  Get/set the stream field

    public String stream ()
    {
        return stream;
    }

    public void setStream (String format, Object ... args)
    {
        //  Format into newly allocated string
        stream = String.format (format, args);
    }

    //  --------------------------------------------------------------------------
    //  Get/set the pattern field

    public String pattern ()
    {
        return pattern;
    }

    public void setPattern (String format, Object ... args)
    {
        //  Format into newly allocated string
        pattern = String.format (format, args);
    }

    //  --------------------------------------------------------------------------
    //  Get/set the latest field

    public long latest ()
    {
        return latest;
    }

    public void setLatest (long latest)
    {
        this.latest = latest;
    }

    //  --------------------------------------------------------------------------
    //  Get/set the credit field

    public long credit ()
    {
        return credit;
    }

    public void setCredit (long credit)
    {
        this.credit = credit;
    }

    //  --------------------------------------------------------------------------
    //  Get/set the key field

    public String key ()
    {
        return key;
    }

    public void setKey (String format, Object ... args)
    {
        //  Format into newly allocated string
        key = String.format (format, args);
    }

    //  --------------------------------------------------------------------------
    //  Get/set the body field

    public byte[] body ()
    {
        return body;
    }

    //  Takes ownership of supplied frame
    public void setBody (byte[] body)
    {
        this.body = body;
    }
    //  --------------------------------------------------------------------------
    //  Get/set the sequence field

    public long sequence ()
    {
        return sequence;
    }

    public void setSequence (long sequence)
    {
        this.sequence = sequence;
    }

    //  --------------------------------------------------------------------------
    //  Get/set the reason field

    public String reason ()
    {
        return reason;
    }

    public void setReason (String format, Object ... args)
    {
        //  Format into newly allocated string
        reason = String.format (format, args);
    }

}

