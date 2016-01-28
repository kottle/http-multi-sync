/* -*- indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */

/* This code is PUBLIC DOMAIN, and is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND. See the accompanying
 * LICENSE file.
 */

#include <nan.h>
#include <curl/curl.h>
#include <string>
#include <string.h>
#include <vector>
#include <stdint.h>
#include <iostream>

using namespace node;
using namespace v8;

#define THROW_BAD_ARGS \
  NanThrowTypeError("Bad argument")

#define PERSISTENT_STRING(id, text) \
  NanAssignPersistent<String>(id, NanNew<String>(text))

typedef std::vector<char> buff_t;

class CurlLib : ObjectWrap {
private:
  static std::string buffer;
  static std::vector<std::string> headers;
  static Persistent<String> sym_body_length;
  static Persistent<String> sym_headers;
  static Persistent<String> sym_timedout;
  static Persistent<String> sym_error;

public:
  static Persistent<Function> s_constructor;
  static void Init(Handle<Object> target) {
    Local<FunctionTemplate> t = NanNew<FunctionTemplate>(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(NanNew<String>("CurlLib"));

    NODE_SET_PROTOTYPE_METHOD(t, "run", Run);
    NODE_SET_PROTOTYPE_METHOD(t, "body", Body);

    NanAssignPersistent<Function>(s_constructor, t->GetFunction());
    target->Set(NanNew<String>("CurlLib"),
                t->GetFunction());

    PERSISTENT_STRING(sym_body_length, "body_length");
    PERSISTENT_STRING(sym_headers, "headers");
    PERSISTENT_STRING(sym_timedout, "timedout");
    PERSISTENT_STRING(sym_error, "error");
  }

  CurlLib() { }
  ~CurlLib() { }

  static NAN_METHOD(New) {
    NanScope();
    CurlLib* curllib = new CurlLib();
    curllib->Wrap(args.This());
    NanReturnValue(args.This());
  }

  static size_t write_data(void *ptr, size_t size,
			   size_t nmemb, void *userdata) {
    buffer.append(static_cast<char*>(ptr), size * nmemb);
    // std::cerr<<"Wrote: "<<size*nmemb<<" bytes"<<std::endl;
    // std::cerr<<"Buffer size: "<<buffer.size()<<" bytes"<<std::endl;
    return size * nmemb;
  }

  static size_t write_headers(void *ptr, size_t size, size_t nmemb, void *userdata)
  {
    std::string header(static_cast<char*>(ptr), size * nmemb);
    headers.push_back(header);
    return size * nmemb;
  }

  static NAN_METHOD(Body) {
    NanScope();

    if (args.Length() < 1 || !Buffer::HasInstance(args[0])) {
      return THROW_BAD_ARGS;
    }

    Local<Object> buffer_obj = args[0]->ToObject();
    char *buffer_data        = Buffer::Data(buffer_obj);
    size_t buffer_length     = Buffer::Length(buffer_obj);

    if (buffer_length < buffer.size()) {
      return NanThrowTypeError("Insufficient Buffer Length");
    }

    if (!buffer.empty()) {
      memcpy(buffer_data, buffer.data(), buffer.size());
    }
    buffer.clear();
    NanReturnValue(buffer_obj);
  }

  static NAN_METHOD(Run) {
    NanScope();

    if (args.Length() < 1) {
      return THROW_BAD_ARGS;
    }

    Local<String> key_url = NanNew<String>("url");
    Local<String> key_headers = NanNew<String>("headers");
    Local<String> key_body = NanNew<String>("body");
    Local<String> key_connect_timeout_ms = NanNew<String>("connect_timeout_ms");
    Local<String> key_timeout_ms = NanNew<String>("timeout_ms");
    Local<String> key_rejectUnauthorized = NanNew<String>("rejectUnauthorized");
    Local<String> key_caCert = NanNew<String>("ca");
    Local<String> key_clientCert = NanNew<String>("cert");
    Local<String> key_pfx = NanNew<String>("pfx");
    Local<String> key_clientKey = NanNew<String>("key");
    Local<String> key_clientKeyPhrase = NanNew<String>("passphrase");
    Local<String> key_copyname = NanNew<String>("copyname");
    Local<String> key_file = NanNew<String>("file");

    static const Local<String> PFXFORMAT = NanNew<String>("P12");

    Local<Array> opt = Local<Array>::Cast(args[0]);

    if (!opt->Has(key_url) ||
        !opt->Has(key_headers)) {
      return THROW_BAD_ARGS;
    }

    if (!opt->Get(key_url)->IsString()) {
      return THROW_BAD_ARGS;
    }

    Local<String> url      = Local<String>::Cast(opt->Get(key_url));
    Local<Array>  reqh     = Local<Array>::Cast(opt->Get(key_headers));
    Local<String> copyname = Local<String>::Cast(opt->Get(key_copyname));
    Local<String> file     = Local<String>::Cast(opt->Get(key_file));
    Local<String> body     = NanNew<String>((const char*)"", 0);
    Local<String> caCert   = NanNew<String>((const char*)"", 0);
    Local<String> clientCert       = NanNew<String>((const char*)"", 0);
    Local<String> clientCertFormat = NanNew<String>((const char*)"", 0);
    Local<String> clientKey        = NanNew<String>((const char*)"", 0);
    Local<String> clientKeyPhrase   = NanNew<String>((const char*)"", 0);

    long connect_timeout_ms = 1 * 60 * 60 * 1000; /* 1 hr in msec */
    long timeout_ms = 1 * 60 * 60 * 1000; /* 1 hr in msec */
    bool rejectUnauthorized = false;

    if (opt->Has(key_caCert) && opt->Get(key_caCert)->IsString()) {
      caCert = opt->Get(key_caCert)->ToString();
    }

    if (opt->Has(key_clientKey) && opt->Get(key_clientKey)->IsString()) {
      clientKey = opt->Get(key_clientKey)->ToString();
    }

    if (opt->Has(key_clientKeyPhrase) && opt->Get(key_clientKeyPhrase)->IsString()) {
      clientKeyPhrase = opt->Get(key_clientKeyPhrase)->ToString();
    }

    if (opt->Has(key_clientCert) && opt->Get(key_clientCert)->IsString()) {
      clientCert = opt->Get(key_clientCert)->ToString();
    } else if (opt->Has(key_pfx) && opt->Get(key_pfx)->IsString()) {
      clientCert = opt->Get(key_pfx)->ToString();
      clientCertFormat = PFXFORMAT;
    }

    if (opt->Has(key_body) && opt->Get(key_body)->IsString()) {
      body = opt->Get(key_body)->ToString();
    }

    if (opt->Has(key_connect_timeout_ms) && opt->Get(key_connect_timeout_ms)->IsNumber()) {
      connect_timeout_ms = opt->Get(key_connect_timeout_ms)->IntegerValue();
    }

    if (opt->Has(key_timeout_ms) && opt->Get(key_timeout_ms)->IsNumber()) {
      timeout_ms = opt->Get(key_timeout_ms)->IntegerValue();
    }

    if (opt->Has(key_rejectUnauthorized)) {
      // std::cerr<<"has reject unauth"<<std::endl;
      if (opt->Get(key_rejectUnauthorized)->IsBoolean()) {
        rejectUnauthorized = opt->Get(key_rejectUnauthorized)->BooleanValue();
      } else if (opt->Get(key_rejectUnauthorized)->IsBooleanObject()) {
        rejectUnauthorized = opt->Get(key_rejectUnauthorized)
          ->ToBoolean()
          ->BooleanValue();
      }
    }

    // std::cerr<<"rejectUnauthorized: " << rejectUnauthorized << std::endl;

    NanUtf8String _body(body);
    NanUtf8String _copyname(copyname);
    NanUtf8String _file(file);
    NanUtf8String _url(url);
    NanUtf8String _cacert(caCert);
    NanUtf8String _clientcert(clientCert);
    NanUtf8String _clientcertformat(clientCertFormat);
    NanUtf8String _clientkeyphrase(clientKeyPhrase);
    NanUtf8String _clientkey(clientKey);

    std::vector<std::string> _reqh;
    for (size_t i = 0; i < reqh->Length(); ++i) {
      _reqh.push_back(*NanUtf8String(reqh->Get(i)));
    }

    // CurlLib* curllib = ObjectWrap::Unwrap<CurlLib>(args.This());

    buffer.clear();
    headers.clear();

    struct curl_httppost *formpost=NULL;
    struct curl_httppost *lastptr=NULL;
    struct curl_slist *headerlist=NULL;
    static const char buf[] = "Expect:";

    CURL *curl;
    CURLM *multi_handle;
    int still_running;
    CURLMcode res = CURLM_INTERNAL_ERROR;
    // char error_buffer[CURL_ERROR_SIZE];
    // error_buffer[0] = '\0';

    curl = curl_easy_init();
    if (curl) {
      // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      // curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

      curl_formadd(&formpost, &lastptr,
        CURLFORM_COPYNAME, "file",
        CURLFORM_FILE, "/Users/eggzero/Projects/EGGZERO/github/http-multi-sync/test.js",
        CURLFORM_END);
      curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);    

      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
      curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5);
      curl_easy_setopt(curl, CURLOPT_URL, *_url);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
      curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_headers);

      curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_ms);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);

      if (rejectUnauthorized) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
      } else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      }

      if (_cacert.length() > 0) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, *_cacert);
      }

      if (_clientcert.length() > 0) {
        if (_clientcertformat.length() > 0) {
          curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, *_clientcertformat);
        }
        curl_easy_setopt(curl, CURLOPT_SSLCERT, *_clientcert);
      }

      if (_clientkeyphrase.length() > 0) {
        curl_easy_setopt(curl, CURLOPT_KEYPASSWD, *_clientkeyphrase);
      }

      if (_clientkey.length() > 0) {
        curl_easy_setopt(curl, CURLOPT_SSLKEY, *_clientkey);
      }


      struct curl_slist *slist = NULL;

      for (size_t i = 0; i < _reqh.size(); ++i) {
        slist = curl_slist_append(slist, _reqh[i].c_str());
      }

      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
      /* init a multi stack */ 
      multi_handle = curl_multi_init();
 
      curl_multi_add_handle(multi_handle, curl);
      curl_multi_perform(multi_handle, &still_running);
 
      do {
            struct timeval timeout;
            int rc; /* select() return code */ 
            CURLMcode mc; /* curl_multi_fdset() return code */ 
       
            fd_set fdread;
            fd_set fdwrite;
            fd_set fdexcep;
            int maxfd = -1;
       
            long curl_timeo = -1;
       
            FD_ZERO(&fdread);
            FD_ZERO(&fdwrite);
            FD_ZERO(&fdexcep);
       
            /* set a suitable timeout to play around with */ 
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
       
            curl_multi_timeout(multi_handle, &curl_timeo);
            if(curl_timeo >= 0) {
              timeout.tv_sec = curl_timeo / 1000;
              if(timeout.tv_sec > 1)
                timeout.tv_sec = 1;
              else
                timeout.tv_usec = (curl_timeo % 1000) * 1000;
            }
       
            /* get file descriptors from the transfers */ 
            mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);
       
            if(mc != CURLM_OK)
            {
              fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
              break;
            }
       
            /* On success the value of maxfd is guaranteed to be >= -1. We call
               select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
               no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
               to sleep 100ms, which is the minimum suggested value in the
               curl_multi_fdset() doc. */ 
       
            if(maxfd == -1) {
      #ifdef _WIN32
              Sleep(100);
              rc = 0;
      #else
              /* Portable sleep for platforms other than Windows. */ 
              struct timeval wait = { 0, 100 * 1000 }; /* 100ms */ 
              rc = select(0, NULL, NULL, NULL, &wait);
      #endif
            }
            else {
              /* Note that on some platforms 'timeout' may be modified by select().
                 If you need access to the original value save a copy beforehand. */ 
              rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
            }
       
            switch(rc) {
            case -1:
              /* select error */ 
              break;
            case 0:
            default:
              /* timeout or readable/writable sockets */ 
              printf("perform!\n");
              res = curl_multi_perform(multi_handle, &still_running);
              printf("running: %d!\n", still_running);
              break;
            }
      } while(still_running);

      curl_multi_cleanup(multi_handle);
      curl_formfree(formpost);
      curl_slist_free_all(slist);
      curl_easy_cleanup(curl);
    }

    // std::cerr<<"error_buffer: "<<error_buffer<<std::endl;

    Local<Object> result = NanNew<Object>();

    if (!res) {
      result->Set(NanNew(sym_body_length), NanNew<Integer>((int32_t)buffer.size()));
      Local<Array> _h = NanNew<Array>();
      for (size_t i = 0; i < headers.size(); ++i) {
        _h->Set(i, NanNew<String>(headers[i].c_str()));
      }
      result->Set(NanNew(sym_headers), _h);
    }
/*    else if (res == CURLE_OPERATION_TIMEDOUT) {
      result->Set(NanNew(sym_timedout), NanNew<Integer>(1));
    } *//*else {
      result->Set(NanNew(sym_error), NanNew<String>(curl_easy_strerror(res)));
    }*/

    headers.clear();
    NanReturnValue(result);
  }
};

Persistent<Function> CurlLib::s_constructor;
std::string CurlLib::buffer;
std::vector<std::string> CurlLib::headers;
Persistent<String> CurlLib::sym_body_length;
Persistent<String> CurlLib::sym_headers;
Persistent<String> CurlLib::sym_timedout;
Persistent<String> CurlLib::sym_error;

extern "C" {
  static void init (Handle<Object> target) {
    CurlLib::Init(target);
  }
  NODE_MODULE(curllib, init);
}

