export PATH="$(pwd)/shady:$PATH"
for file in `find $(pwd) -name "*.png" -o -name "*.xml"`; do
	cd $(dirname $file)
	shady-cli.exe convert $file
	cd - >/dev/null
done
rm -rf output
cp -r data output
find output -name "*.png" -delete -print -o -name "*.xml" -delete -print