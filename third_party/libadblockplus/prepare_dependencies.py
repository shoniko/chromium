import sys
import os
import shutil

def duplicate(src, dst, symlink=False):
  if (os.path.exists(dst)):
    if (os.path.isdir(dst)):
      print('Deleting %s' % dst)
      shutil.rmtree(dst);
    else:
      # symlink
      os.unlink(dst)
      
  dst_parent = os.path.abspath(os.path.join(dst, os.pardir))
  if not os.path.exists(dst_parent):
    print('Creating parent directory %s for %s' % (dst_parent, dst))
    os.makedirs(dst_parent)

  if symlink:
    print('Symlinking %s to %s' % (src, dst))
    os.symlink(src, dst)
  else:
    print('Copying %s to %s' % (src, dst))
    shutil.copytree(src, dst)

def prepare_deps():
  cwd = os.getcwd()
  libadblockplus_root = os.path.join(cwd, 'src', 'third_party', 'libadblockplus', 'third_party')
  
  # googletest
  # Chromium's googletest has a bit different directories structure ('src' as subdirectory)
  #googletest_src = os.path.join(cwd, 'src', 'third_party', 'googletest')
  #googletest_dst = os.path.join(libadblockplus_root, 'googletest')
  #duplicate(googletest_src, googletest_dst)

  # gyp
  # Chromium's gyp can't be used because of compile error:
  # *** No rule to make target `shell/src/Main.cpp', needed by `local/armeabi-v7a/objs/abpshell/shell/src/Main.o'.  Stop.
  # See. https://hg.adblockplus.org/gyp/rev/fix-issue-339
  #gyp_src = os.path.join(cwd, 'src', 'tools', 'gyp')
  #gyp_dst = os.path.join(libadblockplus_root, 'gyp')
  #duplicate(gyp_src, gyp_dst)

  # back-up /v8/testing/gtest as it will be deleted while preparing v8
  gtest_bak_src = os.path.join(libadblockplus_root, 'v8', 'testing', 'gtest')
  gtest_bak_dst = os.path.join(libadblockplus_root, 'gtest.bak')
  shutil.move(gtest_bak_src, gtest_bak_dst)

  # v8
  v8_src = os.path.join(cwd, 'src', 'v8')
  v8_dst = os.path.join(libadblockplus_root, 'v8')
  duplicate(v8_src, v8_dst)

  # restore /v8/testing/gtest from backup
  shutil.move(gtest_bak_dst, gtest_bak_src)

  # v8/base/trace_event/common
  trace_common_src = os.path.join(cwd, 'src', 'base', 'trace_event', 'common')
  trace_common_dst = os.path.join(libadblockplus_root, 'v8', 'base', 'trace_event', 'common')
  duplicate(trace_common_src, trace_common_dst)

  # v8/build
  build_src = os.path.join(cwd, 'src', 'build')
  build_dst = os.path.join(libadblockplus_root, 'v8', 'build')
  duplicate(build_src, build_dst)

  # v8/testing/gtest
  # Chromium's gtest can't be used becuase of compile error:
  # fatal error: third_party/googletest/src/googletest/include/gtest/gtest_prod.h: No such file or directory
  # #include "third_party/googletest/src/googletest/include/gtest/gtest_prod.h"
  # so we have to back-up gtest above and restore it after prepring of 'v8' directory
  #gtest_src = os.path.join(cwd, 'src', 'testing', 'gtest')
  #gtest_dst = os.path.join(libadblockplus_root, 'v8', 'testing', 'gtest')
  #duplicate(gtest_src, gtest_dst)

  # v8/tools/gyp
  tools_gyp_src = os.path.join(cwd, 'src', 'tools', 'gyp')
  tools_gyp_dst = os.path.join(libadblockplus_root, 'v8', 'tools', 'gyp')
  duplicate(tools_gyp_src, tools_gyp_dst)

  # v8/tools/clang
  clang_src = os.path.join(cwd, 'src', 'tools', 'clang')
  clang_dst = os.path.join(libadblockplus_root, 'v8', 'tools', 'clang')
  duplicate(clang_src, clang_dst)

  # v8/third_party/icu
  icu_src = os.path.join(cwd, 'src', 'third_party', 'icu')
  icu_dst = os.path.join(libadblockplus_root, 'v8', 'third_party', 'icu')
  duplicate(icu_src, icu_dst)

  # v8/third_party/jinja2
  jinja2_src = os.path.join(cwd, 'src', 'third_party', 'jinja2')
  jinja2_dst = os.path.join(libadblockplus_root, 'v8', 'third_party', 'jinja2')
  duplicate(jinja2_src, jinja2_dst)

  # v8/third_party/markupsafe
  markupsafe_src = os.path.join(cwd, 'src', 'third_party', 'markupsafe')
  markupsafe_dst = os.path.join(libadblockplus_root, 'v8', 'third_party', 'markupsafe')
  duplicate(markupsafe_src, markupsafe_dst)

  return 0  

def main(argv):
  return prepare_deps();

if '__main__' == __name__:
  try:
    sys.exit(main(sys.argv[1:]))
  except KeyboardInterrupt:
    sys.stderr.write('interrupted\n')
    sys.exit(1)