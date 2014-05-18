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


#include "node_buffer.h"

#include "node.h"
#include "string_bytes.h"

#include "env.h"
#include "env-inl.h"

#include "v8.h"
#include "v8-profiler.h"

#include <assert.h>
#include <string.h> // memcpy
#include <limits.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))

namespace node {

inline void ThrowException(v8::Local<v8::Value> ex) {
  v8::Isolate::GetCurrent()->ThrowException(ex);
}

using namespace v8;

#define SLICE_ARGS(start_arg, end_arg)                               \
  if (!start_arg->IsInt32() || !end_arg->IsInt32()) {                \
    return ThrowTypeError("Bad argument.");                          \
  }                                                                  \
  int32_t start = start_arg->Int32Value();                           \
  int32_t end = end_arg->Int32Value();                               \
  if (start < 0 || end < 0) {                                        \
    return ThrowTypeError("Bad argument.");                          \
  }                                                                  \
  if (!(start <= end)) {                                             \
    return ThrowTypeError("Must have start <= end");                 \
  }                                                                  \
  if ((size_t)end > parent->length_) {                               \
    return ThrowTypeError("end cannot be longer than parent.length");\
  }


static Persistent<String> write_sym;
static Persistent<Function> fast_buffer_constructor;
Persistent<FunctionTemplate> Buffer::constructor_template;


Handle<Object> Buffer::New(Handle<String> string) {
  Isolate* isolate = Isolate::GetCurrent();
  EscapableHandleScope scope(isolate);
  // get Buffer from global scope.
  Local<Object> global = isolate->GetCurrentContext()->Global();
  Local<Value> bv = global->Get(FIXED_ONE_BYTE_STRING(isolate, "Buffer"));
  assert(bv->IsFunction());
  Local<Function> b = Local<Function>::Cast(bv);

  Local<Value> argv[1] = { Local<Value>(string) };
  Local<Object> instance = b->NewInstance(1, argv);

  return scope.Escape(instance);
}


Buffer* Buffer::New(size_t length) {
  Isolate* isolate = Isolate::GetCurrent();
  Local<Value> arg = Integer::NewFromUnsigned(isolate, length);
  Local<Object> b = PersistentToLocal(isolate, constructor_template)->GetFunction()->NewInstance(1, &arg);
  if (b.IsEmpty()) return NULL;

  return ObjectWrap::Unwrap<Buffer>(b);
}


Buffer* Buffer::New(const char* data, size_t length) {
  Isolate* isolate = Isolate::GetCurrent();
  Local<Value> arg = Integer::NewFromUnsigned(isolate, 0);
  Local<Object> obj = PersistentToLocal(isolate, constructor_template)->GetFunction()->NewInstance(1, &arg);

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(obj);
  buffer->Replace(const_cast<char*>(data), length, NULL, NULL);

  return buffer;
}


Buffer* Buffer::New(char *data, size_t length,
                    free_callback callback, void *hint) {
  Isolate* isolate = Isolate::GetCurrent();
  Local<Value> arg = Integer::NewFromUnsigned(isolate, 0);
  Local<Object> obj = PersistentToLocal(isolate, constructor_template)->GetFunction()->NewInstance(1, &arg);

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(obj);
  buffer->Replace(data, length, callback, hint);

  return buffer;
}


void Buffer::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());

  if (!args[0]->IsUint32()) return ThrowTypeError("Bad argument");

  size_t length = args[0]->Uint32Value();
  if (length > Buffer::kMaxLength) {
    return ThrowRangeError("length > kMaxLength");
  }
  new Buffer(args.This(), length);
}


Buffer::Buffer(Handle<Object> wrapper, size_t length) : ObjectWrap() {
  Wrap(wrapper);

  length_ = 0;
  callback_ = NULL;

  Replace(NULL, length, NULL, NULL);
}


Buffer::~Buffer() {
  const int64_t size = -static_cast<int64_t>(sizeof(*this) + length_);
  Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(size);
}


// if replace doesn't have a callback, data must be copied
// const_cast in Buffer::New requires this
void Buffer::Replace(char *data, size_t length,
                     free_callback callback, void *hint) {
  Isolate* isolate = Isolate::GetCurrent();
  if (callback_) {
    callback_(data_, callback_hint_);
  } else if (length_) {
    delete [] data_;
    isolate->AdjustAmountOfExternalAllocatedMemory(
        -static_cast<intptr_t>(sizeof(Buffer) + length_));
  }

  length_ = length;
  callback_ = callback;
  callback_hint_ = hint;

  if (callback_) {
    data_ = data;
  } else if (length_) {
    data_ = new char[length_];
    if (data)
      memcpy(data_, data, length_);
    isolate->AdjustAmountOfExternalAllocatedMemory(sizeof(Buffer) + length_);
  } else {
    data_ = NULL;
  }

  handle()->SetIndexedPropertiesToExternalArrayData(data_,
                                                    kExternalUnsignedByteArray,
                                                    length_);
  handle()->Set(FIXED_ONE_BYTE_STRING(isolate, "length"), Integer::NewFromUnsigned(isolate, length_));
}

template <encoding encoding>
void Buffer::StringSlice(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])

  const char* src = parent->data_ + start;
  size_t slen = (end - start);
  args.GetReturnValue().Set(StringBytes::Encode(src, slen, encoding));
}


void Buffer::BinarySlice(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringSlice<BINARY>(args);
}


void Buffer::AsciiSlice(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringSlice<ASCII>(args);
}


void Buffer::Utf8Slice(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringSlice<UTF8>(args);
}


void Buffer::Ucs2Slice(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringSlice<UCS2>(args);
}



void Buffer::HexSlice(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringSlice<HEX>(args);
}



void Buffer::Base64Slice(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringSlice<BASE64>(args);
}


// buffer.fill(value, start, end);
void Buffer::Fill(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());

  if (!args[0]->IsInt32()) {
    return ThrowTypeError("value is not a number");
  }
  int value = (char)args[0]->Int32Value();

  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[1], args[2])

  memset( (void*)(parent->data_ + start),
          value,
          end - start);
}


// var bytesCopied = buffer.copy(target, targetStart, sourceStart, sourceEnd);
void Buffer::Copy(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  Buffer *source = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!Buffer::HasInstance(args[0])) {
    return ThrowTypeError("First arg should be a Buffer");
  }

  Local<Value> target = args[0];
  char* target_data = Buffer::Data(target);
  size_t target_length = Buffer::Length(target);
  size_t target_start = args[1]->IsUndefined() ? 0 : args[1]->Uint32Value();
  size_t source_start = args[2]->IsUndefined() ? 0 : args[2]->Uint32Value();
  size_t source_end = args[3]->IsUndefined() ? source->length_
                                              : args[3]->Uint32Value();

  if (source_end < source_start) {
    return env->ThrowRangeError("sourceEnd < sourceStart");
  }

  // Copy 0 bytes; we're done
  if (source_end == source_start) {
    return args.GetReturnValue().Set(0);
  }

  if (target_start >= target_length) {
    return env->ThrowRangeError("targetStart out of bounds");
  }

  if (source_start >= source->length_) {
    return env->ThrowRangeError("sourceStart out of bounds");
  }

  if (source_end > source->length_) {
    return env->ThrowRangeError("sourceEnd out of bounds");
  }

  size_t to_copy = MIN(MIN(source_end - source_start,
                           target_length - target_start),
                           source->length_ - source_start);

  // need to use slightly slower memmove is the ranges might overlap
  memmove((void *)(target_data + target_start),
          (const void*)(source->data_ + source_start),
          to_copy);

  args.GetReturnValue().Set(static_cast<uint32_t>(to_copy));
}


void Buffer::Base64Write(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringWrite<BASE64>(args);
}

void Buffer::BinaryWrite(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringWrite<BINARY>(args);
}

void Buffer::Utf8Write(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringWrite<UTF8>(args);
}

void Buffer::Ucs2Write(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringWrite<UCS2>(args);
}

void Buffer::HexWrite(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringWrite<HEX>(args);
}

void Buffer::AsciiWrite(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return Buffer::StringWrite<ASCII>(args);
}

template <encoding encoding>
void Buffer::StringWrite(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);

  Buffer* buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    return ThrowTypeError("Argument must be a string");
  }

  Local<String> str = args[0].As<String>();

  int length = str->Length();

  if (length == 0) {
    Local<Integer> val = Integer::New(isolate, 0);
    PersistentToLocal(isolate, constructor_template)->GetFunction()->Set(FIXED_ONE_BYTE_STRING(isolate, "_charsWritten"), val);
    return args.GetReturnValue().Set(val);
  }

  if (encoding == HEX && length % 2 != 0)
    return ThrowTypeError("Invalid hex string");

  size_t offset = args[1]->Int32Value();
  size_t max_length = args[2]->IsUndefined() ? buffer->length_ - offset
                                             : args[2]->Uint32Value();
  max_length = MIN(buffer->length_ - offset, max_length);

  if (max_length == 0) {
    // shortcut: nothing to write anyway
    Local<Integer> val = Integer::New(isolate, 0);
    PersistentToLocal(isolate, constructor_template)->GetFunction()->Set(FIXED_ONE_BYTE_STRING(isolate, "_charsWritten"), val);
    return args.GetReturnValue().Set(val);
  }

  if (encoding == UCS2)
    max_length = max_length / 2;

  if (offset >= buffer->length_) {
    return ThrowTypeError("Offset is out of bounds");
  }

  char* start = buffer->data_ + offset;
  int chars_written;
  size_t written = StringBytes::Write(start,
                                      max_length,
                                      str,
                                      encoding,
                                      &chars_written);

  PersistentToLocal(isolate, constructor_template)->GetFunction()->Set(FIXED_ONE_BYTE_STRING(isolate, "_charsWritten"),
                                           Integer::New(isolate, chars_written));

  args.GetReturnValue().Set(static_cast<uint32_t>(written));
}

static bool is_big_endian() {
  const union { uint8_t u8[2]; uint16_t u16; } u = {{0, 1}};
  return u.u16 == 1 ? true : false;
}


static void swizzle(char* buf, size_t len) {
  char t;
  for (size_t i = 0; i < len / 2; ++i) {
    t = buf[i];
    buf[i] = buf[len - i - 1];
    buf[len - i - 1] = t;
  }
}


template <typename T, bool ENDIANNESS>
void ReadFloatGeneric(const v8::FunctionCallbackInfo<v8::Value>& args) {
  double offset_tmp = args[0]->NumberValue();
  int64_t offset = static_cast<int64_t>(offset_tmp);
  bool doAssert = !args[1]->BooleanValue();

  if (doAssert) {
    if (offset_tmp != offset || offset < 0)
      return ThrowTypeError("offset is not uint");
    size_t len = static_cast<size_t>(
                    args.This()->GetIndexedPropertiesExternalArrayDataLength());
    if (offset + sizeof(T) > len)
      return ThrowRangeError("Trying to read beyond buffer length");
  }

  T val;
  char* data = static_cast<char*>(
                    args.This()->GetIndexedPropertiesExternalArrayData());
  char* ptr = data + offset;

  memcpy(&val, ptr, sizeof(T));
  if (ENDIANNESS != is_big_endian())
    swizzle(reinterpret_cast<char*>(&val), sizeof(T));

  // TODO: when Number::New is updated to accept an Isolate, make the change
  args.GetReturnValue().Set(val);
}


void Buffer::ReadFloatLE(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return ReadFloatGeneric<float, false>(args);
}


void Buffer::ReadFloatBE(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return ReadFloatGeneric<float, true>(args);
}


void Buffer::ReadDoubleLE(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return ReadFloatGeneric<double, false>(args);
}


void Buffer::ReadDoubleBE(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return ReadFloatGeneric<double, true>(args);
}


template <typename T, bool ENDIANNESS>
void WriteFloatGeneric(const v8::FunctionCallbackInfo<v8::Value>& args) {
  bool doAssert = !args[2]->BooleanValue();

  if (doAssert) {
    if (!args[0]->IsNumber())
      return ThrowTypeError("value not a number");
    if (!args[1]->IsUint32())
      return ThrowTypeError("offset is not uint");
  }

  T val = static_cast<T>(args[0]->NumberValue());
  size_t offset = args[1]->Uint32Value();
  char* data = static_cast<char*>(
                    args.This()->GetIndexedPropertiesExternalArrayData());
  char* ptr = data + offset;

  if (doAssert) {
    size_t len = static_cast<size_t>(
                    args.This()->GetIndexedPropertiesExternalArrayDataLength());
    if (offset + sizeof(T) > len || offset + sizeof(T) < offset)
      return ThrowRangeError("Trying to write beyond buffer length");
  }

  memcpy(ptr, &val, sizeof(T));
  if (ENDIANNESS != is_big_endian())
    swizzle(ptr, sizeof(T));
}


void Buffer::WriteFloatLE(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return WriteFloatGeneric<float, false>(args);
}


void Buffer::WriteFloatBE(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return WriteFloatGeneric<float, true>(args);
}


void Buffer::WriteDoubleLE(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return WriteFloatGeneric<double, false>(args);
}


void Buffer::WriteDoubleBE(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return WriteFloatGeneric<double, true>(args);
}


// var nbytes = Buffer.byteLength("string", "utf8")
void Buffer::ByteLength(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());

  if (!args[0]->IsString()) {
    return ThrowTypeError("Argument must be a string");
  }

  Local<String> s = args[0]->ToString();
  enum encoding e = ParseEncoding(args[1], UTF8);

  args.GetReturnValue().Set(static_cast<uint32_t>(StringBytes::Size(s, e)));
}


void Buffer::MakeFastBuffer(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());

  if (!Buffer::HasInstance(args[0])) {
    return ThrowTypeError("First argument must be a Buffer");
  }

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());
  Local<Object> fast_buffer = args[1]->ToObject();;
  uint32_t offset = args[2]->Uint32Value();
  uint32_t length = args[3]->Uint32Value();

  if (offset > buffer->length_) {
    return ThrowRangeError("offset out of range");
  }

  if (offset + length > buffer->length_) {
    return ThrowRangeError("length out of range");
  }

  // Check for wraparound. Safe because offset and length are unsigned.
  if (offset + length < offset) {
    return ThrowRangeError("offset or length out of range");
  }

  fast_buffer->SetIndexedPropertiesToExternalArrayData(buffer->data_ + offset,
                                                       kExternalUnsignedByteArray,
                                                       length);
}


bool Buffer::HasInstance(Handle<Value> val) {
  if (!val->IsObject()) return false;
  Local<Object> obj = val->ToObject();

  ExternalArrayType type = obj->GetIndexedPropertiesExternalArrayDataType();
  if (type != kExternalUnsignedByteArray)
    return false;

  // Also check for SlowBuffers that are empty.
  Isolate* isolate = Isolate::GetCurrent();
  if (PersistentToLocal(isolate, constructor_template)->HasInstance(obj))
    return true;

  assert(!fast_buffer_constructor.IsEmpty());
  return obj->GetConstructor()->StrictEquals(PersistentToLocal(isolate, fast_buffer_constructor));
}


void SetFastBufferConstructor(const v8::FunctionCallbackInfo<v8::Value>& args) {
  assert(args[0]->IsFunction());
  fast_buffer_constructor.Reset(args.GetIsolate(), args[0].As<Function>());
}


void Buffer::Initialize(Handle<Object> target) {
  Isolate* isolate = Isolate::GetCurrent();

  Local<FunctionTemplate> t = FunctionTemplate::New(isolate, Buffer::New);
  constructor_template.Reset(isolate, t);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "SlowBuffer"));

  NODE_SET_PROTOTYPE_METHOD(t, "binarySlice", Buffer::BinarySlice);
  NODE_SET_PROTOTYPE_METHOD(t, "asciiSlice", Buffer::AsciiSlice);
  NODE_SET_PROTOTYPE_METHOD(t, "base64Slice", Buffer::Base64Slice);
  NODE_SET_PROTOTYPE_METHOD(t, "ucs2Slice", Buffer::Ucs2Slice);
  NODE_SET_PROTOTYPE_METHOD(t, "hexSlice", Buffer::HexSlice);
  NODE_SET_PROTOTYPE_METHOD(t, "utf8Slice", Buffer::Utf8Slice);
  // TODO NODE_SET_PROTOTYPE_METHOD(t, "utf16Slice", Utf16Slice);

  NODE_SET_PROTOTYPE_METHOD(t, "utf8Write", Buffer::Utf8Write);
  NODE_SET_PROTOTYPE_METHOD(t, "asciiWrite", Buffer::AsciiWrite);
  NODE_SET_PROTOTYPE_METHOD(t, "binaryWrite", Buffer::BinaryWrite);
  NODE_SET_PROTOTYPE_METHOD(t, "base64Write", Buffer::Base64Write);
  NODE_SET_PROTOTYPE_METHOD(t, "ucs2Write", Buffer::Ucs2Write);
  NODE_SET_PROTOTYPE_METHOD(t, "hexWrite", Buffer::HexWrite);
  NODE_SET_PROTOTYPE_METHOD(t, "readFloatLE", Buffer::ReadFloatLE);
  NODE_SET_PROTOTYPE_METHOD(t, "readFloatBE", Buffer::ReadFloatBE);
  NODE_SET_PROTOTYPE_METHOD(t, "readDoubleLE", Buffer::ReadDoubleLE);
  NODE_SET_PROTOTYPE_METHOD(t, "readDoubleBE", Buffer::ReadDoubleBE);
  NODE_SET_PROTOTYPE_METHOD(t, "writeFloatLE", Buffer::WriteFloatLE);
  NODE_SET_PROTOTYPE_METHOD(t, "writeFloatBE", Buffer::WriteFloatBE);
  NODE_SET_PROTOTYPE_METHOD(t, "writeDoubleLE", Buffer::WriteDoubleLE);
  NODE_SET_PROTOTYPE_METHOD(t, "writeDoubleBE", Buffer::WriteDoubleBE);
  NODE_SET_PROTOTYPE_METHOD(t, "fill", Buffer::Fill);
  NODE_SET_PROTOTYPE_METHOD(t, "copy", Buffer::Copy);

  NODE_SET_METHOD(t->GetFunction(),
                  "byteLength",
                  Buffer::ByteLength);
  NODE_SET_METHOD(t->GetFunction(),
                  "makeFastBuffer",
                  Buffer::MakeFastBuffer);

  target->Set(FIXED_ONE_BYTE_STRING(isolate, "SlowBuffer"), t->GetFunction());
  target->Set(FIXED_ONE_BYTE_STRING(isolate, "setFastBufferConstructor"),
              FunctionTemplate::New(isolate, SetFastBufferConstructor)->GetFunction());
}


}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(buffer, node::Buffer::Initialize);
