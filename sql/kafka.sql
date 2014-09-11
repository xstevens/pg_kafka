BEGIN;
create schema kafka;

create function kafka.produce(varchar, varchar)
returns boolean as 'pg_kafka.so', 'pg_kafka_produce'
language C immutable;

comment on function kafka.produce(varchar, varchar) is
'Produces a message (topic, message).  The message will only
be produced if the containing PostgreSQL transaction successfully commits.

Produce returns a boolean indicating if the message was sent successfully.  Note that as
Kafka produce is asynchronous, you may find out later it was unsuccessful.';

create function kafka.close() 
returns boolean as 'pg_kafka.so', 'pg_kafka_close'
language C immutable;

comment on function kafka.close() is 
'Closes the broker connections to Kafka.';

create table kafka.broker (
  host text not null,
  port integer not null default 9092,
  primary key(host, port)
);

COMMIT;
