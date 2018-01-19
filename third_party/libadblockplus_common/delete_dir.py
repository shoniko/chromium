import os
import sys
import shutil


def main(argv):
    directory = argv[0]
    if os.path.exists(directory):
        shutil.rmtree(directory)

    return 0


if '__main__' == __name__:
    try:
        sys.exit(main(sys.argv[1:]))
    except KeyboardInterrupt:
        sys.stderr.write('interrupted\n')
        sys.exit(1)
