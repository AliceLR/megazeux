hash=`sha256sum $1 | cut -f 1 -d " "`
curl --progress-bar \
  -H "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_10_3) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.89 Safari/537.36" \
  -F "hash=$hash" \
  -F "upload=@$1" \
  $2
