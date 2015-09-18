#!/usr/bin/perl

use Digest::MD5;
use Digest::HMAC;
use Crypt::RC4::XS;
use MIME::Base64;
use IO::Socket::INET;
use Config::IniFiles;

# Read command line arguments:
my $cmd_user = $ARGV[0];
my $cmd_email = $ARGV[1];

if (!$cmd_email) {
 print "usage: subscribe.pl user email\n";
 print "use email '-' to unsubscribe\n";
 exit;
}

my $b64tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

# Configuration
my $config = Config::IniFiles->new(-file => 'ovms_client.conf');

my $vehicle_id       = $config->val('client','vehicle_id','TESTCAR');
my $server_password  = $config->val('client','server_password','NETPASS');
my $module_password  = $config->val('client','module_password','OVMS');

my $server_ip        = $config->val('client','server_ip','54.243.136.230');


#####
##### CONNECT
#####

my $sock = IO::Socket::INET->new(PeerAddr => $server_ip,
                                 PeerPort => '6867',
                                 Proto    => 'tcp');

#####
##### REGISTER
#####

rand;
my $client_token;
foreach (0 .. 21)
  { $client_token .= substr($b64tab,rand(64),1); }

my $client_hmac = Digest::HMAC->new($server_password, "Digest::MD5");
$client_hmac->add($client_token);
my $client_digest = $client_hmac->b64digest();

print $sock "MP-A 0 $client_token $client_digest $vehicle_id\r\n";

my $line = <$sock>;
chop $line;
chop $line;
my ($welcome,$crypt,$server_token,$server_digest) = split /\s+/,$line;

my $d_server_digest = decode_base64($server_digest);
my $client_hmac = Digest::HMAC->new($server_password, "Digest::MD5");
$client_hmac->add($server_token);
if ($client_hmac->digest() ne $d_server_digest)
  {
  print STDERR "  Client detects server digest is invalid - aborting\n";
  exit(1);
  }

$client_hmac = Digest::HMAC->new($server_password, "Digest::MD5");
$client_hmac->add($server_token);
$client_hmac->add($client_token);
my $client_key = $client_hmac->digest;

my $txcipher = Crypt::RC4::XS->new($client_key);
$txcipher->RC4(chr(0) x 1024); # Prime the cipher
my $rxcipher = Crypt::RC4::XS->new($client_key);
$rxcipher->RC4(chr(0) x 1024); # Prime the cipher


##### 
##### SEND COMMAND
##### 

my $cmd = "p".$cmd_user.",mail,,".$vehicle_id.",".$server_password.",".$cmd_email;

my $encrypted = encode_base64($txcipher->RC4("MP-0 ".$cmd),'');
print $sock "$encrypted\r\n";


##### 
##### READ RESPONSE
##### 

my $ptoken = "";
my $pdigest = "";
my $data = "";
my $discardcnt = 0;

while(1)
{
	# Timeout socket reads after 1 second:
	eval {
			local $SIG{ALRM} = sub { die "Timed Out" };
			alarm 1;
			$data = <$sock>;
			alarm 0;
		};
		if ($@ and $@ =~ /Timed Out/) {
			print STDOUT "OK.\n";
			exit;
		}
}

