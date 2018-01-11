import os
import urllib
import zipfile
import sys
import subprocess

def install(title, package, version, verbose=False):
  print('Installing %s ...' % title)

  cwd = os.getcwd()
  sdk_root = os.path.join(cwd, 'src', 'third_party', 'android_tools', 'sdk')
  sdkmanager = os.path.join(sdk_root, 'tools', 'bin', 'sdkmanager')

  args = [
    sdkmanager,
    "--sdk_root=%s" % (sdk_root),
    "%s;%s" % (package, version)
  ]

  if verbose:
    args += [ "--verbose" ]

  process = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=sys.stdout, stderr=sys.stderr)
  process.stdin.write('y') # Agree to License
  process.communicate()
  process.stdin.close()

  if process.returncode != 0:
    print("%s finished with error code %s" % (args, process.returncode))

  return process.returncode

def main(argv):
  # TODO: update when different version of built-tools and platform are required
  bt25 = install("Build tools", "build-tools", "25.0.0", True)
  if bt25 != 0:
    return bt25

  a16 = install("Platform 16", "platforms", "android-16", True)
  if a16 != 0:
    return a16

  a21 = install("Platform 21", "platforms", "android-21", True)
  if a21 != 0:
    return a21

  return 0

if '__main__' == __name__:
  try:
    sys.exit(main(sys.argv[1:]))
  except KeyboardInterrupt:
    sys.stderr.write('interrupted\n')
    sys.exit(1)