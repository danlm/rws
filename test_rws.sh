#!/bin/sh -
#
# Simple shell script which tests whether rws is basically working.
# - by Richard W.M. Jones <rich@annexia.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Id: test_rws.sh,v 1.4 2003/02/05 23:02:51 rich Exp $

# A random, hopefully free, port.
port=14136

# We need either 'wget' or 'nc'.
wget --help >/dev/null 2>&1
if [ $? -eq 0 ]; then
	mode=wget
else
	nc -h 2>/dev/null
	if [ $? -eq 1 ]; then
		mode=nc
	else
		echo "Please install either 'wget' or 'nc'."
		echo "This test did not run."
		exit 0
	fi
fi

echo "Using $mode to fetch URLs."

tmp=/tmp/rws.$$
rm -rf $tmp
mkdir $tmp

# Create the configuration directory.
mkdir -p $tmp/etc/rws/hosts

cat > $tmp/etc/rws/rws.conf <<EOF
mime types file: $tmp/etc/mime.types
error log: $tmp/log/error_log
access log: $tmp/log/access_log
icon for application/*:         /icons/binary.gif 20x22 "Application"
icon for application/x-tar:     /icons/tar.gif 20x22 "Unix tape archive file"
icon for application/x-gzip:    /icons/compressed.gif 20x22 "Compressed file"
icon for application/zip:       /icons/compressed.gif 20x22 "Compressed file"
icon for audio/*:               /icons/sound1.gif 20x22 "Audio file"
icon for image/*:               /icons/image2.gif 20x22 "Image"
icon for message/*:             /icons/quill.gif 20x22 "Mail message"
icon for text/*:                /icons/text.gif 20x22 "Text file"
icon for video/*:               /icons/movie.gif 20x22 "Video file"
no type icon:                   /icons/generic.gif 20x22 "File"
unknown icon:                   /icons/unknown.gif 20x22 "Unknown file type"
directory icon:                 /icons/dir.gif 20x22 "Directory"
link icon:                      /icons/link.gif 20x22 "Symbolic link"
special icon:                   /icons/sphere2.gif 20x22 "Special file"
EOF

cat > $tmp/etc/rws/hosts/default <<EOF
alias /
	path:	$tmp/html
	show:	1
	list:	1
end alias
alias /so-bin/
	path:	$tmp/so-bin
	exec so: 1
end alias
alias /cgi-bin/
	path:	$tmp/cgi-bin
	exec:	1
end alias
EOF
(cd $tmp/etc/rws/hosts; ln -s default localhost:$port)

cat > $tmp/etc/mime.types <<EOF
text/html html
EOF

mkdir $tmp/log

# Create the content directory.
mkdir $tmp/html
mkdir $tmp/html/files

cat > $tmp/html/index.html <<EOF
<html>
<body>
This is the test page.
MAGIC-1234
</body>
</html>
EOF

cp *.o $tmp/html/files

# Create the so-bin directory.
mkdir $tmp/so-bin
cp examples/show_params.so $tmp/so-bin
chmod 0755 $tmp/so-bin/show_params.so

# Create the CGI directory
mkdir $tmp/cgi-bin
cat > $tmp/cgi-bin/test.sh <<EOF
#!/bin/sh
echo "HTTP/1.0 200 OK"
echo "Content-Type: text/plain"
echo
echo "This is the test CGI script"
echo "MAGIC-4321"
EOF
chmod 0755 $tmp/cgi-bin/test.sh

# Try to start up the server.
./rwsd -p $port -f -a 127.0.0.1 -C $tmp/etc/rws &
rws_pid=$!; sleep 1

# Did it start up?
if kill -0 $rws_pid; then :;
else
	echo "Server did not start up. Check any preceeding messages."
	exit 1
fi

echo "Started rwsd instance."

# Fetch function: fetch (server, port, serverpath, file)
fetch()
{
	server=$1
	port=$2
	serverpath=$3
	file=$4

	echo "Fetching http://$server:$port$serverpath ..."

	if [ $mode = "nc" ]; then
		nc $server $port > $file <<EOF
GET $serverpath HTTP/1.0
User-Agent: test_rws.sh

EOF
		if [ $? -ne 0 ]; then exit 1; fi
	else		# wget mode
		wget -q -O $file http://$server:$port$serverpath
		if [ $? -ne 0 ]; then kill $rws_pid; exit 1; fi
	fi
}

# Fetch the test file.
fetch localhost $port /index.html $tmp/downloaded
if grep -q MAGIC-1234 $tmp/downloaded; then :;
else
	echo "Download of a simple file failed!"
	echo "Look at $tmp/downloaded for clues."
	kill $rws_pid
	exit 1
fi
rm $tmp/downloaded

# Fetch the directory listing.
fetch localhost $port /files/ $tmp/downloaded
if grep -q main.o $tmp/downloaded; then :;
else
	echo "Download of a directory listing failed!"
        echo "Look at $tmp/downloaded for clues."
	kill $rws_pid
	exit 1
fi
rm $tmp/downloaded

# Test shared object scripts.
echo "Testing shared object scripts."
fetch localhost $port '/so-bin/show_params.so?key=value' $tmp/downloaded
if grep -q 'This is the show_params shared object script' $tmp/downloaded
then :;
else
        echo "Execution of a shared object script failed!"
        echo "Look at $tmp/downloaded for clues."
        kill $rws_pid
        exit 1
fi
rm $tmp/downloaded

# Test CGI scripts.
echo "Testing CGI scripts."
fetch localhost $port /cgi-bin/test.sh $tmp/downloaded
if grep -q MAGIC-4321 $tmp/downloaded; then :;
else
        echo "Execution of a CGI script failed!"
        echo "Look at $tmp/downloaded for clues."
        kill $rws_pid
        exit 1
fi
rm $tmp/downloaded

echo "Test completed OK."

# Kill the server.
kill $rws_pid

# Remove the temporary directory.
rm -rf $tmp

exit 0
