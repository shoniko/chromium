from __future__ import print_function

import sys
import os
import shutil


def duplicate(src, dst, symlink=False):
    if os.path.exists(dst):
        if os.path.isdir(dst):
            print('Deleting {}'.format(dst))
            shutil.rmtree(dst)
        else:
            # symlink
            os.unlink(dst)

    dst_parent = os.path.abspath(os.path.join(dst, os.pardir))
    if not os.path.exists(dst_parent):
        print('Creating parent directory {} for {}'.format(dst_parent, dst))
        os.makedirs(dst_parent)

    if symlink:
        print('Symlinking {} to {}'.format(src, dst))
        os.symlink(src, dst)
    else:
        print('Copying {} to {}'.format(src, dst))
        shutil.copytree(src, dst)


def prepare_deps():
    cwd = os.getcwd()
    libadblockplus_third_party = os.path.join(cwd,
                                              'src', 'third_party',
                                              'libadblockplus', 'third_party')

    # googletest
    #
    # Chromium's googletest has a bit different directories structure
    # ('src' as subdirectory)

    # gyp
    #
    # Chromium's gyp can't be used because of compile error:
    # *** No rule to make target `shell/src/Main.cpp',
    # needed by `local/armeabi-v7a/objs/abpshell/shell/src/Main.o'.  Stop.
    # See. https://hg.adblockplus.org/gyp/rev/fix-issue-339

    # back-up /v8/testing/gtest as it will be deleted while preparing v8
    gtest_bak_src = os.path.join(libadblockplus_third_party,
                                 'v8', 'testing', 'gtest')
    gtest_bak_dst = os.path.join(libadblockplus_third_party, 'gtest.bak')
    shutil.move(gtest_bak_src, gtest_bak_dst)

    # v8
    v8_src = os.path.join(cwd, 'src', 'v8')
    v8_dst = os.path.join(libadblockplus_third_party, 'v8')
    duplicate(v8_src, v8_dst)

    # restore /v8/testing/gtest from backup
    shutil.move(gtest_bak_dst, gtest_bak_src)

    # v8/testing/gtest
    #
    # Chromium's gtest can't be used becuase of compile error:
    # fatal error: third_party/googletest/src/googletest/include/gtest/gtest_prod.h: No such file or directory
    # #include "third_party/googletest/src/googletest/include/gtest/gtest_prod.h"
    # so we have to back-up gtest above and restore it
    # after prepring of 'v8' directory

    for path in [
        ('base', 'trace_event', 'common'),
        ('build',),
        ('tools', 'gyp'),
        ('tools', 'clang'),
        ('third_party', 'icu'),
        ('third_party' , 'jinja2'),
        ('third_party', 'markupsafe')  
    ]:
        src = os.path.join(cwd, 'src', *path)
        dst = os.path.join(libadblockplus_third_party, 'v8', *path)
        duplicate(src, dst)


if __name__ == '__main__':
    try:
        prepare_deps()
    except KeyboardInterrupt:
        sys.exit('interrupted')
