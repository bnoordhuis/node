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

static Persistent<String> unknown_method_sym;

#define X(num, name, string) static Persistent<String> name##_sym;
HTTP_METHOD_MAP(X)
#undef X

static struct http_parser_settings settings;


// This is a hack to get the current_buffer to the callbacks with the least
// amount of overhead. Nothing else will run while http_parser_execute()
// runs, therefore this pointer can be set and used for the execution.
static Local<Value>* current_buffer;
static char* current_buffer_data;
static size_t current_buffer_len;


static inline Persistent<String>
method_to_str(unsigned short m) {
  switch (m) {
#define X(num, name, string) case HTTP_##name: return name##_sym;
  HTTP_METHOD_MAP(X)
#undef X
  }
  return unknown_method_sym;
}


class Parser : public ObjectWrap {
public:
  Parser(enum http_parser_type type) : ObjectWrap() {
    static Persistent<Function> uint32_array_constructor;
    HandleScope scope;

    // XXX Make v8_typed_array.h export the constructor?
    if (uint32_array_constructor.IsEmpty()) {
      Local<String> name = String::New("Uint32Array");
      Local<Value> obj = Context::GetCurrent()->Global()->Get(name);
      assert(!obj.IsEmpty() && "Uint32Array: type not found");
      assert(obj->IsFunction() && "Uint32Array: not a constructor");
      uint32_array_constructor = Persistent<Function>::New(obj.As<Function>());
    }

    Local<Value> size = Integer::NewFromUnsigned(kMaxHeaderValues);
    array_ = Persistent<Object>::New(
        uint32_array_constructor->NewInstance(1, &size));
    offsets_ = static_cast<uint32_t*>(
        array_->GetIndexedPropertiesExternalArrayData());

    Init(type);
  }


  ~Parser() {
    array_.Dispose();
    array_.Clear();
  }


  static int OnMessageBegin(http_parser* parser) {
    Parser* self = container_of(parser, Parser, parser_);
    self->prev_header_state_ = NONE;
    self->index_ = 0;
    return 0;
  }


  enum HeaderState { NONE, URL, FIELD, VALUE };
  HeaderState prev_header_state_;


  static int OnHeaderData(HeaderState state,
                          http_parser* parser,
                          const char* at,
                          size_t length) {
    assert(at >= current_buffer_data);
    assert(at + length <= current_buffer_data + current_buffer_len);

    Parser* self = container_of(parser, Parser, parser_);

    if (state != self->prev_header_state_) {
      if (self->prev_header_state_ != NONE &&
          ++self->index_ == kMaxHeaderValues) {
        self->Flush();
        self->index_ = 0;
      }
      assert(self->index_ % 2 == 0);
      self->prev_header_state_ = state;
      //printf("self->offsets_[%d] = %d\n", (int) self->index_, (int) (at - current_buffer_data));
      self->offsets_[self->index_++] = (at - current_buffer_data);
    }
    //printf("self->offsets_[%d] = %d \"%.*s\"\n", (int) self->index_, (int) ((at - current_buffer_data) + length), (int) length, at);
    self->offsets_[self->index_] = (at - current_buffer_data) + length;

    return 0;
  }


#define X(name, state)                                                        \
  static int name(http_parser* parser, const char* at, size_t length) {       \
    return OnHeaderData(state, parser, at, length);                           \
  }

X(OnURL, URL)
X(OnHeaderField, FIELD)
X(OnHeaderValue, VALUE)

#undef X


  static int OnHeadersComplete(http_parser* parser) {
    Parser* self = container_of(parser, Parser, parser_);

    Local<Value> cb = self->handle_->Get(on_headers_complete_sym);
    if (!cb->IsFunction())
      return 0;

    Local<Object> message_info = Object::New();

    if (self->have_flushed_) {
      // Slow case, flush remaining headers.
      self->Flush();
      self->index_ = 0;
    }

    // METHOD
    if (self->parser_.type == HTTP_REQUEST) {
      message_info->Set(method_sym, method_to_str(self->parser_.method));
    }

    // STATUS
    if (self->parser_.type == HTTP_RESPONSE) {
      message_info->Set(status_code_sym,
                        Integer::New(self->parser_.status_code));
    }

    // VERSION
    message_info->Set(version_major_sym,
                      Integer::New(self->parser_.http_major));
    message_info->Set(version_minor_sym,
                      Integer::New(self->parser_.http_minor));

    message_info->Set(should_keep_alive_sym,
                      Boolean::New(http_should_keep_alive(&self->parser_)));

    message_info->Set(upgrade_sym, Boolean::New(self->parser_.upgrade));

    bool has_url = self->parser_.type == HTTP_REQUEST &&
                   self->have_flushed_ == false;

    Local<Value> argv[] = {
      message_info,
      *current_buffer,
      *self->array_,
      Integer::NewFromUnsigned(self->index_ + 1),
      Local<Boolean>::New(Boolean::New(has_url)),
    };
    self->index_ = 0;

    Local<Value> head_response =
        Local<Function>::Cast(cb)->Call(self->handle_, ARRAY_SIZE(argv), argv);

    if (head_response.IsEmpty()) {
      self->got_exception_ = true;
      return -1;
    }

    return head_response->IsTrue() ? 1 : 0;
  }


  static int OnBody(http_parser* parser, const char* at, size_t length) {
    assert(at >= current_buffer_data);
    assert(at + length <= current_buffer_data + current_buffer_len);

    Parser* self = container_of(parser, Parser, parser_);
    HandleScope scope;

    Local<Value> cb = self->handle_->Get(on_body_sym);
    if (!cb->IsFunction())
      return 0;

    Local<Value> argv[3] = {
      *current_buffer,
      Integer::New(at - current_buffer_data),
      Integer::New(length)
    };

    Local<Value> r = Local<Function>::Cast(cb)->Call(self->handle_, 3, argv);

    if (r.IsEmpty()) {
      self->got_exception_ = true;
      return -1;
    }

    return 0;
  }


  static int OnMessageComplete(http_parser* parser) {
    Parser* self = container_of(parser, Parser, parser_);
    HandleScope scope;

    if (self->index_ != 0)
      self->Flush(); // Flush trailing HTTP headers.

    Local<Value> cb = self->handle_->Get(on_message_complete_sym);

    if (!cb->IsFunction())
      return 0;

    Local<Value> r = Local<Function>::Cast(cb)->Call(self->handle_, 0, NULL);

    if (r.IsEmpty()) {
      self->got_exception_ = true;
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
  // spill headers and request path to JS land
  void Flush() {
    HandleScope scope;

    Local<Value> cb = handle_->Get(on_headers_sym);

    if (!cb->IsFunction())
      return;

    bool has_url = parser_.type == HTTP_REQUEST && have_flushed_ == false;

    Local<Value> argv[] = {
      *current_buffer,
      *array_,
      Integer::NewFromUnsigned(index_),
      Local<Boolean>::New(Boolean::New(has_url))
    };

    Local<Value> r = Local<Function>::Cast(cb)->Call(handle_,
                                                     ARRAY_SIZE(argv),
                                                     argv);

    if (r.IsEmpty())
      got_exception_ = true;

    have_flushed_ = true;
  }


  void Init(enum http_parser_type type) {
    http_parser_init(&parser_, type);
    index_ = 0;
    have_flushed_ = false;
    got_exception_ = false;
  }


  static const unsigned int kMaxHeaderValues = 64;
  Persistent<Object> array_; // Uint32Array
  // 0-1=url, 2-3=header name, 4-5=header value, 6-7=header name, etc.
  uint32_t* offsets_;
  unsigned int index_;
  http_parser parser_;
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

  settings.on_message_begin    = Parser::OnMessageBegin;
  settings.on_url              = Parser::OnURL;
  settings.on_header_field     = Parser::OnHeaderField;
  settings.on_header_value     = Parser::OnHeaderValue;
  settings.on_headers_complete = Parser::OnHeadersComplete;
  settings.on_body             = Parser::OnBody;
  settings.on_message_complete = Parser::OnMessageComplete;
}

}  // namespace node

NODE_MODULE(node_http_parser, node::InitHttpParser)
