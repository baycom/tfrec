#!/usr/bin/perl
use strict;
use POSIX;
use CGI;
use Data::Dumper;
use IO::Socket::INET;
use Time::HiRes qw(usleep);
use DBI;
use JSON;

$CGI::PARAM_UTF8=1;
$| = 1;

my $host_name = "localhost";
my $db_name = "smarthome";
my $dsn = "DBI:mysql:host=$host_name;database=$db_name";
my $db_user = "smarthome";
my $db_pass = "some password";

my $q = new CGI();
my $autorefresh = 1;

my $item=$q->param('item');
my $temp=$q->param('temp');
my $humidity=$q->param('humidity');
my $lowbatt=$q->param('lowbatt');

# cleanup
$item=~ s/[^0-9a-fA-F]//g;
$temp=~ s/[^0-9\.\-]//g;
$humidity=~ s/[^0-9]//g;
$lowbatt=~ s/[^0-9]//g;

my $dbh=DBI->connect ($dsn, $db_user, $db_pass, {PrintError => 1, RaiseError => 0, mysql_enable_utf8 => 1});

print $q->header( -type=>'text/html', -charset=>'utf-8');
my $result = "ok";
my $ret;
if($item && $temp && $humidity) {
        $item = $dbh->quote($item);
        my $now = time();
        my ($updated, $upd, $meter) = $dbh->selectrow_array("select UNIX_TIMESTAMP(CONVERT_TZ(updated, '+00:00', 'SYSTEM')), updated, ROUND(meter,1) from sensors where type = 'tfa-temp' and id = $item order by updated desc limit 1;");
        if($temp != $meter) {
                $temp = $dbh->quote($temp);
                my $sql="insert into `sensors` (`type`, `id`, `meter`, `lowbatt`) values ('tfa-temp', $item, $temp, $lowbatt);";
                $ret=$dbh->do($sql);
                if( !$ret || $ret < 1) {
                        $result = "error temp $sql";
                } else {
                        $result = "ok temp $sql";
                }
        } else {
#                my $sql="update `sensors` set `updated` = now() where `type` = 'tfa-temp' and `id` = $item  and `updated` = '$upd';";
#                $dbh->do($sql);
#                $result = " updated temp $sql";
        }

        my ($updated, $upd, $meter) = $dbh->selectrow_array("select UNIX_TIMESTAMP(CONVERT_TZ(updated, '+00:00', 'SYSTEM')), updated, ROUND(meter,0) from sensors where type = 'tfa-humidity' and id = $item order by updated desc limit 1;");
        if($humidity != $meter) {
                $humidity = $dbh->quote($humidity);
                my $sql="insert into `sensors` (`type`, `id`, `meter`, `lowbatt`) values ('tfa-humidity', $item, $humidity, $lowbatt);";
                $ret=$dbh->do($sql);
                if( !$ret || $ret < 1) {
                        $result = $result." error humidity $sql";
                } else {
                        $result = $result." ok humidity $sql";
                }
        }  else {
#                my $sql="update `sensors` set `updated` = now() where `type` = 'tfa-humidity' and `id` = $item  and `updated` = '$upd';";
#                $dbh->do($sql);
#                $result = $result." updated humidity $sql";
        }

}

my $html = << "EOF;";
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html PUBLIC "-//WAPFORUM//DTD XHTML Mobile 1.0//EN" "http://www.wapforum.org/DTD/xhtml-mobile10.dtd">
<html>
<body>
$result
</body>
</html>
EOF;

print $html;

