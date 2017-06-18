# Run some checks to make sure this isn't something other than a
# build directory, because this will totally nuke the directory
# it is run in

# Make sure there's no Makefile or config.sh or anything else
# that should only appear in a development directory
if [ -f config.sh ] || [ -d src ]
then
  >&2 echo "Error: this appears to be a source dir. Aborting."
  exit 1
fi

if [ -f manifest.txt ] && [ -f core.dll ] && [ -f megazeux.exe ]
then
  for file in `find -mindepth 1 -type f -not -name manifest.txt -not -name config.txt`; do
    sha256sum=`sha256sum -b $file | cut -f1 -d\ `
    gzip < $file > $sha256sum
    rm $file
  done
  mv manifest.txt __tmp_manifest.txt
  gzip < __tmp_manifest.txt > manifest.txt
  rm __tmp_manifest.txt

  find -mindepth 1 -type d -exec rm -rf '{}' ';' 2>/dev/null
  rm -f config.txt
  chmod 0644 *
else
  >&2 echo "Error: this is not an MZX build dir. Aborting."
  exit 1
fi
