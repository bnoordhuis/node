#!/usr/bin/env python

from os import path
import json
import pycparser
import re
import subprocess
import sys

toplevel_dir = path.realpath(path.join(path.dirname(__file__), '../..'))
header_dir = path.join(toplevel_dir, 'openssl/include')

include_dirs = 'openssl/include config/piii tools/scan/include'.split()
include_dirs = [path.join(toplevel_dir, s) for s in include_dirs]
includes = ['-I%s' % s for s in include_dirs]

# this should mirror openssl.gyp
defines = ['-D%s' % s for s in '''
L_ENDIAN
OPENSSL_THREADS
PURIFY
_REENTRANT
OPENSSL_NO_DGRAM
OPENSSL_NO_DTLS1
OPENSSL_NO_SCTP
OPENSSL_NO_SOCK
OPENSSL_NO_RDRAND
OPENSSL_NO_GOST
OPENSSL_NO_HW_PADLOCK
OPENSSL_NO_TTY
'''.split()]

header_files = '''
openssl/aes.h openssl/asn1.h openssl/asn1_mac.h openssl/asn1t.h openssl/bio.h
openssl/blowfish.h openssl/bn.h openssl/buffer.h openssl/cast.h openssl/cmac.h
openssl/comp.h openssl/conf_api.h openssl/conf.h openssl/crypto.h openssl/des.h
openssl/dh.h openssl/dsa.h openssl/dso.h openssl/ebcdic.h openssl/ecdh.h
openssl/ecdsa.h openssl/ec.h openssl/engine.h openssl/e_os2.h openssl/err.h
openssl/evp.h openssl/hmac.h openssl/kssl.h openssl/lhash.h openssl/md2.h
openssl/md4.h openssl/md5.h openssl/modes.h openssl/objects.h openssl/obj_mac.h
openssl/ocsp.h openssl/opensslv.h openssl/ossl_typ.h openssl/pem2.h
openssl/pem.h openssl/pkcs12.h openssl/pkcs7.h openssl/pqueue.h openssl/rand.h
openssl/rc2.h openssl/rc4.h openssl/ripemd.h openssl/rsa.h openssl/safestack.h
openssl/sha.h openssl/srp.h openssl/ssl23.h openssl/ssl2.h openssl/ssl.h
openssl/stack.h openssl/store.h openssl/symhacks.h openssl/tls1.h openssl/ts.h
openssl/txt_db.h openssl/ui_compat.h openssl/ui.h openssl/whrlpool.h
openssl/x509.h openssl/x509v3.h openssl/x509_vfy.h
'''.split()


class FuncDeclVisitor(pycparser.c_ast.NodeVisitor):
  def __init__(self, callback):
    self.callback = callback

  def visit_FuncDecl(self, node):
    if not isinstance(node.type, pycparser.c_ast.TypeDecl): return
    self.callback(node.type.declname)


def cpp(source):
  args = ['cpp', '-P', '-nostdinc', '-D__attribute__(x)='] + defines + includes
  proc = subprocess.Popen(args,
                          stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
  (stdout, stderr) = proc.communicate(input=source)
  if stderr: raise RuntimeError(stderr)
  return stdout


def scan(header_files, stream=sys.stdout):
  source = '\n'.join('#include <%s>' % s for s in header_files)
  source = cpp(source)
  #dump(source)
  parser = pycparser.CParser()
  ast = parser.parse(source, '<generated>')
  #ast.show()
  names = []
  FuncDeclVisitor(names.append).visit(ast)
  stream.write('\n'.join(names))


def dump(source, stream=sys.stderr):
  lines = ('% 3d  %s' % (k + 1, v) for k, v in enumerate(source.split('\n')))
  stream.write('\n'.join(lines))


if __name__ == '__main__': scan(header_files)
