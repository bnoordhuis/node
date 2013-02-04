#!/usr/bin/env python

# A target install path that is a directory MUST end with a slash ('/').
# Omitting the slash will result in badly botched installs, to say nothing
# of uninstalls.  You don't want to be the guy that ends up on Hacker News
# because `sudo make uninstall` deleted someone's root file system.

try:
  import json
except ImportError:
  import simplejson as json

import errno
import os
import re
import shutil
import sys

# set at init time
destdir = None
node_prefix = None
node_libdir = None
target_defaults = None
variables = None


def load_config():
  s = open('config.gypi').read()
  s = re.sub(r'#.*?\n', '', s) # strip comments
  s = re.sub(r'\'', '"', s) # convert quotes
  return json.loads(s)


def abspath(*args):
  return os.path.abspath(destdir + os.path.join(*args))


def target_filename(source_path, prefix, target_path):
  path = abspath(prefix, target_path)

  if target_path.endswith('/'):
    path = os.path.join(path,
                        os.path.basename(source_path))

  return path


def try_unlink(source_path, prefix, target_path):
  path = target_filename(source_path, prefix, target_path)
  try:
    os.unlink(path)
  except OSError, e:
    if e.errno != errno.ENOENT: raise


def try_symlink(source_path, prefix, target_path):
  path = abspath(prefix, target_path)
  try_unlink(source_path, prefix, target_path)
  os.symlink(source_path, path)


def try_mkdir_r(prefix, target_path):
  path = abspath(prefix, target_path)
  try:
    os.makedirs(path)
  except OSError, e:
    if e.errno != errno.EEXIST: raise


def try_rmdir_r(prefix, target_path):
  path = abspath(prefix, target_path)
  root = abspath('/')
  while path > root:
    try:
      os.rmdir(path)
    except OSError, e:
      if e.errno == errno.ENOTEMPTY: return
      if e.errno == errno.ENOENT: return
      raise
    # Move up to the parent directory.
    path = os.path.join(path, '..')
    path = os.path.abspath(path)


def try_copy(source_path, prefix, target_path):
  path = abspath(prefix, target_path)
  print 'installing %s' % target_filename(source_path, prefix, target_path)
  try_mkdir_r(prefix,
              os.path.dirname(target_path))
  try_unlink(source_path, prefix, target_path)
  shutil.copy2(source_path,
               abspath(prefix, target_path))


def try_remove(source_path, prefix, target_path):
  path = abspath(prefix, target_path)
  print 'removing %s from %s' % (source_path, target_path)
  try_unlink(source_path, prefix, target_path)
  try_rmdir_r(prefix,
              os.path.dirname(target_path))


def install(paths, prefix, target_path):
  for path in paths:
    try_copy(path, prefix, target_path)


def uninstall(paths, prefix, target_path):
  for path in paths:
    try_remove(path, prefix, target_path)


def waf_files(action):
  action(['tools/node-waf'], node_prefix, 'bin/node-waf')
  action(['tools/wafadmin/ansiterm.py',
          'tools/wafadmin/Build.py',
          'tools/wafadmin/Configure.py',
          'tools/wafadmin/Constants.py',
          'tools/wafadmin/Environment.py',
          'tools/wafadmin/__init__.py',
          'tools/wafadmin/Logs.py',
          'tools/wafadmin/Node.py',
          'tools/wafadmin/Options.py',
          'tools/wafadmin/pproc.py',
          'tools/wafadmin/py3kfixes.py',
          'tools/wafadmin/Runner.py',
          'tools/wafadmin/Scripting.py',
          'tools/wafadmin/TaskGen.py',
          'tools/wafadmin/Task.py',
          'tools/wafadmin/Utils.py'],
          node_libdir, 'node/wafadmin/')
  action(['tools/wafadmin/Tools/ar.py',
          'tools/wafadmin/Tools/cc.py',
          'tools/wafadmin/Tools/ccroot.py',
          'tools/wafadmin/Tools/compiler_cc.py',
          'tools/wafadmin/Tools/compiler_cxx.py',
          'tools/wafadmin/Tools/compiler_d.py',
          'tools/wafadmin/Tools/config_c.py',
          'tools/wafadmin/Tools/cxx.py',
          'tools/wafadmin/Tools/dmd.py',
          'tools/wafadmin/Tools/d.py',
          'tools/wafadmin/Tools/gas.py',
          'tools/wafadmin/Tools/gcc.py',
          'tools/wafadmin/Tools/gdc.py',
          'tools/wafadmin/Tools/gnu_dirs.py',
          'tools/wafadmin/Tools/gob2.py',
          'tools/wafadmin/Tools/gxx.py',
          'tools/wafadmin/Tools/icc.py',
          'tools/wafadmin/Tools/icpc.py',
          'tools/wafadmin/Tools/__init__.py',
          'tools/wafadmin/Tools/intltool.py',
          'tools/wafadmin/Tools/libtool.py',
          'tools/wafadmin/Tools/misc.py',
          'tools/wafadmin/Tools/nasm.py',
          'tools/wafadmin/Tools/node_addon.py',
          'tools/wafadmin/Tools/osx.py',
          'tools/wafadmin/Tools/preproc.py',
          'tools/wafadmin/Tools/python.py',
          'tools/wafadmin/Tools/suncc.py',
          'tools/wafadmin/Tools/suncxx.py',
          'tools/wafadmin/Tools/unittestw.py',
          'tools/wafadmin/Tools/winres.py',
          'tools/wafadmin/Tools/xlc.py',
          'tools/wafadmin/Tools/xlcxx.py'],
          node_libdir, 'node/wafadmin/Tools/')


def update_shebang(filename, shebang):
  print 'updating shebang of %s to %s' % (filename, shebang)
  s = open(filename, 'r').read()
  s = re.sub(r'#!.*\n', '#!' + shebang + '\n', s)
  open(filename, 'w').write(s)


def npm_files(action):
  target_path = 'node_modules/npm/'

  # don't install npm if the target path is a symlink, it probably means
  # that a dev version of npm is installed there
  if os.path.islink(abspath(node_libdir, target_path)): return

  # npm has a *lot* of files and it'd be a pain to maintain a fixed list here
  # so we walk its source directory instead...
  for dirname, subdirs, basenames in os.walk('deps/npm', topdown=True):
    subdirs[:] = filter('test'.__ne__, subdirs) # skip test suites
    paths = [os.path.join(dirname, basename) for basename in basenames]
    action(paths,
           node_libdir,
           target_path + dirname[9:] + '/')

  # create/remove symlink
  link_path = 'bin/npm'
  if action == uninstall:
    action([link_path], node_prefix, 'bin/npm')
  elif action == install:
    source_path = os.path.join(node_libdir,
                               'node_modules/npm/bin/npm-cli.js')
    try_symlink(source_path,
                node_prefix,
                link_path)

    if os.environ.get('PORTABLE'):
      # This crazy hack is necessary to make the shebang execute the copy
      # of node relative to the same directory as the npm script. The precompiled
      # binary tarballs use a prefix of "/" which gets translated to "/bin/node"
      # in the regular shebang modifying logic, which is incorrect since the
      # precompiled bundle should be able to be extracted anywhere and "just work"
      shebang = '/bin/sh\n// 2>/dev/null; exec "`dirname "$0"`/node" "$0" "$@"'
    else:
      shebang = os.path.join(node_prefix, 'bin/node')

    filename = os.path.join(destdir + source_path)
    update_shebang(filename, shebang)
  else:
    assert(0) # unhandled action type


def files(action):
  action(['deps/uv/include/ares.h',
          'deps/uv/include/ares_version.h',
          'deps/uv/include/uv.h',
          'deps/v8/include/v8-debug.h',
          'deps/v8/include/v8-preparser.h',
          'deps/v8/include/v8-profiler.h',
          'deps/v8/include/v8-testing.h',
          'deps/v8/include/v8.h',
          'deps/v8/include/v8stdint.h',
          'src/eio-emul.h',
          'src/ev-emul.h',
          'src/node.h',
          'src/node_buffer.h',
          'src/node_object_wrap.h',
          'src/node_version.h'],
          node_prefix, 'include/node/')
  action(['deps/uv/include/uv-private/eio.h',
          'deps/uv/include/uv-private/ev.h',
          'deps/uv/include/uv-private/ngx-queue.h',
          'deps/uv/include/uv-private/tree.h',
          'deps/uv/include/uv-private/uv-unix.h',
          'deps/uv/include/uv-private/uv-win.h'],
          node_prefix, 'include/node/uv-private/')
  action(['out/Release/node'], node_prefix, 'bin/node')

  # install unconditionally, checking if the platform supports dtrace doesn't
  # work when cross-compiling and besides, there's at least one linux flavor
  # with dtrace support now (oracle's "unbreakable" linux)
  action(['src/node.d'], node_libdir, 'dtrace/')

  if 'freebsd' in sys.platform or 'openbsd' in sys.platform:
    action(['doc/node.1'], node_prefix, 'man/man1/')
  else:
    action(['doc/node.1'], node_prefix, 'share/man/man1/')

  if 'true' == variables.get('node_install_waf'): waf_files(action)
  if 'true' == variables.get('node_install_npm'): npm_files(action)


def run(args):
  global destdir
  global node_prefix
  global node_libdir
  global target_defaults
  global variables

  # chdir to the project's top-level directory
  os.chdir(os.path.join(os.path.dirname(__file__), '..'))

  conf = load_config()
  variables = conf['variables']
  target_defaults = conf['target_defaults']

  node_prefix = variables.get('node_prefix') or '/usr/local'
  node_libdir = variables.get('node_libdir') or os.path.join(node_prefix, 'lib')

  # argv[2] is a custom install prefix for packagers (think DESTDIR)
  destdir = os.path.abspath(args[2]) if args[2:3] and args[2] else ''
  cmd = args[1] if args[1:2] else 'install'

  if cmd == 'install': return files(install)
  if cmd == 'uninstall': return files(uninstall)
  raise RuntimeError('Bad command: %s\n' % cmd)


if __name__ == '__main__':
  run(sys.argv[:])
