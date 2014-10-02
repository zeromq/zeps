package org.zproto;

import static org.junit.Assert.*;
import org.junit.Test;
import org.zeromq.ZMQ;
import org.zeromq.ZMQ.Socket;
import org.zeromq.ZFrame;
import org.zeromq.ZContext;

public class TestZepsMsg
{
    @Test
    public void testZepsMsg ()
    {
        System.out.printf (" * zeps_msg: ");

        //  Simple create/destroy test
        ZepsMsg self = new ZepsMsg (0);
        assert (self != null);
        self.destroy ();

        //  Create pair of sockets we can send through
        ZContext ctx = new ZContext ();
        assert (ctx != null);

        Socket output = ctx.createSocket (ZMQ.DEALER);
        assert (output != null);
        output.bind ("inproc://selftest");
        Socket input = ctx.createSocket (ZMQ.ROUTER);
        assert (input != null);
        input.connect ("inproc://selftest");

        //  Encode/send/decode and verify each message type

        self = new ZepsMsg (ZepsMsg.ATTACH);
        self.setStream ("Life is short but Now lasts for ever");
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        assertEquals (self.stream (), "Life is short but Now lasts for ever");
        self.destroy ();

        self = new ZepsMsg (ZepsMsg.ATTACH_OK);
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new ZepsMsg (ZepsMsg.SUBSCRIBE);
        self.setPattern ("Life is short but Now lasts for ever");
        self.setLatest ((byte) 123);
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        assertEquals (self.pattern (), "Life is short but Now lasts for ever");
        assertEquals (self.latest (), 123);
        self.destroy ();

        self = new ZepsMsg (ZepsMsg.SUBSCRIBE_OK);
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new ZepsMsg (ZepsMsg.CREDIT);
        self.setCredit ((byte) 123);
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        assertEquals (self.credit (), 123);
        self.destroy ();

        self = new ZepsMsg (ZepsMsg.PUBLISH);
        self.setKey ("Life is short but Now lasts for ever");
        self.setBody ("Captcha Diem".getBytes());
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        assertEquals (self.key (), "Life is short but Now lasts for ever");
        assertTrue (java.util.Arrays.equals("Captcha Diem".getBytes(), self.body ()));
        self.destroy ();

        self = new ZepsMsg (ZepsMsg.DELIVER);
        self.setSequence ((byte) 123);
        self.setKey ("Life is short but Now lasts for ever");
        self.setBody ("Captcha Diem".getBytes());
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        assertEquals (self.sequence (), 123);
        assertEquals (self.key (), "Life is short but Now lasts for ever");
        assertTrue (java.util.Arrays.equals("Captcha Diem".getBytes(), self.body ()));
        self.destroy ();

        self = new ZepsMsg (ZepsMsg.PING);
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new ZepsMsg (ZepsMsg.PING_OK);
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new ZepsMsg (ZepsMsg.DETACH);
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new ZepsMsg (ZepsMsg.DETACH_OK);
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new ZepsMsg (ZepsMsg.INVALID);
        self.setReason ("Life is short but Now lasts for ever");
        self.send (output);

        self = ZepsMsg.recv (input);
        assert (self != null);
        assertEquals (self.reason (), "Life is short but Now lasts for ever");
        self.destroy ();

        ctx.destroy ();
        System.out.printf ("OK\n");
    }
}
