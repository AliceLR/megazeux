# Unused now because we use "make archive" instead and
# call the builddir-to-updatedir script directly.
rm -f /mzx-build-workingdir/zips/$1-$2.zip
zip -r /mzx-build-workingdir/zips/$1-$2.zip *
/mzx-build-scripts/builddir-to-updatedir.sh
