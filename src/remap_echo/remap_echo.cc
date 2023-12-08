/** @file

  Remap echo plugin

  (Apapted from statichit plugin)

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#include <cerrno>
#include <cinttypes>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <fstream>
#include <sstream>

#include <string>
#include <filesystem>
#include <getopt.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "ts/ts.h"
#include "ts/remap.h"

constexpr char PLUGIN[] = "remap_echo";

static DbgCtl dbg_ctl{PLUGIN};

#define VDEBUG(fmt, ...) Dbg(dbg_ctl, fmt, ##__VA_ARGS__)

#if DEBUG
#define VERROR(fmt, ...) Dbg(dbg_ctl, fmt, ##__VA_ARGS__)
#else
#define VERROR(fmt, ...) TSError("[%s] %s: " fmt, PLUGIN, __FUNCTION__, ##__VA_ARGS__)
#endif

#define VIODEBUG(vio, fmt, ...)                                                                                              \
  VDEBUG("vio=%p vio.cont=%p, vio.cont.data=%p, vio.vc=%p " fmt, (vio), TSVIOContGet(vio), TSContDataGet(TSVIOContGet(vio)), \
         TSVIOVConnGet(vio), ##__VA_ARGS__)

static int StatCountBytes     = -1;
static int StatCountResponses = -1;

static int RemapEchoInterceptHook(TSCont contp, TSEvent event, void *edata);
static int RemapEchoTxnHook(TSCont contp, TSEvent event, void *edata);

struct RemapEchoConfig {
  explicit RemapEchoConfig(const std::string &content, const std::string &mimeType, int statusCode)
    : content{content}, mimeType{mimeType}, statusCode{statusCode}
  {
  }

  ~RemapEchoConfig() { TSContDestroy(cont); }

  std::string content;
  std::string mimeType;
  int statusCode;

  TSCont cont = nullptr;
};

struct RemapEchoRequest;

union argument_type {
  void *ptr;
  intptr_t ecode;
  TSVConn vc;
  TSVIO vio;
  TSHttpTxn txn;
  RemapEchoRequest *trq;

  argument_type(void *_p) : ptr(_p) {}
};

// This structure represents the state of a streaming I/O request. It
// is directional (ie. either a read or a write). We need two of these
// for each TSVConn; one to push data into the TSVConn and one to pull
// data out.
struct IOChannel {
  TSVIO vio = nullptr;
  TSIOBuffer iobuf;
  TSIOBufferReader reader;

  IOChannel() : iobuf(TSIOBufferSizedCreate(TS_IOBUFFER_SIZE_INDEX_32K)), reader(TSIOBufferReaderAlloc(iobuf)) {}
  ~IOChannel()
  {
    if (this->reader) {
      TSIOBufferReaderFree(this->reader);
    }

    if (this->iobuf) {
      TSIOBufferDestroy(this->iobuf);
    }
  }

  void
  read(TSVConn vc, TSCont contp)
  {
    this->vio = TSVConnRead(vc, contp, this->iobuf, INT64_MAX);
  }

  void
  write(TSVConn vc, TSCont contp)
  {
    this->vio = TSVConnWrite(vc, contp, this->reader, INT64_MAX);
  }
};

struct RemapEchoHttpHeader {
  TSMBuffer buffer;
  TSMLoc header;
  TSHttpParser parser;

  RemapEchoHttpHeader()
  {
    this->buffer = TSMBufferCreate();
    this->header = TSHttpHdrCreate(this->buffer);
    this->parser = TSHttpParserCreate();
  }

  ~RemapEchoHttpHeader()
  {
    if (this->parser) {
      TSHttpParserDestroy(this->parser);
    }

    TSHttpHdrDestroy(this->buffer, this->header);
    TSHandleMLocRelease(this->buffer, TS_NULL_MLOC, this->header);
    TSMBufferDestroy(this->buffer);
  }
};

struct RemapEchoRequest {
  RemapEchoRequest() {}

  off_t nbytes   = 0; // Number of bytes to generate.
  int statusCode = 200;
  IOChannel readio;
  IOChannel writeio;
  RemapEchoHttpHeader rqheader;

  std::string content;
  std::string mimeType;

  static RemapEchoRequest *
  createRemapEchoRequest(RemapEchoConfig *tc, [[maybe_unused]] TSHttpTxn txn)
  {
    RemapEchoRequest *shr = new RemapEchoRequest;

    shr->statusCode = tc->statusCode;
    shr->content    = tc->content;
    shr->nbytes     = static_cast<off_t>(shr->content.size());
    shr->mimeType   = tc->mimeType;
    return shr;
  }

  ~RemapEchoRequest() = default;
};

// Destroy a RemapEchoRequest, including the per-txn continuation.
static void
RemapEchoRequestDestroy(RemapEchoRequest *trq, TSVIO vio, TSCont contp)
{
  if (vio) {
    TSVConnClose(TSVIOVConnGet(vio));
  }

  TSContDestroy(contp);
  delete trq;
}

// NOTE: This will always append a new "field_name: value"
static void
HeaderFieldDateSet(const RemapEchoHttpHeader &http, const char *field_name, int field_len, time_t value)
{
  TSMLoc field;

  TSMimeHdrFieldCreateNamed(http.buffer, http.header, field_name, field_len, &field);
  TSMimeHdrFieldValueDateSet(http.buffer, http.header, field, value);
  TSMimeHdrFieldAppend(http.buffer, http.header, field);
  TSHandleMLocRelease(http.buffer, http.header, field);
}

// NOTE: This will always append a new "field_name: value"
static void
HeaderFieldIntSet(const RemapEchoHttpHeader &http, const char *field_name, int field_len, int64_t value)
{
  TSMLoc field;

  TSMimeHdrFieldCreateNamed(http.buffer, http.header, field_name, field_len, &field);
  TSMimeHdrFieldValueInt64Set(http.buffer, http.header, field, -1, value);
  TSMimeHdrFieldAppend(http.buffer, http.header, field);
  TSHandleMLocRelease(http.buffer, http.header, field);
}

// NOTE: This will always append a new "field_name: value"
static void
HeaderFieldStringSet(const RemapEchoHttpHeader &http, const char *field_name, int field_len, const char *value)
{
  TSMLoc field;

  TSMimeHdrFieldCreateNamed(http.buffer, http.header, field_name, field_len, &field);
  TSMimeHdrFieldValueStringSet(http.buffer, http.header, field, -1, value, -1);
  TSMimeHdrFieldAppend(http.buffer, http.header, field);
  TSHandleMLocRelease(http.buffer, http.header, field);
}

static TSReturnCode
WriteResponseHeader(RemapEchoRequest *trq, [[maybe_unused]] TSCont contp, TSHttpStatus status)
{
  RemapEchoHttpHeader response;

  VDEBUG("writing response header");

  if (TSHttpHdrTypeSet(response.buffer, response.header, TS_HTTP_TYPE_RESPONSE) != TS_SUCCESS) {
    VERROR("failed to set type");
    return TS_ERROR;
  }
  if (TSHttpHdrVersionSet(response.buffer, response.header, TS_HTTP_VERSION(1, 1)) != TS_SUCCESS) {
    VERROR("failed to set HTTP version");
    return TS_ERROR;
  }
  if (TSHttpHdrStatusSet(response.buffer, response.header, status) != TS_SUCCESS) {
    VERROR("failed to set HTTP status");
    return TS_ERROR;
  }

  TSHttpHdrReasonSet(response.buffer, response.header, TSHttpHdrReasonLookup(status), -1);

  // Set the Content-Length header.
  HeaderFieldIntSet(response, TS_MIME_FIELD_CONTENT_LENGTH, TS_MIME_LEN_CONTENT_LENGTH, trq->nbytes);

  // Set the Cache-Control header.
  HeaderFieldStringSet(response, TS_MIME_FIELD_CACHE_CONTROL, TS_MIME_LEN_CACHE_CONTROL, "no-cache");

  HeaderFieldStringSet(response, TS_MIME_FIELD_CONTENT_TYPE, TS_MIME_LEN_CONTENT_TYPE, trq->mimeType.c_str());

  // Write the header to the IO buffer. Set the VIO bytes so that we can get a WRITE_COMPLETE
  // event when this is done.
  int hdrlen = TSHttpHdrLengthGet(response.buffer, response.header);

  TSHttpHdrPrint(response.buffer, response.header, trq->writeio.iobuf);
  TSVIONBytesSet(trq->writeio.vio, hdrlen);
  TSVIOReenable(trq->writeio.vio);

  TSStatIntIncrement(StatCountBytes, hdrlen);

  return TS_SUCCESS;
}

// Handle events from TSHttpTxnServerIntercept. The intercept
// starts with TS_EVENT_NET_ACCEPT, and then continues with
// TSVConn events.
static int
RemapEchoInterceptHook(TSCont contp, TSEvent event, void *edata)
{
  VDEBUG("RemapEchoInterceptHook: %p ", edata);

  argument_type arg(edata);

  VDEBUG("contp=%p, event=%s (%d), edata=%p", contp, TSHttpEventNameLookup(event), event, arg.ptr);

  switch (event) {
  case TS_EVENT_NET_ACCEPT: {
    // TS_EVENT_NET_ACCEPT will be delivered when the server intercept
    // is set up by the core. We just need to allocate a RemapEcho
    // request state and start reading the VC.
    RemapEchoRequest *trq = static_cast<RemapEchoRequest *>(TSContDataGet(contp));

    TSStatIntIncrement(StatCountResponses, 1);
    VDEBUG("allocated server intercept RemapEcho trq=%p", trq);

    // This continuation was allocated in RemapEchoTxnHook. Reset the
    // data to keep track of this generator request.
    TSContDataSet(contp, trq);

    // Start reading the request from the server intercept VC.
    trq->readio.read(arg.vc, contp);
    VIODEBUG(trq->readio.vio, "started reading RemapEcho request");

    return TS_EVENT_NONE;
  }

  case TS_EVENT_NET_ACCEPT_FAILED: {
    // TS_EVENT_NET_ACCEPT_FAILED will be delivered if the
    // transaction is cancelled before we start tunnelling
    // through the server intercept. One way that this can happen
    // is if the intercept is attached early, and then we serve
    // the document out of cache.

    // There's nothing to do here except nuke the continuation
    // that was allocated in RemapEchoTxnHook().

    RemapEchoRequest *trq = static_cast<RemapEchoRequest *>(TSContDataGet(contp));
    delete trq;

    TSContDestroy(contp);
    return TS_EVENT_NONE;
  }

  case TS_EVENT_VCONN_READ_READY: {
    argument_type cdata           = TSContDataGet(contp);
    RemapEchoHttpHeader &rqheader = cdata.trq->rqheader;

    VDEBUG("reading vio=%p vc=%p, trq=%p", arg.vio, TSVIOVConnGet(arg.vio), cdata.trq);

    TSIOBufferBlock blk;
    TSParseResult result = TS_PARSE_CONT;

    for (blk = TSIOBufferReaderStart(cdata.trq->readio.reader); blk; blk = TSIOBufferBlockNext(blk)) {
      const char *ptr;
      const char *end;
      int64_t nbytes;
      TSHttpStatus status = static_cast<TSHttpStatus>(cdata.trq->statusCode);

      ptr = TSIOBufferBlockReadStart(blk, cdata.trq->readio.reader, &nbytes);
      if (ptr == nullptr || nbytes == 0) {
        continue;
      }

      end    = ptr + nbytes;
      result = TSHttpHdrParseReq(rqheader.parser, rqheader.buffer, rqheader.header, &ptr, end);
      switch (result) {
      case TS_PARSE_ERROR:
        // If we got a bad request, just shut it down.
        VDEBUG("bad request on trq=%p, sending an error", cdata.trq);
        RemapEchoRequestDestroy(cdata.trq, arg.vio, contp);
        return TS_EVENT_ERROR;

      case TS_PARSE_DONE:
        // Start the vconn write.
        cdata.trq->writeio.write(TSVIOVConnGet(arg.vio), contp);
        TSVIONBytesSet(cdata.trq->writeio.vio, 0);

        if (WriteResponseHeader(cdata.trq, contp, status) != TS_SUCCESS) {
          VERROR("failure writing response");
          return TS_EVENT_ERROR;
        }

        return TS_EVENT_NONE;

      case TS_PARSE_CONT:
        break;
      }
    }

    TSReleaseAssert(result == TS_PARSE_CONT);

    // Reenable the read VIO to get more events.
    TSVIOReenable(arg.vio);
    return TS_EVENT_NONE;
  }

  case TS_EVENT_VCONN_WRITE_READY: {
    argument_type cdata = TSContDataGet(contp);

    if (cdata.trq->nbytes) {
      int64_t nbytes = cdata.trq->nbytes;

      VIODEBUG(arg.vio, "writing %" PRId64 " bytes for trq=%p", nbytes, cdata.trq);
      nbytes = TSIOBufferWrite(cdata.trq->writeio.iobuf, cdata.trq->content.c_str(), nbytes);

      cdata.trq->nbytes -= nbytes;
      TSStatIntIncrement(StatCountBytes, nbytes);

      // Update the number of bytes to write.
      TSVIONBytesSet(arg.vio, TSVIONBytesGet(arg.vio) + nbytes);
      TSVIOReenable(arg.vio);
    }

    return TS_EVENT_NONE;
  }

  case TS_EVENT_ERROR:
  case TS_EVENT_VCONN_EOS: {
    argument_type cdata = TSContDataGet(contp);

    VIODEBUG(arg.vio, "received EOS or ERROR for trq=%p", cdata.trq);
    RemapEchoRequestDestroy(cdata.trq, arg.vio, contp);
    return event == TS_EVENT_ERROR ? TS_EVENT_ERROR : TS_EVENT_NONE;
  }

  case TS_EVENT_VCONN_READ_COMPLETE:
    // We read data forever, so we should never get a READ_COMPLETE.
    VIODEBUG(arg.vio, "unexpected TS_EVENT_VCONN_READ_COMPLETE");
    return TS_EVENT_NONE;

  case TS_EVENT_VCONN_WRITE_COMPLETE: {
    argument_type cdata = TSContDataGet(contp);

    // If we still have bytes to write, kick off a new write operation, otherwise
    // we are done and we can shut down the VC.
    if (cdata.trq->nbytes) {
      cdata.trq->writeio.write(TSVIOVConnGet(arg.vio), contp);
      TSVIONBytesSet(cdata.trq->writeio.vio, cdata.trq->nbytes);
    } else {
      VIODEBUG(arg.vio, "TS_EVENT_VCONN_WRITE_COMPLETE %" PRId64 " todo", TSVIONTodoGet(arg.vio));
      RemapEchoRequestDestroy(cdata.trq, arg.vio, contp);
    }

    return TS_EVENT_NONE;
  }

  case TS_EVENT_TIMEOUT: {
    return TS_EVENT_NONE;
  }

  case TS_EVENT_VCONN_INACTIVITY_TIMEOUT:
    VERROR("unexpected event %s (%d) edata=%p", TSHttpEventNameLookup(event), event, arg.ptr);
    return TS_EVENT_ERROR;

  default:
    VERROR("unexpected event %s (%d) edata=%p", TSHttpEventNameLookup(event), event, arg.ptr);
    return TS_EVENT_ERROR;
  }
}

static void
RemapEchoSetupIntercept(RemapEchoConfig *cfg, TSHttpTxn txn)
{
  RemapEchoRequest *req = RemapEchoRequest::createRemapEchoRequest(cfg, txn);

  if (req == nullptr) {
    return;
  }

  TSCont cnt = TSContCreate(RemapEchoInterceptHook, TSMutexCreate());
  TSContDataSet(cnt, req);

  TSHttpTxnServerIntercept(cnt, txn);

  return;
}

// Handle events that occur on the TSHttpTxn.
static int
RemapEchoTxnHook(TSCont contp, TSEvent event, void *edata)
{
  argument_type arg(edata);

  VDEBUG("contp=%p, event=%s (%d), edata=%p", contp, TSHttpEventNameLookup(event), event, edata);

  switch (event) {
  case TS_EVENT_HTTP_CACHE_LOOKUP_COMPLETE: {
    int method_length, status;
    TSMBuffer bufp;
    TSMLoc hdr_loc;
    const char *method;

    if (TSHttpTxnCacheLookupStatusGet(arg.txn, &status) != TS_SUCCESS) {
      VERROR("failed to get client request handle");
      goto done;
    }

    if (TSHttpTxnClientReqGet(arg.txn, &bufp, &hdr_loc) != TS_SUCCESS) {
      VERROR("Couldn't retrieve client request header");
      goto done;
    }

    method = TSHttpHdrMethodGet(bufp, hdr_loc, &method_length);
    if (nullptr == method) {
      VERROR("Couldn't retrieve client request method");
      goto done;
    }

    if (status != TS_CACHE_LOOKUP_HIT_FRESH || method != TS_HTTP_METHOD_GET) {
      RemapEchoSetupIntercept(static_cast<RemapEchoConfig *>(TSContDataGet(contp)), arg.txn);
    }

    break;
  }

  default:
    VERROR("unexpected event %s (%d)", TSHttpEventNameLookup(event), event);
    break;
  }

done:
  TSHttpTxnReenable(arg.txn, TS_EVENT_HTTP_CONTINUE);
  return TS_EVENT_NONE;
}

TSReturnCode
TSRemapInit([[maybe_unused]] TSRemapInterface *api_info, [[maybe_unused]] char *errbuf, [[maybe_unused]] int errbuf_size)
{
  if (TSStatFindName("RemapEcho.response_bytes", &StatCountBytes) == TS_ERROR) {
    StatCountBytes = TSStatCreate("RemapEcho.response_bytes", TS_RECORDDATATYPE_COUNTER, TS_STAT_NON_PERSISTENT, TS_STAT_SYNC_SUM);
  }

  if (TSStatFindName("RemapEcho.response_count", &StatCountResponses) == TS_ERROR) {
    StatCountResponses =
      TSStatCreate("RemapEcho.response_count", TS_RECORDDATATYPE_COUNTER, TS_STAT_NON_PERSISTENT, TS_STAT_SYNC_COUNT);
  }
  return TS_SUCCESS;
}

TSRemapStatus
TSRemapDoRemap(void *ih, TSHttpTxn rh, [[maybe_unused]] TSRemapRequestInfo *rri)
{
  const TSHttpStatus txnstat = TSHttpTxnStatusGet(rh);
  if (txnstat != TS_HTTP_STATUS_NONE && txnstat != TS_HTTP_STATUS_OK) {
    VDEBUG("transaction status_code=%d already set; skipping processing", static_cast<int>(txnstat));
    return TSREMAP_NO_REMAP;
  }

  RemapEchoConfig *cfg = static_cast<RemapEchoConfig *>(ih);

  if (!cfg) {
    VERROR("No remap context available, check code / config");
    TSHttpTxnStatusSet(rh, TS_HTTP_STATUS_INTERNAL_SERVER_ERROR);
    return TSREMAP_NO_REMAP;
  }

  TSHttpTxnConfigIntSet(rh, TS_CONFIG_HTTP_CACHE_HTTP, 0);
  RemapEchoSetupIntercept(static_cast<RemapEchoConfig *>(ih), rh);

  return TSREMAP_NO_REMAP; // This plugin never rewrites anything.
}

TSReturnCode
TSRemapNewInstance(int argc, char *argv[], void **ih, [[maybe_unused]] char *errbuf, [[maybe_unused]] int errbuf_size)
{
  static const struct option longopt[] = {
    {"content",     required_argument, nullptr, 'c' },
    {"mime-type",   required_argument, nullptr, 'm' },
    {"status-code", required_argument, nullptr, 's' },
    {nullptr,       no_argument,       nullptr, '\0'}
  };

  std::string content;
  std::string mimeType = "text/plain";
  int statusCode       = 0;

  // argv contains the "to" and "from" URLs. Skip the first so that the
  // second one poses as the program name.
  --argc;
  ++argv;
  optind = 0;

  while (true) {
    int opt = getopt_long(argc, (char *const *)argv, "c:m:s", longopt, nullptr);

    switch (opt) {
    case 'c': {
      content = std::string(optarg);
    } break;
    case 'm': {
      mimeType = std::string(optarg);
    } break;
    case 's': {
      statusCode = atoi(optarg);
    } break;
    }

    if (opt == -1) {
      break;
    }
  }

  if (content.size() == 0) {
    VERROR("Need to specify --content\n");
    return TS_ERROR;
  }

  RemapEchoConfig *tc = new RemapEchoConfig(content, mimeType, statusCode);

  // Finally, create the continuation to use for this remap rule, tracking the config as cont data.
  tc->cont = TSContCreate(RemapEchoTxnHook, nullptr);
  TSContDataSet(tc->cont, tc);

  *ih = static_cast<void *>(tc);

  return TS_SUCCESS;
}

void
TSRemapDeleteInstance(void *ih)
{
  RemapEchoConfig *tc = static_cast<RemapEchoConfig *>(ih);
  delete tc;
}
