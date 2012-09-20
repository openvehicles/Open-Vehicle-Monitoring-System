#!/usr/bin/perl

use Digest::MD5;
use Digest::HMAC;
use Crypt::RC4::XS;
use MIME::Base64;

my $b64tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

select STDOUT; $|=1;

print "VIN:    ";
my $vin = <>; chop $vin;
print "ICCID:  ";
my $iccid = <>; chop $iccid;
my @apparams;
my $k=0;
printf "#%-4s:  ",$k;
while (<>)
  {
  chop;
  last if (/^$/);
  push @apparams,$_;
  $k++; printf "#%-4s:  ",$k;
  }
my $apn = join ' ',@apparams;
my $shared_secret = $iccid;

print "\n";
print "Generating Auto-Provisioning record:\n";
print "VIN:    ",$vin,"\n";
print "ICCID:  ",$iccid,"\n";
print "Params: ",join(' ',@apparams),"\n";
print "\n";
rand;
my $server_token;
foreach (0 .. 21)
  { $server_token .= substr($b64tab,rand(64),1); }

print "Server Token:      ",$server_token,"\n";

my $server_hmac = Digest::HMAC->new($shared_secret, "Digest::MD5");
$server_hmac->add($server_token);
my $server_digest = $server_hmac->b64digest();
print "Server Digest:     ",$server_digest,"\n";

my $server_key= decode_base64($server_digest);

my $txcipher = Crypt::RC4::XS->new($server_key);
$txcipher->RC4(chr(0) x 1024); # Prime the cipher

my $encrypted = encode_base64($txcipher->RC4($apn),'');
print "AP Record:         ",$encrypted,"\n";
