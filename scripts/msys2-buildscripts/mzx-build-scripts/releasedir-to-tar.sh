if [ -f updates-uncompressed.txt ]
then
  gzip < updates-uncompressed.txt > updates.txt
  tar --exclude updates-uncompressed.txt -cvf /mzx-build-workingdir/uploads/updates.tar *
else
  >&2 echo "Error: this is not the releases dir. Aborting."
  exit 1
fi
