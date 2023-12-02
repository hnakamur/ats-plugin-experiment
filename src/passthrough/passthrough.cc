/** @file

  Passthrough remap plugin

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

#include <filesystem>
#include <getopt.h>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "ts/remap.h"
#include "ts/ts.h"

constexpr char PLUGIN[] = "passthrough";

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

TSReturnCode
TSRemapInit([[maybe_unused]] TSRemapInterface *api_info, [[maybe_unused]] char *errbuf, [[maybe_unused]] int errbuf_size)
{
  Dbg(dbg_ctl, "enter");
  return TS_SUCCESS;
}

TSRemapStatus
TSRemapDoRemap(void *ih, TSHttpTxn rh, TSRemapRequestInfo *rri)
{
  const TSHttpStatus txnstat = TSHttpTxnStatusGet(rh);
  if (txnstat != TS_HTTP_STATUS_NONE && txnstat != TS_HTTP_STATUS_OK) {
    VDEBUG("transaction status_code=%d already set; skipping processing", static_cast<int>(txnstat));
    return TSREMAP_NO_REMAP;
  }

  // Disable cache lookup
  VDEBUG("disable cache lookup");
  TSHttpTxnConfigIntSet(rh, TS_CONFIG_HTTP_CACHE_HTTP, 0);

  return TSREMAP_NO_REMAP;
}

TSReturnCode
TSRemapNewInstance([[maybe_unused]] int argc, [[maybe_unused]] char *argv[], [[maybe_unused]] void **ih,
                   [[maybe_unused]] char *errbuf, [[maybe_unused]] int errbuf_size)
{
  Dbg(dbg_ctl, "enter");
  return TS_SUCCESS;
}

void
TSRemapDeleteInstance([[maybe_unused]] void *ih)
{
  Dbg(dbg_ctl, "enter");
}
