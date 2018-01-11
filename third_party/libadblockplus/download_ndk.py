import os
import urllib
import zipfile
import sys

def main(argv):
  # Download
  ndk_src = 'https://dl.google.com/android/repository/android-ndk-r12b-linux-x86_64.zip'

  cwd = os.getcwd()
  libadblockplus_root = os.path.join(cwd, 'src', 'third_party', 'libadblockplus', 'third_party')
  ndk_dst = os.path.join(libadblockplus_root, 'android-ndk-r12b-linux-x86_64.zip')

  if os.path.exists(ndk_dst):
    os.remove(ndk_dst)

  print('Downloading %s to %s' % (ndk_src, ndk_dst))
  urllib.urlretrieve(ndk_src, ndk_dst)

  # Extract zip (preserving file permissions)
  print('Extracting %s to %s' % (ndk_dst, libadblockplus_root))
  with zipfile.ZipFile(ndk_dst, 'r') as zf:
    for info in zf.infolist():
      zf.extract(info.filename, path=libadblockplus_root)
      out_path = os.path.join(libadblockplus_root, info.filename)

      perm = info.external_attr >> 16L
      os.chmod(out_path, perm)

  # Delete zip
  os.remove(ndk_dst)

  return 0

if '__main__' == __name__:
  try:
    sys.exit(main(sys.argv[1:]))
  except KeyboardInterrupt:
    sys.stderr.write('interrupted\n')
    sys.exit(1)