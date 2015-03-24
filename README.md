# ZeroMQ Enterprise Publish-Subscribe - ZEPS

## DEPRECATED

This project is deprecated by zeromq/Malamute.

## Overview

ZEPS is a broker-based publish-and-subscribe design that offers both high-speed low-latency pub-sub, and persistent journalled pub-sub, automatically switching clients from low gear to high gear according to their capabilities. ZEPS is compatible with multicast protocols like NORM and PGM, and has extensible subscription matching.

Note that this is a concept wire-frame. There is no ZEPS broker at this stage. ZEPS is intended to be packaged into the ZeroMQ broker project (zbroker), which is an embeddable broker library that can be wrapped in arbitrary languages for deployment.

## Problem Statement

The built-in ZeroMQ PUB-SUB pattern is tuned for high frequency, low-latency, transient data (the radio broadcast model). This model is extremely scalable, and requires no mediation (brokers). It also maps directly to multicast protocols like PGM and NORM.

However, the PUB-SUB pattern has two scalability features that become anti-features, when scale, volume, and low-latency are not so important:

* The pattern keeps no message state, so subscribers will lose arbitrary numbers of messages when they start-up. This is called the "late joiner syndrome".

* The pattern deals with slow subscribers, or congested networks, by dropping messages. This can lead to gaps that are hard to detect, and very hard to recover from.

It is thus a frequent demand from users: "how does one do reliable publish-subscribe with ZeroMQ?"

ZEPS is an answer to this question.

## Overall Design

ZEPS is a broker-based design. It connects a set of clients, which can both subscribe and publish, independently.

The message flow operates at two levels, real-time and historical:

* During normal operation, messages are routed in real-time to subscribers, held in outgoing socket pipes, and delivered without any processing, disk I/O, or other work except subscription resolution and network I/O.
* Concurrently, all messages are logged to a journal, which is a sequential disk file.
* When a client subscribes, it can request messages from a certain past point (a time period, or a message sequence). The broker will then replay its journal, filtering messages according to the subscription pattern. When it has finished replaying historical data, the broker switches the client to receiving the real-time flow.

In the same way, a client that is congested will stop receiving real-time data and be switched to receiving historical data. These switches between real-time routing and historical replay are invisible to clients.

ZEPS makes some important assumptions:

* *The fanout ratio is significant, at least ten times.* This means, publishers will never be a bottleneck, and a ZEPS broker can always accept new published messages, without confirmation. Publshing is thus a fully asynchronous operation with no possibilty of message loss.
* *The ZEPS broker is utterly reliable, and will not crash.* In reality the hardware will fail at some stage. However the broker can be made simple, and reliable enough, to never fail. This allows it to act as a single point of failure, and take responsibility for message sequencing.

When publishers are faced with network congestion, or a lost connection to the broker, they will either block, or execute other strategies. Messages will not be dropped unless the publisher explicitly does this.

## Protocol Specification

The following ABNF grammar defines the ZEPS protocol:

    ZEPS         = open-stream *use-stream [ close-stream ]

    open-stream  = C:ATTACH ( S:ATTACH-OK / error )

    use-stream   = C:SUBSCRIBE ( S:SUBSCRIBE-OK / error )
                 / C:CREDIT
                 / S:DELIVER
                 / C:PUBLISH
                 / C:PING S:PING-OK

    close-stream = C:DETACH / S:DETACH

    error        = S:INVALID

    ;     Client opens stream
    attach          = signature %d1 protocol version stream
    signature       = %xAA %xA5             ; two octets
    protocol        = string                ; Constant "ZEPS"
    version         = number-2              ; Protocol version 1
    stream          = string                ; Stream name

    ;     Server grants the client access
    attach_ok       = signature %d2

    ;     Client subscribes to a set of topics
    subscribe       = signature %d3 pattern latest
    pattern         = string                ; Subscription pattern
    latest          = number-8              ; Last known sequence

    ;     Server confirms the subscription
    subscribe_ok    = signature %d4

    ;     Client sends credit to the server
    credit          = signature %d5 credit
    credit          = number-8              ; Credit, in bytes

    ;     Client publishes a new message to the open stream
    publish         = signature %d6 key body
    key             = string                ; Message routing key
    body            = chunk                 ; Message contents

    ;     Server delivers a message to the client
    deliver         = signature %d7 sequence key body
    sequence        = number-8              ; Sequenced per stream
    key             = string                ; Message routing key
    body            = chunk                 ; Message contents

    ;     Client or server sends a heartbeat
    ping            = signature %d8

    ;     Client or server answers a heartbeat
    ping_ok         = signature %d9

    ;     Client closes the peering
    detach          = signature %d10

    ;     Server handshake the close
    detach_ok       = signature %d11

    ;     Server tells client it sent an invalid message
    invalid         = signature %d12 reason
    reason          = string                ; Printable explanation

    ; A chunk has 4-octet length + binary contents
    chunk           = number-4 *OCTET

    ; Strings are always length + text contents
    string          = number-1 *VCHAR

    ; Numbers are unsigned integers in network byte order
    number-1        = 1OCTET
    number-2        = 2OCTET
    number-4        = 4OCTET
    number-8        = 8OCTET

## Detailed Design Notes

The broker uses "streams" to segregate and isolate traffic. Each stream corresponds to a single journal file (implementations may do further sharding to improve performance). Within a stream, each message has a "routing key" and a multipart content (currently implemented as a chunk). Messages are sequenced per stream.

ZEPS uses a credit-based flow control scheme to control buffer sizes on the broker. In this model, the client sends credit (in bytes) to the broker, which send messages to the client. These two flows happen asynchronously. When there is no credit, the broker stops sending to the client, and switches it to historical replay. This will happen, for instance, when a client is disconnected or restarted.

Within a stream, clients can subscribe to sub-streams of messages using pattern matching on a message "routing key". ZeroMQ PUB-SUB does prefix matching, using a trie structure. ZEPS does not yet specify a matching style. Some options are:

* prefix matching using a trie
* regular expression matching

More complex matching needs more complex data structures. This is an area we may make extensible, internally.

## Multicast Output

For certain use cases, real-time data can be sent over multicast (using NORM or PGM). Historical data would never be multicast. If clients want both multicast real-time data, and historical data, they would create two connections and reconcile them.

