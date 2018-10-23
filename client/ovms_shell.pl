#!/usr/bin/perl

use EV;
use AnyEvent;
use AnyEvent::Handle;
use AnyEvent::Socket;
use AnyEvent::ReadLine::Gnu;
use Digest::MD5;
use Digest::HMAC;
use Crypt::RC4::XS;
use MIME::Base64;
use DBI;
use Config::IniFiles;

# Command line interface
select STDOUT; $|=1;
my $server = $ARGV[0]; $server="127.0.0.1" if (!defined $server);
my $port = $ARGV[1];   $port=6867 if (!defined $port);

# Configuration
my $config = Config::IniFiles->new(-file => 'ovms_shell.conf') if (-e 'ovms_shell.conf');

# Database
our $db = DBI->connect($config->val('db','path'),$config->val('db','user'),$config->val('db','pass')) if (defined $config);
$db->{mysql_auto_reconnect} = 1 if (defined $db);

# Globals
my $b64tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
my $connvid;
my $connpass;
my $conn;
my $connected = 0;
my $client_token;
my $txcipher;
my $rxcipher;
my $rl;

sub conn_line
  {
  my ($hdl, $data) = @_;

  my $decoded = $rxcipher->RC4(decode_base64($data));
  $decoded =~ tr /\r/\n/;

  $rl->hide();
  if ($decoded =~ /^MP-0 Z(\d+)/)
    {
    $rl->print("Server reports $1 app(s) connected (including this shell)\n\n");
    }
  elsif ($decoded =~ /^MP-0 T(\d+)/)
    {
    if ($1 == 0)
      {
      if ($connected==1)
        {
        $rl->print("Vehicle is online live\n\n");
        $connected=2;
        }
      }
    else
      { $rl->print("Vehicle data is currently $1 second(s) delayed\n\n"); }
    }
  elsif ($decoded =~ /^MP-0 F(.+)/)
    {
    my ($version,$vin,$netsq,$netsq1,$type,$provider) = split /,/,$1;
    $rl->print("Vehicle Firmware Message:
  Version:   $version
  Type:      $type
  VIN:       $vin
  Signal:    $provider ($netsq)\n\n");
    }
  elsif ($decoded =~ /^MP-0 f(.+)/)
    {
    $rl->print("Server Firmware Message:
  Version:   $1\n\n");
    }
  elsif ($decoded =~ /^MP-0 W(.+)/)
    {
    my ($fr_p,$fr_t,$rr_p,$rr_t,$fl_p,$fl_t,$rl_p,$rl_t) = split /,/,$1;
    $rl->print("Vehicle TPMS Message:
  FL:        $fl_p PSI $fl_t C
  FR:        $fr_p PSI $fr_t C
  RL:        $rl_p PSI $rl_t C
  RR:        $rr_p PSI $rr_t C\n\n");
    }
  elsif ($decoded =~ /^MP-0 L(.+)/)
    {
    my ($lat,$lon,$dir,$alt,$gpslock,$speed,$drivemode,$batp,$bateu,$bater) = split /,/,$1;
    $rl->print("Vehicle Location Message:
  Position:  $lat,$lon (lock: $gpslock)
  Direction: $dir
  Altitude:  $alt
  Speed:     $speed
  DriveMode: $drivemode
  Battery:   power $batp used $bateu recd $bater\n\n");
    }
  elsif ($decoded =~ /^MP-0 S(.+)/)
    {
    my ($soc,$units,$cv,$cc,$cs,$cm,$ri,$re,$climit,$ctime,$b4,$chkwh,$chss,$chs,$chm,
        $ctm,$cts,$stale,$cac,$cdf,$cle,$cls,$cooling,$cooltb,$cooltl,$chargest,$minsr,$minssoc,
        $bmr,$chargetype,$batv,$soh) = split /,/,$1;
    $rl->print("Vehicle Status Message:
  Battery:   soc $soc cac $cac soh $soh voltage $batv range-full $bmr$units
  Range:     ideal $ri$units estimated $re$units
  Charge:    state $cs mode $cm ($cv V $cc A, limit $climit A) type $chargetype
             state $chs mode $chm substate $chss
             time $ctime duration $cd ($chkwh kWh) to-full $cdf to-set $cle$units/$cls%%
             timer $ctm (start $cts)
  Cooling:   $cooling (limit temp $cooltb time $cooltl) estimated $chargest $minsr$units $minssoc%%\n\n");
    }
  elsif ($decoded =~ /^MP-0 D(.+)/)
    {
    my ($doors1,$doors2,$locked,$ti,$tm,$tb,$trip,$odometer,$speed,$parktime,$te,
        $doors3,$tstale,$estale,$b12v,$doors4,$b12r,$doors5,$tc,$b12c,$tcab) = split /,/,$1;
    $rl->print("Vehicle Environment Message
  Temps:     inverter $ti motor $tm battery $tb environment $te charger $tc cabin $tcab (stale $tstale)
  Locked:    $locked
  Trip/Odo:  trip $trip odometer $odometer speed $speed park-time $parktime
  12vBatt:   level $b12v current $b12c (ref $b12r)
  Doors:     doors1 $doors1 doors2 $doors2 doors3 $doors3 doors4 $doors4 doors5 $doors5\n\n");
    }
  elsif ($decoded =~ /^MP-0 c7,0,(.+)$/s)
    {
    $rl->print("Vehicle Response:\n  " . join("\n  ",split(/\n/,$1)) . "\n");
    }
  else
    {
    $rl->print("<$decoded\n");
    }
  $rl->show();
  $hdl->push_read(line => \&conn_line);
  }

sub conn_login
  {
  my ($hdl, $data) = @_;
  $rl->hide();
  $rl->print("<$data\n");

  my ($welcome,$crypt,$server_token,$server_digest) = split /\s+/,$data;
  my $d_server_digest = decode_base64($server_digest);
  my $client_hmac = Digest::HMAC->new($connpass, "Digest::MD5");
  $client_hmac->add($server_token);

  if ($client_hmac->digest() ne $d_server_digest)
    {
    $rl->print("Server digest is invalid (incorrect vehicleid/password?)\n");
    undef $conn;
    $rl->show();
    return;
    }

  $client_hmac = Digest::HMAC->new($connpass, "Digest::MD5");
  $client_hmac->add($server_token);
  $client_hmac->add($client_token);
  my $client_key = $client_hmac->digest;
  $txcipher = Crypt::RC4::XS->new($client_key);
  $txcipher->RC4(chr(0) x 1024); # Prime the cipher
  $rxcipher = Crypt::RC4::XS->new($client_key);
  $rxcipher->RC4(chr(0) x 1024); # Prime the cipher
  $rl->print("Connected and logged in to $connvid\n");
  $rl->show();
  $connected = 1;
  $conn->push_read(line => \&conn_line);
  };

sub command
  {
  my ($line) = @_;
  $line =~ s/^\s+//g;
  $line =~ s/\s+$//g;

  if ($line =~ /^open\s+(\S+)(\s+(\S+))?$/)
    {
    if (defined $conn)
      {
      $rl->print("Closing connection to $connvid\n");
      undef $conn;
      }
    $rl->print("Connecting to $1...\n");
    $connvid = $1;
    $connpass = $3;
    if ((!defined $connpass)&&(defined $db))
      {
      my $sth = $db->prepare('SELECT * FROM ovms_cars WHERE vehicleid=?');
      $sth->execute($connvid);
      my $row = $sth->fetchrow_hashref();
      if (defined $row)
        {
        $connpass = $row->{'carpass'};
        $rl->print("(vehicle password retrieved from server database)\n");
        }
      }
    $conn = new AnyEvent::Handle
      connect => [$server,$port],
      on_connect => sub
        {
        my ($hdl) = @_;
        $rl->hide();
        $rl->print("Connected to server\n");
        foreach (0 .. 21) { $client_token .= substr($b64tab,rand(64),1); }
        my $client_hmac = Digest::HMAC->new($connpass, "Digest::MD5");
        $client_hmac->add($client_token);
        my $client_digest = $client_hmac->b64digest();
        $rl->print(">MP-A 0 $client_token $client_digest $connvid\n");
        $hdl->push_write("MP-A 0 $client_token $client_digest $connvid\r\n");
        $hdl->push_read(line => \&conn_login);
        $rl->show();
        },
      on_connect_error => sub
        {
        $rl->hide();
        $rl->print("Failed to connect to server ($!)\n");
        undef $conn;
        $connected =0;
        $rl->show();
        },
      on_error => sub
        {
        $rl->hide();
        $rl->print("Disconnected from server ($!)\n");
        undef $conn;
        $connected = 0;
        $rl->show();
        },
    }
  elsif ($line =~ /^close$/)
    {
    if (defined $conn)
      {
      $rl->print("Closing connection to $connvid\n");
      undef $conn;
      $connected = 0;
      }
    }
  else
    {
    # Send the line to the car, as a command...
    if (!$connected)
      {
      $rl->print("Not connected, so can't command vehicle\n");
      }
    else
      {
      $conn->push_write(encode_base64($txcipher->RC4("MP-0 C7,".$line),'')."\r\n");
      $rl->print("...\n");
      }
    }
  }

print "Welcome to the OVMS shell\n";
print "Server is $server:$port (can be specified on the command line)\n";
print "Enter 'open <vehicleid> [<password>]' to connect to a vehicle, or 'close' to disconnect\n";
print "\n";
$rl = new AnyEvent::ReadLine::Gnu
  prompt => "ovms> ",
  on_line => sub
    {
    # called for each line entered by the user
    my ($line) = @_;
    if (!defined $line)
      {
      $rl->print("All done\n");
      EV::unloop();
      }
    else
      {
      $rl->hide();
      &command($line);
      $rl->show();
      }
    };

# Main event loop...
EV::loop();
exit 0;
