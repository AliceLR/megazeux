# $1: url
MSYSTEM=MSYS
. /etc/profile
cd /mzx-build-workingdir
scp uploads/updates.tar $1:updates.tar
ssh $1 "tar xvf updates.tar && rm -f updates.tar"
