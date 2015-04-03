# pg_kafka

Version: 0.0.1

**pg_kafka** is a PostgreSQL extension to produce messages to Apache Kafka. When combined with PostgreSQL it 
creates a convenient way to get operations and row data without the limits of using LISTEN/NOTIFY.

**pg_kafka** is released under the MIT license (See LICENSE file).

Shout out to the [pg_amqp extension](https://github.com/omniti-labs/pg_amqp) authors. I used their project as 
a guide to teach myself how to write a PostgreSQL extension.

### Version Compatability
This code is built with the following assumptions.  You may get mixed results if you deviate from these versions.

* [PostgreSQL](http://www.postgresql.org) 9.2+
* [Kafka](http://kafka.apache.org) 0.8.1+
* [librdkafka](https://github.com/edenhill/librdkafka) 0.8.x

### Requirements
* PostgreSQL
* librdkafka
* libsnappy
* zlib

### Building

To build you will need to install PostgreSQL (for pg_config) and PostgreSQL server development packages. On Debian 
based distributions you can usually do something like this:

    apt-get install -y postgresql postgresql-server-dev-9.2
    
You will also need to make sure the librdkafka library and it's header files have been installed. See their Github 
page for further details.

If you have all of the prerequisites installed you should be able to just:

    make && make install

Once the extension has been installed you just need to enable it in postgresql.conf:

    shared_preload_libraries = 'pg_kafka.so'

And restart PostgreSQL.

### Usage
    -- run once
    create extension kafka;
    -- insert broker information
    insert into kafka.broker values ('localhost', 9092);
    -- produce a message
    select kafka.produce('test_topic', 'my message');

For something a bit more useful, consider setting this up on a trigger and producing a message for every INSERT, UPDATE, 
and DELETE that happens on a table.

If the kafka schema wasn't auto-created by installing the extension take a look at sql/kafka.sql.
            
### Support

File bug reports, feature requests and questions using
[GitHub Issues](https://github.com/xstevens/pg_kafka/issues)

### Notes

Before implementing this project I had looked into LISTEN/NOTIFY operations in PostgreSQL. NOTIFY is unfortunately limited 
to 8000 bytes for the total payload size. I also found several mentions in the PostgreSQL mailing lists that NOTIFY was 
never intended to send row data; rather it was intended to get change notifications on keys to clean up external caching, etc. 

So my next approach was trying to read PostgreSQL WAL data by creating a process that acted as a replication 
client. This approach would have a number of advantages compared to being an extension in database (namely you wouldn't 
potentially affect any transactions and you can run outside of the database). I successfully started receiving WAL data, 
but I could not find any material on how to actually decode that data. Fortunately as of Postgres 9.4 they have added Logical Decoding. I have started working on a logical decoding output plugin called [decoderbufs](https://github.com/xstevens/decoderbufs).
