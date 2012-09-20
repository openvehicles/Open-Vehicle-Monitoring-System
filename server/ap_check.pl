#!/usr/bin/perl

use Digest::MD5;
use Digest::HMAC;
use Crypt::RC4::XS;
use MIME::Base64;

my $b64tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

select STDOUT; $|=1;

print "ICCID:            ";
my $iccid = <>; chop $iccid;
print "Server Token:     ";
my $server_token = <>; chop $server_token;
print "Server Digest:    ";
my $server_digest_b64 = <>; chop $server_digest_b64;
print "AP Record:        ";
my $ap_record = <>; chop $ap_record; $ap_record = decode_base64($ap_record);
print "\n";

my $shared_secret = $iccid;

my $server_hmac = Digest::HMAC->new($shared_secret, "Digest::MD5");
$server_hmac->add($server_token);
my $server_digest = $server_hmac->b64digest();

if ($server_digest ne $server_digest_b64)
  {
  print "ERROR: Server digest does not authenticate ($server_digest != $server_digest_b64)\n";
  exit(1);
  }

print "Server Digest:     AUTHENTICATED\n";

my $server_key= decode_base64($server_digest);

my $rxcipher = Crypt::RC4::XS->new($server_key);
$rxcipher->RC4(chr(0) x 1024); # Prime the cipher

my $decrypted = $rxcipher->RC4($ap_record);
print "AP Record:         ",$decrypted,"\n";
