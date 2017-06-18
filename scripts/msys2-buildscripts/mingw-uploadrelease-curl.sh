# Just pass this the upload php url

MSYSTEM=MSYS
. /etc/profile
cd /mzx-build-workingdir
/mzx-build-scripts/upload-updates.sh uploads/updates.tar $1
