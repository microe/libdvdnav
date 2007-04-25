if test "$dvdread" = "external"; then
    dvdreadlib="-ldvdread"
    dvdreadmsg="[--minilibs]"
fi

usage()
{
	cat <<EOF
Usage: dvdnav-config [OPTIONS] [LIBRARIES]
Options:
	[--prefix[=DIR]]
	[--version]
        [--libs]
	[--cflags]
        $dvdreadmsg
EOF
	exit $1
}

if test $# -eq 0; then
	usage 1 1>&2
fi

while test $# -gt 0; do
  case "$1" in
  -*=*) optarg=`echo "$1" | sed 's/[-_a-zA-Z0-9]*=//'` ;;
  *) optarg= ;;
  esac

  case $1 in
    --prefix)
      echo_prefix=yes
      ;;
    --version)
      echo $version
      ;;
    --cflags)
      echo_cflags=yes
      ;;
    --libs)
      echo_libs=yes
      ;;
    --minilibs)
      if test "$dvdread" = "external"; then
          echo_minilibs=yes
      else
          usage 1 1>&2
      fi
      ;;
    *)
      usage 1 1>&2
      ;;
  esac
  shift
done

if test "$echo_prefix" = "yes"; then
	echo $prefix
fi

if test "$echo_cflags" = "yes"; then
      echo -I$prefix/include -I$prefix/include/dvdnav $threadcflags
fi

if test "$echo_libs" = "yes"; then
      echo -L$prefix/lib -ldvdnav $dvdreadlib $threadlib
fi      

if test "$echo_minilibs" = "yes"; then
      echo -L$prefix/lib -ldvdnav $threadlib
fi
