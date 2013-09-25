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

#include "function-tracer.h"
#include <cstring>
#include <cstdio>
#include <set>

#define debug(...) printf(__VA_ARGS__)

namespace node {
namespace trace {

using v8::Handle;
using v8::Isolate;
using v8::JitCodeEvent;
using v8::Script;
using v8::V8;
using v8::kJitCodeEventEnumExisting;

class FunctionInfoMap;

class FunctionInfo {
 public:
  FunctionInfo(uintptr_t code_start,
               size_t size = 0,
               const char* name = NULL,
               size_t namelen = 0,
               Handle<Script> script = Handle<Script>());
  Handle<Script> script() const;
  uintptr_t code_start() const;
  uintptr_t code_end() const;
  size_t size() const;
  const char* name() const;
  bool operator<(const FunctionInfo& that) const;
 private:
  FunctionInfo(const FunctionInfo&);
  void operator=(const FunctionInfo&);
  const uintptr_t code_start_;
  const uintptr_t code_end_;
  Handle<Script> script_;
  char name_[64];
};

class FunctionInfoMap {
 public:
  FunctionInfoMap();
  void Insert(FunctionInfo* function_info);
  FunctionInfo* Remove(uintptr_t address);
  FunctionInfo* Find(uintptr_t address);
 private:
  // We want stable addresses so wrap the FunctionInfo instances.
  class Entry {
   public:
    explicit Entry(FunctionInfo* function_info);
    bool operator<(const Entry& that) const;
    FunctionInfo* operator*() const;
   private:
    FunctionInfo* const function_info_;
  };
  FunctionInfoMap(const FunctionInfoMap&);
  void operator=(const FunctionInfoMap&);
  typedef std::set<Entry> FunctionInfoContainer;
  FunctionInfoContainer function_info_container_;
};

void FunctionEntryHook(uintptr_t function_address, uintptr_t stack_address);
uintptr_t ReturnAddressLocationResolver(uintptr_t return_address);
void JitCodeEventHandler(const v8::JitCodeEvent* event);

// TODO(bnoordhuis) Make per-isolate.
FunctionInfoMap function_info_map;
FunctionInfo* last_added;
unsigned int indent_level;

void Initialize(v8::Isolate* isolate) {
  V8::SetFunctionEntryHook(isolate, trace::FunctionEntryHook);
  V8::SetReturnAddressLocationResolver(trace::ReturnAddressLocationResolver);
}

void StartJitCodeEventHandler() {
  V8::SetJitCodeEventHandler(kJitCodeEventEnumExisting, JitCodeEventHandler);
}

void Indent() {
  const unsigned int max = 65;
  const unsigned int k = indent_level % max;
  for (unsigned int i = 0; i < k; ++i) {
    putc(' ', stdout);
  }
}

void FunctionEntryHook(uintptr_t function_address, uintptr_t stack_address) {
  Indent();
  if (FunctionInfo* function_info = function_info_map.Find(function_address)) {
    printf("-> %s (%zx-%zx)\n",
           function_info->name(),
           function_info->code_start(),
           function_info->code_end());
  } else{
    printf("-> <unknown> (%zx)\n", function_address);
  }
  indent_level += 1;
}

uintptr_t ReturnAddressLocationResolver(uintptr_t return_address) {
  indent_level -= 1;
  Indent();
  // return_address points to the stack slot containing the return address.
  uintptr_t* stack_slot = reinterpret_cast<uintptr_t*>(return_address);
  if (FunctionInfo* function_info = function_info_map.Find(*stack_slot)) {
    printf("<- %s (%zx)\n", function_info->name(), *stack_slot);
  } else {
    printf("<- <unknown> (%zx)\n", *stack_slot);
  }
  return return_address;
}

void JitCodeEventHandler(const JitCodeEvent* event) {
  switch (event->type) {
    case JitCodeEvent::CODE_ADDED:
    {
      debug("CODE_ADDED %.*s code_start=%p code_len=%zu\n",
            static_cast<int>(event->name.len),
            event->name.str,
            event->code_start,
            event->code_len);
      uintptr_t address = reinterpret_cast<uintptr_t>(event->code_start);
      FunctionInfo* function_info = new FunctionInfo(address,
                                                     event->code_len,
                                                     event->name.str,
                                                     event->name.len,
                                                     event->script);
      function_info_map.Insert(function_info);
      last_added = function_info;
      break;
    }
    case JitCodeEvent::CODE_MOVED:
    {
      debug("CODE_MOVED %.*s code_start=%p code_len=%zu new_code_start=%p\n",
            static_cast<int>(event->name.len),
            event->name.str,
            event->code_start,
            event->code_len,
            event->new_code_start);
        uintptr_t address = reinterpret_cast<uintptr_t>(event->code_start);
        if (FunctionInfo* function_info = function_info_map.Remove(address)) {
          uintptr_t new_address =
              reinterpret_cast<uintptr_t>(event->new_code_start);
          FunctionInfo* new_function_info =
              new FunctionInfo(new_address,
                               function_info->size(),
                               function_info->name(),
                               strlen(function_info->name()),
                               function_info->script());
          function_info_map.Insert(new_function_info);
          delete function_info;
        }
      break;
    }
    case JitCodeEvent::CODE_REMOVED:
    {
      debug("CODE_REMOVED %.*s code_start=%p code_len=%zu\n",
            static_cast<int>(event->name.len),
            event->name.str,
            event->code_start,
            event->code_len);
      uintptr_t address = reinterpret_cast<uintptr_t>(event->code_start);
      delete function_info_map.Remove(address);
      break;
    }
    case JitCodeEvent::CODE_START_LINE_INFO_RECORDING:
      debug("CODE_START_LINE_INFO_RECORDING\n");
      // This const_cast is intentional. This part of the V8 API is less than
      // stellar...
      const_cast<JitCodeEvent*>(event)->user_data = last_added;
      break;
    case JitCodeEvent::CODE_ADD_LINE_POS_INFO:
    {
      const char* position_type = "UNKNOWN";
      switch (event->line_info.position_type) {
        case JitCodeEvent::POSITION:
          position_type = "POSITION";
          break;
        case JitCodeEvent::STATEMENT_POSITION:
          position_type = "STATEMENT_POSITION";
          break;
      }
      int line_number = -1;
      const FunctionInfo* function_info =
          static_cast<const FunctionInfo*>(event->user_data);
      Handle<Script> script = function_info->script();
      if (script.IsEmpty() == false) {
        line_number = script->GetLineNumber(event->line_info.pos);
      }
      debug("CODE_ADD_LINE_POS_INFO offset=%zu pos=%zu position_type=%s\n",
            event->line_info.offset,
            event->line_info.pos,
            position_type);
      debug("# %s:%d\n", function_info->name(), line_number + 1);
      break;
    }
    case JitCodeEvent::CODE_END_LINE_INFO_RECORDING:
      debug("CODE_END_LINE_INFO_RECORDING code_start=%p\n", event->code_start);
      break;
  }
}

FunctionInfo::FunctionInfo(uintptr_t code_start,
                           size_t size,
                           const char* name,
                           size_t namelen,
                           Handle<Script> script)
    : code_start_(code_start)
    , code_end_(code_start_ + size)
    , script_(script) {
  if (namelen >= sizeof(name_)) {
    namelen = sizeof(name_) - 1;
  }
  if (name != NULL) {
    memcpy(name_, name, namelen);
  }
  name_[namelen] = '\0';
}

Handle<Script> FunctionInfo::script() const {
  return script_;
}

uintptr_t FunctionInfo::code_start() const {
  return code_start_;
}

uintptr_t FunctionInfo::code_end() const {
  return code_end_;
}

size_t FunctionInfo::size() const {
  return code_end() - code_start();
}

bool FunctionInfo::operator<(const FunctionInfo& that) const {
  //return code_start() < that.code_start();
  return code_start() < that.code_start() && code_end() < that.code_end();
}

const char* FunctionInfo::name() const {
  return name_;
}

FunctionInfoMap::FunctionInfoMap() {
}

void FunctionInfoMap::Insert(FunctionInfo* function_info) {
  Entry entry(function_info);
  function_info_container_.insert(entry);
}

FunctionInfo* FunctionInfoMap::Remove(uintptr_t address) {
  FunctionInfo with_address(address);
  Entry entry(&with_address);
  FunctionInfoContainer::const_iterator it =
      function_info_container_.find(entry);
  if (it != function_info_container_.end()) {
    FunctionInfo* function_info = **it;
    function_info_container_.erase(it);
    return function_info;
  }
  return NULL;
}

FunctionInfo* FunctionInfoMap::Find(uintptr_t address) {
  FunctionInfo with_address(address);
  Entry entry(&with_address);
  FunctionInfoContainer::iterator it = function_info_container_.find(entry);
  if (it != function_info_container_.end()) {
    return **it;
  }
  return NULL;
}

FunctionInfoMap::Entry::Entry(FunctionInfo* function_info)
    : function_info_(function_info) {
}

bool FunctionInfoMap::Entry::operator<(const Entry& that) const {
  return ***this < **that;
}

FunctionInfo* FunctionInfoMap::Entry::operator*() const {
  return function_info_;
}

}  // namespace trace
}  // namespace node
