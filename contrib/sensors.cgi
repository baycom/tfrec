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
my $step = 60;

my $q = new CGI();

my $item=$q->param('item');
my $limit=$q->param('limit');
my $type=$q->param('type');

# Cleanup
$item=~ s/[^0-9a-fA-F]//g;
$limit=~ s/[0-9]//g;
$type=~ s/[^\w\-]//g;

if ($item eq '') {
    $item = '3900';
}

if ($type eq '') {
    $type = 'tfa-humidity';
}

my $rlimit = "";

if ($limit eq '') {
    $limit = '`updated` > timestampadd(day, -90, now())';
} else {
    $limit = -int($limit);
    if($limit != 0) {
        $limit = "`updated` > timestampadd(day, $limit, now())";
    } else {
        $limit = '`updated` > timestampadd(day, -1, now())';
        $rlimit = "limit 1";        
    }
}

my $dbh=DBI->connect ($dsn, $db_user, $db_pass, {PrintError => 1, RaiseError => 0, mysql_enable_utf8 => 1});
$item = $dbh->quote($item);
$type = $dbh->quote($type);
my $sql = "select UNIX_TIMESTAMP(updated), ROUND(`meter`,1) from sensors where $limit and id = $item and type = $type order by updated desc $rlimit;";
my $array = $dbh->selectall_arrayref($sql);
print $q->header( -type=>'application/json', -charset=>'UTF-8');
#print $sql;
#print Dumper($array);

my @values = reverse(@$array);
my @newarray;
my $outarray = \@values;
my $lastval = 0;
my $lasttime = 0;
if(1) {
foreach my $vals (@values) {
    my @out = ( @$vals[0]*1.0, @$vals[1]*1.0);
    my $timenow = @$vals[0]*1.0;
    my $valnow = @$vals[1]*1.0;
    if($lasttime > 0) {
        my $timediff = $timenow - $lasttime;
        if($timediff > $step ) {
            while(($lasttime + $step) < $timenow) {
                $lasttime += $step;
                my @fill = ($lasttime, $lastval);
                push(@newarray, \@fill);
            } 
        }
    }
    
    push(@newarray, \@out);
    $lasttime = $timenow;
    $lastval  = $valnow;
}
my @out = ( time(), $lastval);
push(@newarray, \@out);

$outarray = \@newarray;
}
#print Dumper(@newarray);
my $json_str = encode_json($outarray);
#$json_str =~ s/\"//gm;

print "$json_str\n";

