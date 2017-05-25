#! /bin/sh

set -e

minify=
embed=

while echo "$1" | grep '^-'; do
  if [ "$1" = "-m" ]; then
    minify=1
  fi
  if [ "$1" = "-e" ]; then
    embed=1
  fi
  shift
fi

do_clean() {
  infile="$1"
  if [ -z "$2" ]; then
    outfile="$1"
  else
    outfile="$2"
  fi
  tmpfile="${outfile}~"

  if [ "$minify" = "1" ]; then
    cat "$infile" | sed -e 's/^ *//g' | sed -e 's/ *$//' | tr -d "\r\n" > "$tmpfile"
    mv "$tmpfile" "$outfile"
  elif [ "$infile" != "$outfile" ]; then
    cp "$infile" "$outfile"
  fi

  if[ "$embed" = "1" ]; then
    cat "$outfile" | sed -e 's/\(<?[^>]*>\)\?<resource[^>]*>//g' | sed -e 's/<\/resource>//g' > "$tmpfile"
    mv "$tmpfile" "$outfile"
  fi

  # Remove newlines and lines containing only spaces from all given files
  # Remove empty elements
  cat "$outfile" | sed -e "s/ *$//" | sed -e "/^$/d" \
    | sed -e 's/<\([[:alnum:]]\+\) *><\/\1>//g' \
    | sed -e 's/<\([[:alnum:]]\+\) *\( [^>]\+\)" *><\/\1>/<\1\2"\/>/g' \
    | sed -e "/^ *$/d" \
    > "$tmpfile"
  mv "$tmpfile" "$outfile"
 
  if [ "$minify" != "1" ]; then
    if uname | grep -i 'MINGW\|CYGWIN' > /dev/null; then
      unix2dos "$outfile"
    fi
  fi
}

if [ ! -f "$1" ]; then
  for i in *.xrc; do
    do_clean "$i" || exit 1
  done
  exit
fi

do_clean "$1" "$2"