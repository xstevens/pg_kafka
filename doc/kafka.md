kafka 0.0.1
==========

Synopsis
--------

    % CREATE EXTENSION kafka;
    CREATE EXTENSION

    % SELECT kafka.produce('topic', 'message');

Description
-----------

The pg_kafka package provides the ability for PostgreSQL statements to directly
send messages to an [Apache Kafka](https://kafka.apache.org/) broker.

Usage
-----
Insert Kafka broker information (host/port) into the
`kafka.broker` table.

A process starts and connects to PostgreSQL and runs:

    SELECT kafka.produce('topic', 'message');

Upon process termination broker connections will be torn down.
If there is a need to disconnect from a specific broker, one can call:

    SELECT kafka.close();

which will close the connection to the broker if it is connected and do nothing
if it is already disconnected.

Support
-------

This library is stored in an open [GitHub 
repository](https://github.com/xstevens/pg_kafka). Feel free to fork and 
contribute! Please file bug reports via [GitHub
Issues](https://github.com/omniti-labs/pg_kafka/issues/).

Author
------

[Xavier Stevens](https://github.com/xstevens)
