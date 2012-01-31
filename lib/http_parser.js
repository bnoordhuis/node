'use strict';

var REQUEST  = exports.REQUEST  = 1;
var RESPONSE = exports.RESPONSE = 2;

var n = 1;
var M_CHECKOUT    = exports.M_CHECKOUT    = n++;
var M_CONNECT     = exports.M_CONNECT     = n++;
var M_COPY        = exports.M_COPY        = n++;
var M_DELETE      = exports.M_DELETE      = n++;
var M_GET         = exports.M_GET         = n++;
var M_HEAD        = exports.M_HEAD        = n++;
var M_LOCK        = exports.M_LOCK        = n++;
var M_MERGE       = exports.M_MERGE       = n++;
var M_MKACTIVITY  = exports.M_MKACTIVITY  = n++;
var M_MKCOL       = exports.M_MKCOL       = n++;
var M_MOVE        = exports.M_MOVE        = n++;
var M_MSEARCH     = exports.M_MSEARCH     = n++;
var M_NOTIFY      = exports.M_NOTIFY      = n++;
var M_OPTIONS     = exports.M_OPTIONS     = n++;
var M_PATCH       = exports.M_PATCH       = n++;
var M_POST        = exports.M_POST        = n++;
var M_PROPFIND    = exports.M_PROPFIND    = n++;
var M_PROPPATCH   = exports.M_PROPPATCH   = n++;
var M_PUT         = exports.M_PUT         = n++;
var M_REPORT      = exports.M_REPORT      = n++;
var M_SUBSCRIBE   = exports.M_SUBSCRIBE   = n++;
var M_TRACE       = exports.M_TRACE       = n++;
var M_UNLOCK      = exports.M_UNLOCK      = n++;
var M_UNSUBSCRIBE = exports.M_UNSUBSCRIBE = n++;

n = 0;
var E_OK                     = exports.E_OK                     = ++n;
var E_CB_message_begin       = exports.E_CB_message_begin       = ++n;
var E_CB_path                = exports.E_CB_path                = ++n;
var E_CB_query_string        = exports.E_CB_query_string        = ++n;
var E_CB_url                 = exports.E_CB_url                 = ++n;
var E_CB_fragment            = exports.E_CB_fragment            = ++n;
var E_CB_header_field        = exports.E_CB_header_field        = ++n;
var E_CB_header_value        = exports.E_CB_header_value        = ++n;
var E_CB_headers_complete    = exports.E_CB_headers_complete    = ++n;
var E_CB_body                = exports.E_CB_body                = ++n;
var E_CB_message_complete    = exports.E_CB_message_complete    = ++n;
var E_INVALID_EOF_STATE      = exports.E_INVALID_EOF_STATE      = ++n;
var E_HEADER_OVERFLOW        = exports.E_HEADER_OVERFLOW        = ++n;
var E_CLOSED_CONNECTION      = exports.E_CLOSED_CONNECTION      = ++n;
var E_INVALID_VERSION        = exports.E_INVALID_VERSION        = ++n;
var E_INVALID_STATUS         = exports.E_INVALID_STATUS         = ++n;
var E_INVALID_METHOD         = exports.E_INVALID_METHOD         = ++n;
var E_INVALID_URL            = exports.E_INVALID_URL            = ++n;
var E_INVALID_HOST           = exports.E_INVALID_HOST           = ++n;
var E_INVALID_PORT           = exports.E_INVALID_PORT           = ++n;
var E_INVALID_PATH           = exports.E_INVALID_PATH           = ++n;
var E_INVALID_QUERY_STRING   = exports.E_INVALID_QUERY_STRING   = ++n;
var E_INVALID_FRAGMENT       = exports.E_INVALID_FRAGMENT       = ++n;
var E_LF_EXPECTED            = exports.E_LF_EXPECTED            = ++n;
var E_INVALID_HEADER_TOKEN   = exports.E_INVALID_HEADER_TOKEN   = ++n;
var E_INVALID_CONTENT_LENGTH = exports.E_INVALID_CONTENT_LENGTH = ++n;
var E_INVALID_CHUNK_SIZE     = exports.E_INVALID_CHUNK_SIZE     = ++n;
var E_INVALID_CONSTANT       = exports.E_INVALID_CONSTANT       = ++n;
var E_INVALID_INTERNAL_STATE = exports.E_INVALID_INTERNAL_STATE = ++n;
var E_STRICT                 = exports.E_STRICT                 = ++n;
var E_PAUSED                 = exports.E_PAUSED                 = ++n;
var E_UNKNOWN                = exports.E_UNKNOWN                = ++n;

n = 1;
var s_dead                    = n++;
var s_start_req_or_res        = n++;
var s_res_or_resp_H           = n++;
var s_start_res               = n++;
var s_res_H                   = n++;
var s_res_HT                  = n++;
var s_res_HTT                 = n++;
var s_res_HTTP                = n++;
var s_res_first_http_major    = n++;
var s_res_http_major          = n++;
var s_res_first_http_minor    = n++;
var s_res_http_minor          = n++;
var s_res_first_status_code   = n++;
var s_res_status_code         = n++;
var s_res_status              = n++;
var s_res_line_almost_done    = n++;
var s_start_req               = n++;
var s_req_method              = n++;
var s_req_spaces_before_url   = n++;
var s_req_schema              = n++;
var s_req_schema_slash        = n++;
var s_req_schema_slash_slash  = n++;
var s_req_host                = n++;
var s_req_port                = n++;
var s_req_path                = n++;
var s_req_query_string_start  = n++;
var s_req_query_string        = n++;
var s_req_fragment_start      = n++;
var s_req_fragment            = n++;
var s_req_http_start          = n++;
var s_req_http_H              = n++;
var s_req_http_HT             = n++;
var s_req_http_HTT            = n++;
var s_req_http_HTTP           = n++;
var s_req_first_http_major    = n++;
var s_req_http_major          = n++;
var s_req_first_http_minor    = n++;
var s_req_http_minor          = n++;
var s_req_line_almost_done    = n++;
var s_header_field_start      = n++;
var s_header_field            = n++;
var s_header_value_start      = n++;
var s_header_value            = n++;
var s_header_value_lws        = n++;
var s_header_almost_done      = n++;
var s_chunk_size_start        = n++;
var s_chunk_size              = n++;
var s_chunk_parameters        = n++;
var s_chunk_size_almost_done  = n++;
var s_headers_almost_done     = n++;
var s_headers_done            = n++;
var s_chunk_data              = n++;
var s_chunk_data_almost_done  = n++;
var s_chunk_data_done         = n++;
var s_body_identity           = n++;
var s_body_identity_eof       = n++;
var s_message_done            = n++;

var CR = 13; // \r
var LF = 10; // \n
var SP = 32; // space

// 256 bits bitmap
var normal_url_chars = new Buffer([
  // chars 0-127
  0, 0, 0, 0, 111, 255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 254,
  // chars 128-255
  0, 0, 0, 0, 0, 0, 0, 0
]);

var methods = {};
methods[M_DELETE]      = new Buffer('DELETE');
methods[M_GET]         = new Buffer('GET');
methods[M_HEAD]        = new Buffer('HEAD');
methods[M_POST]        = new Buffer('POST');
methods[M_PUT]         = new Buffer('PUT');
methods[M_CONNECT]     = new Buffer('CONNECT');
methods[M_OPTIONS]     = new Buffer('OPTIONS');
methods[M_TRACE]       = new Buffer('TRACE');
methods[M_COPY]        = new Buffer('COPY');
methods[M_LOCK]        = new Buffer('LOCK');
methods[M_MKCOL]       = new Buffer('MKCOL');
methods[M_MOVE]        = new Buffer('MOVE');
methods[M_PROPFIND]    = new Buffer('PROPFIND');
methods[M_PROPPATCH]   = new Buffer('PROPPATCH');
methods[M_UNLOCK]      = new Buffer('UNLOCK');
methods[M_REPORT]      = new Buffer('REPORT');
methods[M_MKACTIVITY]  = new Buffer('MKACTIVITY');
methods[M_CHECKOUT]    = new Buffer('CHECKOUT');
methods[M_MERGE]       = new Buffer('MERGE');
methods[M_MSEARCH]     = new Buffer('M-SEARCH');
methods[M_NOTIFY]      = new Buffer('NOTIFY');
methods[M_SUBSCRIBE]   = new Buffer('SUBSCRIBE');
methods[M_UNSUBSCRIBE] = new Buffer('UNSUBSCRIBE');
methods[M_PATCH]       = new Buffer('PATCH');


function UNREACHABLE() {
  throw new Error('UNREACHABLE CODE REACHED');
}


function isdigit(c) {
  return (c >= 48 && c <= 57); // 0-9
}


function isxdigit(c) {
  // 0-9 || A-Z || a-z
  return (c >= 48 && c <= 57) || (c >= 65 && c <= 70) || (c >= 97 && c <= 102);
}


function isalpha(c) {
  return (c >= 65 && c <= 90) || (c >= 97 && c <= 122); // A-Z || a-z
}


function isalnum(c) {
  return isalpha(c) || isdigit(c);
}


function is_host_char(c) {
  return alnum(c) || c === 45 || c === 46; // alnum || '-' || '.'
}


function is_url_char(c) {
  return normal_url_chars[c >> 3] & (1 << (c & 7));
}


function dec2num(c) {
  if (c >= 48 && c <= 57) return c - 48;  // 0-9
  UNREACHABLE();
}


function hex2num(c) {
  // TODO use lookup table? benchmark it first
  if (c >= 48 && c <= 57) return c - 48;  // 0-9
  if (c >= 65 && c <= 70) return c - 65;  // A-F
  if (c >= 97 && c <= 102) return c - 97; // a-F
  UNREACHABLE();
}


function add_dec(v, c) {
  // FIXME add overflow check
  return (v * 10) + dec2num(c);
}


function add_hex(v, c) {
  // FIXME add overflow check
  return (v * 16) + hex2num(c);
}


function parse_url_char(state, c, is_connect) {
  switch (state) {
  case s_req_spaces_before_url:
    if (c === 47 || c === 42) return s_req_path; // '/' || '*'
    if (is_connect && isalnum(c)) return s_req_host;
    if (isalpha(c)) return s_req_schema;
    return s_dead;

  case s_req_schema:
    if (isalpha(c)) return state;
    if (c === 58) return s_req_schema_slash; // ':'
    return s_dead;

  case s_req_schema_slash:
    if (c === 47) return s_req_schema_slash_slash; // '/'
    return s_dead;

  case s_req_schema_slash_slash:
    if (c === 47) return s_req_host; // '/'
    return s_dead;

  case s_req_host:
    if (is_host_char(c)) return state;
    if (c === 58) return s_req_port; // ':'
    if (c === 47) return s_req_path; // '/'
    if (c === 63) return s_req_query_string_start; // '?'
    return s_dead;

  case s_req_port:
    if (isdigit(c)) return state;
    if (c === 47) return s_req_path; // '/'
    if (c === 63) return s_req_query_string_start; // '?'
    return s_dead;

  case s_req_path:
    if (is_url_char(c)) return state;
    if (c === 35) return s_req_fragment_start; // '#'
    if (c === 63) return s_req_query_string_start; // '?'
    return s_dead;

  case s_req_query_string_start:
    if (is_url_char(c)) return s_req_query_string;
    if (c === 35) return s_req_fragment_start; // '#'
    if (c === 63) return state; // '?'
    return s_dead;

  case s_req_query_string:
    if (is_url_char(c)) return state;
    if (c === 35) return s_req_fragment_start; // '#'
    if (c === 63) return state; // '?'
    return s_dead;

  case s_req_fragment_start:
    if (is_url_char(c)) return s_req_fragment;
    if (c === 35) return state; // '#'
    if (c === 63) return s_req_fragment_start; // '?'
    return s_dead;

  case s_req_fragment:
    if (is_url_char(c)) return state;
    if (ch === 35) return state; // '#'
    if (ch === 63) return state; // '?'
    return s_dead;
  }

  UNREACHABLE();
}


function Parser(type) {
  this.init(type);
}
exports.Parser = Parser;


Parser.prototype.init = function(type) {
  this.type = type;
  this.index = 0;
  this.errno = E_OK;
  this.state = type == REQUEST ? s_start_req : s_start_res;
  this.contentLength = 0;
};


Parser.prototype.feed = function(data, offset, length) {
  var state = this.state;
  var index = this.index;
  var mark = offset;

  var i = offset, k = offset + length;

error:
  for (; i < k; ++i, ++index) {
    var c = data[i];

    switch (state) {
    case s_dead:
      if (c == CR || c == LF) break;
      this.errno = E_CLOSED_CONNECTION;
      break;

    case s_start_req:
      if (c == CR || c == LF) break;

      switch (c) {
      case 67: this.method = M_CONNECT; break; // COPY, CHECKOUT
      case 68: this.method = M_DELETE; break;
      case 71: this.method = M_GET; break;
      case 72: this.method = M_HEAD; break;
      case 76: this.method = M_LOCK; break;
      case 77: this.method = M_MKCOL; break; // MOVE, MERGE, etc.
      case 78: this.method = M_NOTIFY; break;
      case 79: this.method = M_OPTIONS; break;
      case 80: this.method = M_POST; break; // PUT, PATCH, PROPFIND, etc.
      case 82: this.method = M_REPORT; break;
      case 83: this.method = M_SUBSCRIBE; break;
      case 84: this.method = M_TRACE; break;
      case 85: this.method = M_UNLOCK; break;
      default: this.errno = E_INVALID_METHOD; break error;
      }

      state = s_req_method;
      break;

    case s_req_method:
    {
      var method = this.method;
      var method_name = methods[method];

      if (c === SP && index === method_name.length) {
        state = s_req_spaces_before_url;
        mark = 0;
      }
      else if (c === method_name[index]) {
        // ok, carry on
      }
      else if (method === M_CONNECT) {
        if      (index === 1 && c === 72) method = this.method = M_CHECKOUT;
        else if (index === 2 && c === 80) method = this.method = M_COPY;
        else                              break error;
      }
      else if (method === M_MKCOL) {
        if      (index === 1 && c === 79) method = this.method = M_MOVE;
        else if (index === 1 && c === 69) method = this.method = M_MERGE;
        else if (index === 1 && c === 45) method = this.method = M_MSEARCH;
        else if (index === 2 && c === 65) method = this.method = M_MKACTIVITY;
        else                              break error;
      }
      else if (index === 1 && method === M_POST) {
        if      (c === 65) method = this.method = M_PATCH;
        else if (c === 82) method = this.method = M_PROPFIND;
        else if (c === 85) method = this.method = M_PUT;
        else               break error;
      }
      else if (index === 2 && method === M_UNLOCK && c === 83) {
        method = this.method = M_UNSUBSCRIBE;
      }
      else if (index === 2 && method === M_PROPFIND && c === 80) {
        method = this.method = M_PROPPATCH;
      }
      else {
        this.errno = E_INVALID_METHOD;
        break error;
      }

      break;
    }

    case s_req_spaces_before_url:
      if (c === SP) break;

      if (!mark) mark = i;

      if (state === s_dead) {
        this.errno = E_INVALID_URL;
        break error;
      }

      break;
    }
  }

  if (i < k) state = s_dead;
  this.state = state;

  return state !== s_dead;
};
