LANGS=`cat LINGUAS | grep -v '#'`
for lang in $LANGS ; do
  echo "lang: $lang"
done
