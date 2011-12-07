#!/usr/bin/perl

use Digest::MD5;
use Digest::HMAC;
use Crypt::RC4::XS;
use MIME::Base64;
use IO::Socket::INET;

my $b64tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

my $shared_secret = "NETPASS";
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

print STDERR "I am the client\n";

rand;
my $client_token;
foreach (0 .. 21)
  { $client_token .= substr($b64tab,rand(64),1); }
print STDERR "  Client token is  $client_token\n";

my $client_hmac = Digest::HMAC->new($shared_secret, "Digest::MD5");
$client_hmac->add($client_token);
my $client_digest = $client_hmac->b64digest();
print STDERR "  Client digest is $client_digest\n";

print $sock "MP-C 0 $client_token $client_digest TESTCAR\r\n";

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

$encrypted = encode_base64($txcipher->RC4("MP-0 S80,K,220,70,charging,standard,280,270"),'');
print STDERR "  Sending message $encrypted\n";
print $sock "$encrypted\r\n";

$encrypted = encode_base64($txcipher->RC4("MP-0 L22.274165,114.185715"),'');
print STDERR "  Sending message $encrypted\n";
print $sock "$encrypted\r\n";

#$encrypted = encode_base64($txcipher->RC4("MP-0 PMSonny: This is a 3rd test of the vehicle transmission system"),'');
#print STDERR "  Sending message $encrypted\n";
#print $sock "$encrypted\r\n";

while(<$sock>)
  {
  chop; chop;
  print STDERR "  Received $_ from server\n";
  my $line = $rxcipher->RC4(decode_base64($_));
  print STDERR "  Server message decodes to: ",$line,"\n";
  if ($line =~ /^MP-0 A/)
    {
    $encrypted = encode_base64($txcipher->RC4("MP-0 a"),'');
    print STDERR "  Sending message $encrypted\n";
    print $sock "$encrypted\r\n";
    }
  }
