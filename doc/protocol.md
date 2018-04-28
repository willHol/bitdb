# BitDB control protocol (Version 1)

## Scope

This document describes a communication protocol that is used to
by programs to communicate with a locally running BitDB process.
This document is heavily influenced by the tor control protocol,
and to a lesser degree Redis:

https://gitweb.torproject.org/torspec.git/tree/control-spec.txt
https://redis.io/topics/protocol

## Message format

The message formats shown below use ABNF as described in RFC 2234.

We define the following general-use nonterminals from RFC 2822:

	QuotedString = DQUOTE *qcontent DQUOTE

There are no limits on line length and all 8-bit characters are
permissable. In QuotedStrings, backslashes and quotes must be
escaped.

Lines should always be terminated with CRLF.

## Commands from controller to BitDB

	Command = Keyword OptArguments CRLF 
	Keyword = 1*ALPHA
	OptArguments = [ SP *(SP / VCHAR) ]	

A command is a single line containing a Keyword and zero or more
OptArguments seperated by spaces.

## Replies from BitDB to the controller

	Reply = Status StatusCode [ SP (DataSize / ErrorMessage) ] SP CRLF
	Status = "+"/"-"
	StatusCode = 1*ALPHA
	DataSize = 1*HEXDIG
	ErrorMessage = 1*ALPHA

A reply is a single line containing a Status symbol followed by
a StatusCode, consisting of one or more digits, and an optional
DataSize.

A status code can be either "+" or "-": the former indicates a
successful reply, while the latter indicates an unsuccessful reply.

Immediately following a successful response which includes a DataSize
attribute, BitDB will send an additional response which contains
DataSize many bytes. If BitDB transmits greater or fewer bytes this
should be treated as an error.

The following is an example reply:

	+OK 32 \r\n

After which BitDB would transmit 32 bytes of data.

## General-use tokens

	Key = 1*ALPHA
	Value = 1*HEXDIG

# Commands

All commands are case-insensitive. 

## PUT

Add a single key-value pair to the database. The syntax is:

	"PUT" SP Key [ SP Value ] CRLF

If Value is not sent by the controller BitDB will wait for the configured
timeout for the controller to send some binary data to BitDB. If the
timer expires the command is ignored and BitDB will begin accepting
commands again.

If Value is sent by the controller BitDB will return a response
immediately.

Response:

	+OK 32 \r\n

## GET

Retrieves a single value from the database. The syntax is:

	"GET" SP Key CRLF

If an error occurs the controller should not expect a binary response

If successful (key exists):

	+OK 32 \r\n
	32-byte binary response

If unsuccessful:

	-ERR message \r\n


