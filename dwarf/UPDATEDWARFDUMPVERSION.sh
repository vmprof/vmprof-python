# Do not turn on -x here. it will screw things up!
x=`date |tr '\n' ' '`
cat > UPD.awk <<\EOF
BEGIN {
if (ARGC <=  2)  {
    print "Bogus use of awk file, requires arg"
    exit 1   
} else  {
    v=ARGV[1]
    ARGV[1]=""
}
}
$0 ~  /#define DWARFDUMP_VERSION/ { print $1, $2, "\"",v,"\"" }
$0 !~ /^#define DWARFDUMP_VERSION/ { print $0 }
EOF
awk -f UPD.awk  "$x"  dwarfdump/dwarfdump.c >t
mv t dwarfdump/dwarfdump.c
awk -f UPD.awk  "$x"  dwarfdump/common.c >t
mv t dwarfdump/common.c
