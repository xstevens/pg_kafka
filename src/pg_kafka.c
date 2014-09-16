/*
 * pg_kafka - PostgreSQL extension to produce messages to Apache Kafka
 *
 * Copyright (c) 2014 Xavier Stevens
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <syslog.h>
#include <sys/time.h>
#include <errno.h>

#include "postgres.h"
#include "fmgr.h"
#include "executor/spi.h"
#include "access/xact.h"
#include "utils/builtins.h"
#include "librdkafka/rdkafka.h"

PG_MODULE_MAGIC;

void _PG_init(void);
Datum pg_kafka_produce(PG_FUNCTION_ARGS);
Datum pg_kafka_close(PG_FUNCTION_ARGS);

/**
 * Message delivery report callback.
 * Called once for each message.
 * See rkafka.h for more information.
 */
static void rk_msg_delivered(rd_kafka_t *rk, void *payload, size_t len,
                             int error_code, void *opaque, void *msg_opaque) {
  if (error_code)
    elog(WARNING, "%% Message delivery failed: %s\n",
         rd_kafka_err2str(error_code));
  fprintf(stderr, "Messsage delivery success.\n");
}

/**
 * Kafka logger callback
 */
static void rk_logger(const rd_kafka_t *rk, int level, const char *fac,
                      const char *buf) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  fprintf(stderr, "%u.%03u RDKAFKA-%i-%s: %s: %s\n", (int)tv.tv_sec,
          (int)(tv.tv_usec / 1000), level, fac, rd_kafka_name(rk), buf);
}

static rd_kafka_t *GRK = NULL;

static rd_kafka_t *get_rk() {
  if (GRK) {
    return GRK;
  }

  rd_kafka_t *rk;
  char errstr[512];
  char *brokers;
  char *sql = "select string_agg(host || ':' || port, ',') from kafka.broker";
  if (SPI_connect() == SPI_ERROR_CONNECT)
    return NULL;
  if (SPI_OK_SELECT == SPI_execute(sql, true, 100) && SPI_processed > 0) {
    brokers = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
  } else {
    brokers = "localhost:9092";
  }
  SPI_finish();

  /* kafka configuration */
  rd_kafka_conf_t *conf = rd_kafka_conf_new();
  if (rd_kafka_conf_set(conf, "compression.codec", "snappy", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    elog(WARNING, "%s\n", errstr);
  }

  /* set message delivery callback */
  rd_kafka_conf_set_dr_cb(conf, rk_msg_delivered);

  /* get producer handle to kafka */
  if (!(rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr)))) {
    elog(WARNING, "%% Failed to create new producer: %s\n", errstr);
    goto broken;
  }

  /* set logger */
  rd_kafka_set_logger(rk, rk_logger);
  rd_kafka_set_log_level(rk, LOG_INFO);

  /* add brokers */
  if (rd_kafka_brokers_add(rk, brokers) == 0) {
    elog(WARNING, "%% No valid brokers specified\n");
    goto broken;
  }

  GRK = rk;
  return rk;
broken:
  rd_kafka_destroy(rk);
  return NULL;
}

static void rk_destroy() {
  rd_kafka_t *rk = get_rk();
  rd_kafka_destroy(rk);
  GRK = NULL;
}

static void pg_xact_callback(XactEvent event, void *arg) {
  switch (event) {
    case XACT_EVENT_COMMIT:
      break;
    case XACT_EVENT_ABORT:
      rk_destroy();
      break;
    case XACT_EVENT_PREPARE:
      /* nothin' */
      break;
    case XACT_EVENT_PRE_COMMIT:
      /* nothin' */
      break;
    case XACT_EVENT_PRE_PREPARE:
      /* nothin' */
      break;
  }
}

void _PG_init() { RegisterXactCallback(pg_xact_callback, NULL); }

PG_FUNCTION_INFO_V1(pg_kafka_produce);
Datum pg_kafka_produce(PG_FUNCTION_ARGS) {
  if (!PG_ARGISNULL(0) && !PG_ARGISNULL(1)) {
    /* get topic arg */
	text *topic_txt = PG_GETARG_TEXT_PP(0);
	char *topic = text_to_cstring(topic_txt);
    /* get msg arg */
	text *msg_txt = PG_GETARG_TEXT_PP(1);
	void *msg = VARDATA_ANY(msg_txt);
	size_t msg_len = VARSIZE_ANY_EXHDR(msg_txt);
    /* create topic */
    rd_kafka_topic_conf_t *topic_conf = rd_kafka_topic_conf_new();
    rd_kafka_t *rk = get_rk();
    if (!rk) {
      PG_RETURN_BOOL(0 != 0);
    }
    rd_kafka_topic_t *rkt = rd_kafka_topic_new(rk, topic, topic_conf);

    /* using random partition for now */
    int partition = RD_KAFKA_PARTITION_UA;

    /* send/produce message. */
    int rv = rd_kafka_produce(rkt, partition, RD_KAFKA_MSG_F_COPY, msg,
    						  msg_len, NULL, 0, NULL);
    if (rv == -1) {
      fprintf(stderr, "%% Failed to produce to topic %s partition %i: %s\n",
              rd_kafka_topic_name(rkt), partition,
              rd_kafka_err2str(rd_kafka_errno2err(errno)));
      /* poll to handle delivery reports */
      rd_kafka_poll(rk, 0);
    }

    /* destroy kafka topic */
    rd_kafka_topic_destroy(rkt);
    pfree(topic);

    PG_RETURN_BOOL(rv == 0);
  }
  PG_RETURN_BOOL(0 != 0);
}

PG_FUNCTION_INFO_V1(pg_kafka_close);
Datum pg_kafka_close(PG_FUNCTION_ARGS) {
  rk_destroy();
  PG_RETURN_VOID();
}
