#!/usr/bin/perl

use EV;
use AnyEvent;
use AnyEvent::Handle;
use AnyEvent::Socket;
use Digest::MD5;
use Digest::HMAC;
use Crypt::RC4::XS;
use MIME::Base64;
use IO::Socket::INET;
use Math::Trig qw(deg2rad pi great_circle_distance asin acos);

my $b64tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

my $shared_secret = "DEMO";
print STDERR "Shared Secret is: ",$shared_secret,"\n";

#####
##### CONNECT
#####

my $sock = IO::Socket::INET->new(PeerAddr => '127.0.0.1',
                                 PeerPort => '6867',
                                 Proto    => 'tcp');

#####
##### CLIENT
#####

my @latitudes =  ( 22.270553, 22.272161, 22.301067, 22.375294, 22.286586, 22.307848, 22.262411, 22.492383, 
                   22.338058, 22.442075, 22.288353, 22.371672, 22.324756, 22.243414, 22.323072, 22.426144, 
                   22.274433, 22.366056, 22.310994, 22.502972, 22.319078, 22.295394, 22.337297, 22.342644, 
                   22.320317, 22.300880, 22.295550, 22.280397, 22.315425, 22.277361, 22.369086, 22.382773, 
                   22.365850, 22.282833, 22.450369, 22.319344, 22.270489, 22.282989, 22.449021, 22.393533, 
                   22.274311, 22.442208, 22.270363, 22.280659, 22.278494, 22.280464, 22.286659, 22.267424, 
                   22.266014, 22.280827, 22.287945, 22.286108, 22.280728, 22.282644, 22.273867, 22.281420, 
                   22.278444, 22.303761, 22.311768, 22.321377, 22.313337, 22.311876, 22.309065, 22.324429, 
                   22.325256, 22.313612, 22.313227, 22.325961, 22.313087, 22.340977, 22.340788, 22.337493, 
                   22.302919, 22.298368, 22.297485, 22.317623, 22.319450, 22.290963, 22.361467, 22.487410, 
                   22.506491, 22.499964, 22.500574, 22.389791, 22.397291, 22.388657, 22.424746, 22.375674, 
                   22.363600, 22.371627, 22.372817, 22.372320, 22.383770, 22.396284, 22.393525, 22.393525, 
                   22.445194, 22.446680, 22.440573, 22.319101, 22.373611 );
my @longitudes = ( 114.186994, 114.186064, 114.167956, 114.111264, 114.218114, 114.162031, 114.130567, 114.138867, 
                   114.173817, 114.028017, 113.941636, 113.993500, 114.173128, 114.148067, 114.204267, 114.210080, 
                   114.171048, 114.137133, 114.225769, 114.127592, 114.168436, 114.272208, 114.187969, 114.190697, 
                   114.208603, 114.172019, 114.169275, 114.226847, 114.162317, 114.165336, 114.120258, 114.189416, 
                   114.140106, 114.160631, 114.160903, 114.156444, 114.149522, 114.191528, 114.002949, 113.976739, 
                   114.172247, 114.027564, 114.149948, 114.154709, 114.160867, 114.156865, 114.191566, 114.242821, 
                   114.250496, 114.224365, 114.202827, 114.196198, 114.174889, 114.173044, 114.187346, 114.182404, 
                   114.181541, 114.186460, 114.189247, 114.209373, 114.219511, 114.224358, 114.221664, 114.210350, 
                   114.210876, 114.218033, 114.219772, 114.204940, 114.219345, 114.171577, 114.202516, 114.214618, 
                   114.169495, 114.178352, 114.173419, 114.155945, 114.158206, 113.942835, 114.123802, 114.142662, 
                   114.122136, 114.144508, 114.143852, 114.207764, 114.193474, 114.207840, 114.228050, 114.112716, 
                   114.114326, 113.992403, 113.991780, 113.969810, 113.971344, 113.969627, 113.976739, 113.976739, 
                   114.031456, 114.021980, 114.021744, 113.943900, 114.109540 );

print STDERR "I am the demo car\n";

rand;
my $client_token;
foreach (0 .. 21)
  { $client_token .= substr($b64tab,rand(64),1); }
print STDERR "  Client token is  $client_token\n";

my $client_hmac = Digest::HMAC->new($shared_secret, "Digest::MD5");
$client_hmac->add($client_token);
my $client_digest = $client_hmac->b64digest();
print STDERR "  Client digest is $client_digest\n";

print $sock "MP-C 0 $client_token $client_digest DEMO\r\n";

my $line = <$sock>;
chop $line;
chop $line;
print STDERR "  Received $line from server\n";
my ($welcome,$crypt,$server_token,$server_digest) = split /\s+/,$line;

print STDERR "  Received $server_token $server_digest from server\n";

my $d_server_digest = decode_base64($server_digest);
my $client_hmac = Digest::HMAC->new($shared_secret, "Digest::MD5");
$client_hmac->add($server_token);
if ($client_hmac->digest() ne $d_server_digest)
  {
  print STDERR "  Client detects server digest is invalid - aborting\n";
  exit(1);
  }
print STDERR "  Client verification of server digest is ok\n";

$client_hmac = Digest::HMAC->new($shared_secret, "Digest::MD5");
$client_hmac->add($server_token);
$client_hmac->add($client_token);
my $client_key = $client_hmac->digest;
print STDERR "  Client version of shared key is: ",encode_base64($client_key,''),"\n";

my $txcipher = Crypt::RC4::XS->new($client_key);
$txcipher->RC4(chr(0) x 1024); # Prime the cipher
my $rxcipher = Crypt::RC4::XS->new($client_key);
$rxcipher->RC4(chr(0) x 1024); # Prime the cipher

my $encrypted = encode_base64($txcipher->RC4("MP-0 A"),'');
print STDERR "  Sending message $encrypted\n";
print $sock "$encrypted\r\n";

$line = <$sock>;
chop $line;
chop $line;
print STDERR "  Received $line from server\n";
print STDERR "  Server message decodes to: ",$rxcipher->RC4(decode_base64($line)),"\n";

# Now, let's go for it...
my $travelling = 1;
my $lat = $latitudes[0];
my $lon = $longitudes[0];
my ($soc,$kmideal,$kmest,$state,$mode,$volts,$amps) = (80,int((80.0/100.0)*350.0),int((80.0/100.0)*350.0)-20,'done','standard',0,0);
$encrypted = encode_base64($txcipher->RC4("MP-0 S$soc,K,$volts,$amps,$state,$mode,$kmideal,$kmest"),'');
print STDERR "  Sending message $encrypted\n";
print $sock "$encrypted\r\n";
$encrypted = encode_base64($txcipher->RC4("MP-0 L$lat,$lon"),'');
print STDERR "  Sending message $encrypted\n";
print $sock "$encrypted\r\n";

#$encrypted = encode_base64($txcipher->RC4("MP-0 PMWelcome to the Open Vehicle Monitoring System (push notification)"),'');
#print STDERR "  Sending message $encrypted\n";
#print $sock "$encrypted\r\n";

srand();

my $handle = new AnyEvent::Handle(fh => $sock, on_error => \&io_error, keepalive => 1, no_delay => 1);
$handle->push_read (line => \&io_line);

my $tim = AnyEvent->timer (after => 120, interval => 120, cb => \&io_tim);

# Main event loop...
EV::loop();

sub io_error
  {
  my ($hdl, $fatal, $msg) = @_;

  undef $hdl;
  undef $handle;
  print "Got error: $msg\n";
  sleep 30;
  exec './ovms_democar.pl';
  }

sub io_tim
  {
  my ($hdl) = @_;

  if ($travelling)
    {
    my $newdest = int(rand(scalar @longitudes - 1));
    my $newlon = $longitudes[$newdest];
    my $newlat = $latitudes[$newdest];
    my $dist = int(&Haversine($lat, $lon, $newlat, $newlon));
    ($lat,$lon) = ($newlat,$newlon);
    $kmideal = $kmideal-$dist;
    $kmest = int($kmideal*0.9);
    $soc = int(100*$kmideal/350);
    print "Travelling ",$dist,"km - now SOC is $soc ($kmideal,$kmest)\n";
    if ($soc < 30)
      {
      ($travelling,$volts,$amps,$state,$mode) = (0,220,70,"charging","standard");
      }
    $handle->push_write(encode_base64($txcipher->RC4("MP-0 S$soc,K,$volts,$amps,$state,$mode,$kmideal,$kmest"),'')."\r\n");
    $handle->push_write(encode_base64($txcipher->RC4("MP-0 L$lat,$lon"),'')."\r\n");
    }
  else
    {
    $kmideal += 10;
    $kmest = int($kmideal*0.9);
    $soc = int(100*$kmideal/350);
    print "Charging - now SOC is $soc ($kmideal,$kmest)\n";
    if ($soc > 95)
      {
      ($travelling,$volts,$amps,$state,$mode) = (1,0,0,"done","standard");
      }
    $handle->push_write(encode_base64($txcipher->RC4("MP-0 S$soc,K,$volts,$amps,$state,$mode,$kmideal,$kmest"),'')."\r\n");
    }
  }

sub io_line
  {
  my ($hdl, $line) = @_;

  print "  Received $line from server\n";

  $line = $rxcipher->RC4(decode_base64($line));
  print "  Server message decodes to: $line\n";
  if ($line =~ /^MP-0 A/)
    {
    my $encrypted = encode_base64($txcipher->RC4("MP-0 a"),'');
    print STDERR "  Sending message $encrypted\n";
    print $hdl->push_write("$encrypted\r\n");
    }
  $hdl->push_read (line => \&io_line);
  }

sub Haversine
  {
  my ($lat1, $long1, $lat2, $long2) = @_;
  my $r=3956;

  $dlong = deg2rad($long1) - deg2rad($long2);
  $dlat  = deg2rad($lat1) - deg2rad($lat2);

  $a = sin($dlat/2)**2 +cos(deg2rad($lat1)) 
                    * cos(deg2rad($lat2))
                    * sin($dlong/2)**2;
  $c = 2 * (asin(sqrt($a)));
  $dist = $r * $c;               

  return $dist*1.609344;
  }

