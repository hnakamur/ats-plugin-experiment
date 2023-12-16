/*
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

/**
 * @file aws_auth_v4.h
 * @brief AWS Auth v4 signing utility.
 * @see aws_auth_v4.cc
 */

#pragma once

#include <algorithm>   /* transform() */
#include <cstddef>     /* size_t */
#include <string>      /* std::string */
#include <string_view> /* std::string_view */
#include <sstream>     /* std::stringstream */
#include <map>         /* std::map */
#include <set>         /* std::set */

#include <ts/ts.h>

using String         = std::string;
using StringSet      = std::set<std::string>;
using StringMap      = std::map<std::string, std::string>;
using HeaderMultiMap = std::multimap<std::string, std::string>;

class HeaderIterator;

class TsInterface
{
public:
  virtual ~TsInterface(){};
  virtual std::string_view getMethod() = 0;
  virtual std::string_view getHost()   = 0;
  virtual std::string_view getPath()   = 0;
  virtual std::string_view getParams() = 0;
  virtual std::string_view getQuery()  = 0;
  virtual HeaderIterator headerBegin() = 0;
  virtual HeaderIterator headerEnd()   = 0;
};

#ifdef AWS_AUTH_V4_UNIT_TEST
#include "unit_tests/test_aws_auth_v4.h"
#else
#include "aws_auth_v4_wrap.h"
#endif

/* S3 auth v4 utility API */

static const String X_AMZ_CONTENT_SHA256 = "x-amz-content-sha256";
static const String X_AMX_DATE           = "x-amz-date";
static const String X_AMZ_SECURITY_TOKEN = "x-amz-security-token";
static const String X_AMZ                = "x-amz-";
static const String CONTENT_TYPE         = "content-type";
static const String HOST                 = "host";

std::string_view trimWhiteSpaces(std::string_view s);

template <typename ContainerType>
void
commaSeparateString(ContainerType &ss, const String &input, bool trim = true, bool lowerCase = true)
{
  std::istringstream istr(input);
  String token;

  while (std::getline(istr, token, ',')) {
    token = trim ? trimWhiteSpaces(token) : token;
    if (lowerCase) {
      std::transform(token.begin(), token.end(), token.begin(), ::tolower);
    }
    ss.insert(ss.end(), token);
  }
}

class AwsAuthV4
{
public:
  AwsAuthV4(TsInterface &api, time_t *now, bool signPayload, std::string_view awsAccessKeyId, std::string_view awsSecretAccessKey,
            std::string_view awsService, const StringSet &includedHeaders, const StringSet &excludedHeaders,
            const StringMap &regionMap);
  const char *getDateTime(size_t *dateTimeLen);
  String getPayloadHash();
  String getAuthorizationHeader();

private:
  TsInterface &_api;
  char _dateTime[sizeof "20170428T010203Z"];
  bool _signPayload = false;
  std::string_view _awsAccessKeyId;
  std::string_view _awsSecretAccessKey;
  std::string_view _awsService;

  const StringSet &_includedHeaders;
  const StringSet &_excludedHeaders;
  const StringMap &_regionMap;
};
