// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node_http_parser.h"

#include "v8.h"
#include "node.h"
#include "node_buffer.h"

#include <string.h>  /* strdup() */
#if !defined(_MSC_VER)
#include <strings.h>  /* strcasecmp() */
#else
#define strcasecmp _stricmp
#endif
#include <stdlib.h>  /* free() */

// This is a binding to http_parser (https://github.com/joyent/http-parser)
// The goal is to decouple sockets from parsing for more javascript-level
// agility. A Buffer is read from a socket and passed to parser.execute().
// The parser then issues callbacks with slices of the data
//     parser.onMessageBegin
//     parser.onPath
//     parser.onBody
//     ...
// No copying is performed when slicing the buffer, only small reference
// allocations.


namespace node {

using namespace v8;

static Persistent<String> on_headers_sym;
static Persistent<String> on_headers_complete_sym;
static Persistent<String> on_body_sym;
static Persistent<String> on_message_complete_sym;

static Persistent<String> method_sym;
static Persistent<String> status_code_sym;
static Persistent<String> http_version_sym;
static Persistent<String> version_major_sym;
static Persistent<String> version_minor_sym;
static Persistent<String> should_keep_alive_sym;
static Persistent<String> upgrade_sym;
static Persistent<String> headers_sym;
static Persistent<String> url_sym;

static Persistent<String> unknown_method_sym;

#define X(num, name, string) static Persistent<String> name##_sym;
HTTP_METHOD_MAP(X)
#undef X

static Persistent<String> accept_header;
static Persistent<String> accept_charset_header;
static Persistent<String> accept_encoding_header;
static Persistent<String> accept_language_header;
static Persistent<String> authorization_header;
static Persistent<String> cache_control_header;
static Persistent<String> connection_header;
static Persistent<String> content_encoding_header;
static Persistent<String> content_length_header;
static Persistent<String> content_type_header;
static Persistent<String> cookie_header;
static Persistent<String> date_header;
static Persistent<String> etag_header;
static Persistent<String> expect_header;
static Persistent<String> expires_header;
static Persistent<String> host_header;
static Persistent<String> if_modified_since_header;
static Persistent<String> if_none_match_header;
static Persistent<String> last_modified_header;
static Persistent<String> location_header;
static Persistent<String> range_header;
static Persistent<String> server_header;
static Persistent<String> set_cookie_header;
static Persistent<String> te_header;
static Persistent<String> transfer_encoding_header;
static Persistent<String> user_agent_header;
static Persistent<String> via_header;
static Persistent<String> www_authenticate_header;
static Persistent<String> x_forwarded_for_header;

static struct http_parser_settings settings;


// This is a hack to get the current_buffer to the callbacks with the least
// amount of overhead. Nothing else will run while http_parser_execute()
// runs, therefore this pointer can be set and used for the execution.
static Local<Value>* current_buffer;
static char* current_buffer_data;
static size_t current_buffer_len;


#define HTTP_CB(name)                                               \
	  static int name(http_parser* p_) {                              \
	    Parser* self = container_of(p_, Parser, parser_);             \
	    return self->name##_();                                       \
	  }                                                               \
	  int name##_()


#define HTTP_DATA_CB(name)                                          \
  static int name(http_parser* p_, const char* at, size_t length) { \
    Parser* self = container_of(p_, Parser, parser_);               \
    return self->name##_(at, length);                               \
  }                                                                 \
  int name##_(const char* at, size_t length)


static inline Persistent<String>
method_to_str(unsigned short m) {
  switch (m) {
#define X(num, name, string) case HTTP_##name: return name##_sym;
  HTTP_METHOD_MAP(X)
#undef X
  }
  return unknown_method_sym;
}


// helper class for the Parser
struct StringPtr {
  StringPtr() {
    on_heap_ = false;
    Reset();
  }


  ~StringPtr() {
    Reset();
  }


  // If str_ does not point to a heap string yet, this function makes it do
  // so. This is called at the end of each http_parser_execute() so as not
  // to leak references. See issue #2438 and test-http-parser-bad-ref.js.
  void Save() {
    if (!on_heap_ && size_ > 0) {
      char* s = new char[size_];
      memcpy(s, str_, size_);
      str_ = s;
      on_heap_ = true;
    }
  }


  void Reset() {
    if (on_heap_) {
      delete[] str_;
      on_heap_ = false;
    }

    str_ = NULL;
    size_ = 0;
  }


  void Update(const char* str, size_t size) {
    if (str_ == NULL)
      str_ = str;
    else if (on_heap_ || str_ + size_ != str) {
      // Non-consecutive input, make a copy on the heap.
      // TODO Use slab allocation, O(n) allocs is bad.
      char* s = new char[size_ + size];
      memcpy(s, str_, size_);
      memcpy(s + size_, str, size);

      if (on_heap_)
        delete[] str_;
      else
        on_heap_ = true;

      str_ = s;
    }
    size_ += size;
  }


  Local<String> ToString() const {
    if (str_)
      return String::New(str_, size_);
    else
      return String::Empty();
  }


  Handle<String> ToHeaderString() const {
    if (str_ == NULL || size_ == 0)
      return String::Empty();

    char car = str_[0];
    const char* cdr = str_ + 1;

    switch (size_) {
    case 1:
    case 2:
      if (car == 'T') {
        if (cdr[0] == 'E') return te_header;
      }
      break;

    case 3:
      if (car == 'V') {
        if (cdr[0] == 'i' && cdr[1] == 'a') return via_header;
      }
      break;

    case 4:
      if (car == 'D') {
        if (memcmp(cdr, "ate", 3) == 0) return date_header;
      }
      else if (car == 'E') {
        if (memcmp(cdr, "Tag", 3) == 0) return etag_header;
      }
      else if (car == 'H') {
        if (memcmp(cdr, "ost", 3) == 0) return host_header;
      }
      break;

    case 5:
      if (car == 'R') {
        if (memcmp(cdr, "ange", 4) == 0) return range_header;
      }
      break;

    case 6:
      if (car == 'A') {
        if (memcmp(cdr, "ccept", 5) == 0) return accept_header;
      }
      else if (car == 'C') {
        if (memcmp(cdr, "ookie", 5) == 0) return cookie_header;
      }
      else if (car == 'E') {
        if (memcmp(cdr, "xpect", 5) == 0) return expect_header;
      }
      else if (car == 'S') {
        if (memcmp(cdr, "erver", 5) == 0) return server_header;
      }
      break;

    case 7:
      if (car == 'E') {
        if (memcmp(cdr, "xpires", 6) == 0) return expires_header;
      }
      break;

    case 8:
      if (car == 'L') {
        if (memcmp(cdr, "ocation", 7) == 0) return location_header;
      }
      break;

    case 9:
      break;

    case 10:
      if (car == 'C') {
        if (memcmp(cdr, "onnection", 9) == 0) return connection_header;
      }
      else if (car == 'S') {
        if (memcmp(cdr, "et-Cookie", 9) == 0) return set_cookie_header;
      }
      else if (car == 'U') {
        if (memcmp(cdr, "ser-Agent", 9) == 0) return user_agent_header;
      }
      break;

    case 11:
      break;

    case 12:
      if (car == 'C') {
        if (memcmp(cdr, "ontent-Type", 11) == 0) return content_type_header;
      }
      break;

    case 13:
      if (car == 'A') {
        if (memcmp(cdr, "uthorization", 12) == 0) return authorization_header;
      }
      else if (car == 'C') {
        if (memcmp(cdr, "ache-Control", 12) == 0) return cache_control_header;
      }
      else if (car == 'I') {
        if (memcmp(cdr, "f-None-Match", 12) == 0) return if_none_match_header;
      }
      else if (car == 'L') {
        if (memcmp(cdr, "ast-Modified", 12) == 0) return last_modified_header;
      }
      break;

    case 14:
      if (car == 'A') {
        if (memcmp(cdr, "ccept-Charset", 13) == 0) return accept_charset_header;
      }
      else if (car == 'C') {
        if (memcmp(cdr, "ontent-Length", 13) == 0) return content_length_header;
      }
      break;

    case 15:
      if (car == 'A') {
        if (memcmp(cdr, "ccept-", 6) == 0) {
          if (cdr[6] == 'E') {
            if (memcmp(cdr + 7, "ncoding", 7) == 0)
              return accept_encoding_header;
          }
          else if (cdr[6] == 'L') {
            if (memcmp(cdr + 7, "anguage", 7) == 0)
              return accept_language_header;
          }
        }
      }
      else if (car == 'X') {
        if (memcmp(cdr, "-Forwarded-For", 14) == 0)
          return x_forwarded_for_header;
      }
      break;

    case 16:
      if (car == 'C') {
        if (memcmp(cdr, "ontent-Encoding", 15) == 0)
          return content_encoding_header;
      }
      else if (car == 'W') {
        if (memcmp(cdr, "WW-Authenticate", 15) == 0)
          return www_authenticate_header;
      }
      break;

    case 17:
      if (car == 'I') {
        if (memcmp(cdr, "f-Modified-Since", 16) == 0)
          return if_modified_since_header;
      }
      else if (car == 'T') {
        if (memcmp(cdr, "ransfer-Encoding", 16) == 0)
          return transfer_encoding_header;
      }
      break;
    }

    return String::New(str_, size_);
  }


  const char* str_;
  bool on_heap_;
  size_t size_;
};


class Parser : public ObjectWrap {
public:
  Parser(enum http_parser_type type) : ObjectWrap() {
    Init(type);
  }


  ~Parser() {
  }


  HTTP_CB(on_message_begin) {
    num_fields_ = num_values_ = 0;
    url_.Reset();
    return 0;
  }


  HTTP_DATA_CB(on_url) {
    url_.Update(at, length);
    return 0;
  }


  HTTP_DATA_CB(on_header_field) {
    if (num_fields_ == num_values_) {
      // start of new field name
      num_fields_++;
      if (num_fields_ == ARRAY_SIZE(fields_)) {
        // ran out of space - flush to javascript land
        Flush();
        num_fields_ = 1;
        num_values_ = 0;
      }
      fields_[num_fields_ - 1].Reset();
    }

    assert(num_fields_ < (int)ARRAY_SIZE(fields_));
    assert(num_fields_ == num_values_ + 1);

    fields_[num_fields_ - 1].Update(at, length);

    return 0;
  }


  HTTP_DATA_CB(on_header_value) {
    if (num_values_ != num_fields_) {
      // start of new header value
      num_values_++;
      values_[num_values_ - 1].Reset();
    }

    assert(num_values_ < (int)ARRAY_SIZE(values_));
    assert(num_values_ == num_fields_);

    values_[num_values_ - 1].Update(at, length);

    return 0;
  }


  HTTP_CB(on_headers_complete) {
    Local<Value> cb = handle_->Get(on_headers_complete_sym);

    if (!cb->IsFunction())
      return 0;

    Local<Object> message_info = Object::New();

    if (have_flushed_) {
      // Slow case, flush remaining headers.
      Flush();
    }
    else {
      // Fast case, pass headers and URL to JS land.
      message_info->Set(headers_sym, CreateHeaders());
      if (parser_.type == HTTP_REQUEST)
        message_info->Set(url_sym, url_.ToString());
    }
    num_fields_ = num_values_ = 0;

    // METHOD
    if (parser_.type == HTTP_REQUEST) {
      message_info->Set(method_sym, method_to_str(parser_.method));
    }

    // STATUS
    if (parser_.type == HTTP_RESPONSE) {
      message_info->Set(status_code_sym, Integer::New(parser_.status_code));
    }

    // VERSION
    message_info->Set(version_major_sym, Integer::New(parser_.http_major));
    message_info->Set(version_minor_sym, Integer::New(parser_.http_minor));

    message_info->Set(should_keep_alive_sym,
        http_should_keep_alive(&parser_) ? True() : False());

    message_info->Set(upgrade_sym, parser_.upgrade ? True() : False());

    Local<Value> argv[1] = { message_info };

    Local<Value> head_response =
        Local<Function>::Cast(cb)->Call(handle_, 1, argv);

    if (head_response.IsEmpty()) {
      got_exception_ = true;
      return -1;
    }

    return head_response->IsTrue() ? 1 : 0;
  }


  HTTP_DATA_CB(on_body) {
    HandleScope scope;

    Local<Value> cb = handle_->Get(on_body_sym);
    if (!cb->IsFunction())
      return 0;

    Local<Value> argv[3] = {
      *current_buffer,
      Integer::New(at - current_buffer_data),
      Integer::New(length)
    };

    Local<Value> r = Local<Function>::Cast(cb)->Call(handle_, 3, argv);

    if (r.IsEmpty()) {
      got_exception_ = true;
      return -1;
    }

    return 0;
  }


  HTTP_CB(on_message_complete) {
    HandleScope scope;

    if (num_fields_)
      Flush(); // Flush trailing HTTP headers.

    Local<Value> cb = handle_->Get(on_message_complete_sym);

    if (!cb->IsFunction())
      return 0;

    Local<Value> r = Local<Function>::Cast(cb)->Call(handle_, 0, NULL);

    if (r.IsEmpty()) {
      got_exception_ = true;
      return -1;
    }

    return 0;
  }


  static Handle<Value> New(const Arguments& args) {
    HandleScope scope;

    http_parser_type type =
        static_cast<http_parser_type>(args[0]->Int32Value());

    if (type != HTTP_REQUEST && type != HTTP_RESPONSE) {
      return ThrowException(Exception::Error(String::New(
          "Argument must be HTTPParser.REQUEST or HTTPParser.RESPONSE")));
    }

    Parser* parser = new Parser(type);
    parser->Wrap(args.This());

    return args.This();
  }


  void Save() {
    url_.Save();

    for (int i = 0; i < num_fields_; i++) {
      fields_[i].Save();
    }

    for (int i = 0; i < num_values_; i++) {
      values_[i].Save();
    }
  }


  // var bytesParsed = parser->execute(buffer, off, len);
  static Handle<Value> Execute(const Arguments& args) {
    HandleScope scope;

    Parser* parser = ObjectWrap::Unwrap<Parser>(args.This());

    assert(!current_buffer);
    assert(!current_buffer_data);

    if (current_buffer) {
      return ThrowException(Exception::TypeError(
            String::New("Already parsing a buffer")));
    }

    Local<Value> buffer_v = args[0];

    if (!Buffer::HasInstance(buffer_v)) {
      return ThrowException(Exception::TypeError(
            String::New("Argument should be a buffer")));
    }

    Local<Object> buffer_obj = buffer_v->ToObject();
    char *buffer_data = Buffer::Data(buffer_obj);
    size_t buffer_len = Buffer::Length(buffer_obj);

    size_t off = args[1]->Int32Value();
    if (off >= buffer_len) {
      return ThrowException(Exception::Error(
            String::New("Offset is out of bounds")));
    }

    size_t len = args[2]->Int32Value();
    if (off+len > buffer_len) {
      return ThrowException(Exception::Error(
            String::New("off + len > buffer.length")));
    }

    // Assign 'buffer_' while we parse. The callbacks will access that varible.
    current_buffer = &buffer_v;
    current_buffer_data = buffer_data;
    current_buffer_len = buffer_len;
    parser->got_exception_ = false;

    size_t nparsed =
      http_parser_execute(&parser->parser_, &settings, buffer_data + off, len);

    parser->Save();

    // Unassign the 'buffer_' variable
    assert(current_buffer);
    current_buffer = NULL;
    current_buffer_data = NULL;

    // If there was an exception in one of the callbacks
    if (parser->got_exception_) return Local<Value>();

    Local<Integer> nparsed_obj = Integer::New(nparsed);
    // If there was a parse error in one of the callbacks
    // TODO What if there is an error on EOF?
    if (!parser->parser_.upgrade && nparsed != len) {
      enum http_errno err = HTTP_PARSER_ERRNO(&parser->parser_);

      Local<Value> e = Exception::Error(String::NewSymbol("Parse Error"));
      Local<Object> obj = e->ToObject();
      obj->Set(String::NewSymbol("bytesParsed"), nparsed_obj);
      obj->Set(String::NewSymbol("code"), String::New(http_errno_name(err)));
      return scope.Close(e);
    } else {
      return scope.Close(nparsed_obj);
    }
  }


  static Handle<Value> Finish(const Arguments& args) {
    HandleScope scope;

    Parser* parser = ObjectWrap::Unwrap<Parser>(args.This());

    assert(!current_buffer);
    parser->got_exception_ = false;

    int rv = http_parser_execute(&(parser->parser_), &settings, NULL, 0);

    if (parser->got_exception_) return Local<Value>();

    if (rv != 0) {
      enum http_errno err = HTTP_PARSER_ERRNO(&parser->parser_);

      Local<Value> e = Exception::Error(String::NewSymbol("Parse Error"));
      Local<Object> obj = e->ToObject();
      obj->Set(String::NewSymbol("bytesParsed"), Integer::New(0));
      obj->Set(String::NewSymbol("code"), String::New(http_errno_name(err)));
      return scope.Close(e);
    }

    return Undefined();
  }


  static Handle<Value> Reinitialize(const Arguments& args) {
    HandleScope scope;

    http_parser_type type =
        static_cast<http_parser_type>(args[0]->Int32Value());

    if (type != HTTP_REQUEST && type != HTTP_RESPONSE) {
      return ThrowException(Exception::Error(String::New(
          "Argument must be HTTPParser.REQUEST or HTTPParser.RESPONSE")));
    }

    Parser* parser = ObjectWrap::Unwrap<Parser>(args.This());
    parser->Init(type);

    return Undefined();
  }


private:

  Local<Array> CreateHeaders() {
    // num_values_ is either -1 or the entry # of the last header
    // so num_values_ == 0 means there's a single header
    Local<Array> headers = Array::New(2 * num_values_);

    for (int i = 0; i < num_values_; ++i) {
      headers->Set(2 * i, fields_[i].ToHeaderString());
      headers->Set(2 * i + 1, values_[i].ToString());
    }

    return headers;
  }


  // spill headers and request path to JS land
  void Flush() {
    HandleScope scope;

    Local<Value> cb = handle_->Get(on_headers_sym);

    if (!cb->IsFunction())
      return;

    Local<Value> argv[2] = {
      CreateHeaders(),
      url_.ToString()
    };

    Local<Value> r = Local<Function>::Cast(cb)->Call(handle_, 2, argv);

    if (r.IsEmpty())
      got_exception_ = true;

    url_.Reset();
    have_flushed_ = true;
  }


  void Init(enum http_parser_type type) {
    http_parser_init(&parser_, type);
    url_.Reset();
    num_fields_ = 0;
    num_values_ = 0;
    have_flushed_ = false;
    got_exception_ = false;
  }


  http_parser parser_;
  StringPtr fields_[32];  // header fields
  StringPtr values_[32];  // header values
  StringPtr url_;
  int num_fields_;
  int num_values_;
  bool have_flushed_;
  bool got_exception_;
};


void InitHttpParser(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Parser::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("HTTPParser"));

  PropertyAttribute attrib = (PropertyAttribute) (ReadOnly | DontDelete);
  t->Set(String::NewSymbol("REQUEST"), Integer::New(HTTP_REQUEST), attrib);
  t->Set(String::NewSymbol("RESPONSE"), Integer::New(HTTP_RESPONSE), attrib);

  NODE_SET_PROTOTYPE_METHOD(t, "execute", Parser::Execute);
  NODE_SET_PROTOTYPE_METHOD(t, "finish", Parser::Finish);
  NODE_SET_PROTOTYPE_METHOD(t, "reinitialize", Parser::Reinitialize);

  target->Set(String::NewSymbol("HTTPParser"), t->GetFunction());

  on_headers_sym          = NODE_PSYMBOL("onHeaders");
  on_headers_complete_sym = NODE_PSYMBOL("onHeadersComplete");
  on_body_sym             = NODE_PSYMBOL("onBody");
  on_message_complete_sym = NODE_PSYMBOL("onMessageComplete");

#define X(num, name, string) name##_sym = NODE_PSYMBOL(#string);
  HTTP_METHOD_MAP(X)
#undef X
  unknown_method_sym = NODE_PSYMBOL("UNKNOWN_METHOD");

  method_sym = NODE_PSYMBOL("method");
  status_code_sym = NODE_PSYMBOL("statusCode");
  http_version_sym = NODE_PSYMBOL("httpVersion");
  version_major_sym = NODE_PSYMBOL("versionMajor");
  version_minor_sym = NODE_PSYMBOL("versionMinor");
  should_keep_alive_sym = NODE_PSYMBOL("shouldKeepAlive");
  upgrade_sym = NODE_PSYMBOL("upgrade");
  headers_sym = NODE_PSYMBOL("headers");
  url_sym = NODE_PSYMBOL("url");

  settings.on_message_begin    = Parser::on_message_begin;
  settings.on_url              = Parser::on_url;
  settings.on_header_field     = Parser::on_header_field;
  settings.on_header_value     = Parser::on_header_value;
  settings.on_headers_complete = Parser::on_headers_complete;
  settings.on_body             = Parser::on_body;
  settings.on_message_complete = Parser::on_message_complete;

#define X(s) Persistent<String>::New(String::New(s))
  accept_header             = X("Accept");
  accept_charset_header     = X("Accept-Charset");
  accept_encoding_header    = X("Accept-Encoding");
  accept_language_header    = X("Accept-Language");
  authorization_header      = X("Authorization");
  cache_control_header      = X("Cache-Control");
  connection_header         = X("Connection");
  content_encoding_header   = X("Content-Encoding");
  content_length_header     = X("Content-Length");
  content_type_header       = X("Content-Type");
  cookie_header             = X("Cookie");
  date_header               = X("Date");
  etag_header               = X("ETag");
  expect_header             = X("Expect");
  expires_header            = X("Expires");
  host_header               = X("Host");
  if_modified_since_header  = X("If-Modified-Since");
  if_none_match_header      = X("If-None-Match");
  last_modified_header      = X("Last-Modified");
  location_header           = X("Location");
  range_header              = X("Range");
  server_header             = X("Server");
  set_cookie_header         = X("Set-Cookie");
  te_header                 = X("TE");
  transfer_encoding_header  = X("Transfer-Encoding");
  user_agent_header         = X("User-Agent");
  via_header                = X("Via");
  www_authenticate_header   = X("WWW-Authenticate");
  x_forwarded_for_header    = X("X-Forwarded-For");
#undef X
}

}  // namespace node

NODE_MODULE(node_http_parser, node::InitHttpParser)
