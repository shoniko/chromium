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
                                              'libadblockplus', 'src',
                                              'third_party')

    # Actually one should use the version of googletest specified in the DEPS file
    # of the chromium's V8, however since it's not checked out into chromium source
    # tree and that version luckily is the same as in libadblockplus' V8,
    # for the sake of simplicity we create a temporary copy of already checked out
    # googletest and restore it after preparing the version of chromium's V8.
    # After changing of a chromium's version it may not work, we will likely have
    # to check out googletest of another version.

    # Chromium code does not fetch V8 deps because in chromium project the
    # dependencies are taken from a root directory of a parent root project (see
    # v8/BUILD.gn), so in order to build V8 we have to copy them because we are
    # still using gyp and gyp does not support such tricks, and even if it supported
    # libadblockplus Makefile does not support it right now either.

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
