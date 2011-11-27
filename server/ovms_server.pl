#!/usr/bin/perl

use EV;
use AnyEvent;
use AnyEvent::Handle;
use AnyEvent::Socket;
use IO::Handle;
use AnyEvent::Log;
use Config::IniFiles;
use DBI;
use Digest::MD5;
use Digest::HMAC;
use Crypt::RC4::XS;
use MIME::Base64;

# Global Variables

my $b64tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
my %conns;
my %car_conns;
my %app_conns;
my $db;
my $config;

# Auto-flush
select STDOUT; $|=1;
$AnyEvent::Log::FILTER->level ("info");

# Configuration
$config = Config::IniFiles->new(-file => 'ovms_server.conf');

# A database ticker
$db = DBI->connect($config->val('db','path'),$config->val('db','user'),$config->val('db','pass'));
if (!defined $db)
  {
  AE::log error => "fatal: cannot connect to database ($!)";
  exit(1);
  }
$db->{mysql_auto_reconnect} = 1;
my $dbtim = AnyEvent->timer (after => 60, interval => 60, cb => \&db_tim);

sub io_error
  {
  my ($hdl, $fatal, $msg) = @_;

  my $fn = $hdl->fh->fileno();
  my $vid = $conns{$fn}{'vehicleid'}; $vid='-' if (!defined $vid);
  my $clienttype = $conns{$fn}{'clienttype'}; $clienttype='-' if (!defined $clienttype);
  AE::log info => "#$fn $clienttype $vid got error $msg";
  &io_terminate($fn,$hdl,$conns{$fn}{'vehicleid'},undef);
  }

sub io_timeout
  {
  my ($hdl) = @_;

  my $fn = $hdl->fh->fileno();
  my $vid = $conns{$fn}{'vehicleid'}; $vid='-' if (!defined $vid);
  my $clienttype = $conns{$fn}{'clienttype'}; $clienttype='-' if (!defined $clienttype);
  AE::log error => "#$fn $clienttype $vid got timeout";
  &io_terminate($fn,$hdl,$conns{$fn}{'vehicleid'},undef);
  }

sub io_line
  {
  my ($hdl, $line) = @_;

  my $fn = $hdl->fh->fileno();
  my $vid = $conns{$fn}{'vehicleid'}; $vid='-' if (!defined $vid);
  my $clienttype = $conns{$fn}{'clienttype'}; $clienttype='-' if (!defined $clienttype);
  AE::log info => "#$fn $clienttype $vid rx $line";
  $hdl->push_read(line => \&io_line);

  if ($line =~ /^MP-(\S)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)/)
    {
    my ($clienttype,$protscheme,$clienttoken,$clientdigest,$vehicleid) = ($1,$2,$3,$4,$5);
    if ($protscheme ne '0')
      {
      &io_terminate($fn,$hdl,undef,"#$fn $vehicleid error - Unsupported protection scheme - aborting connection");
      return;
      }
    my $vrec = &db_get_vehicle($vehicleid);
    if (!defined $vrec)
      {
      &io_terminate($fn,$hdl,undef,"#$fn $vehicleid error - Unknown vehicle - aborting connection");
      return;
      }

    # Authenticate the client
    my $dclientdigest = decode_base64($clientdigest);
    my $serverhmac = Digest::HMAC->new($vrec->{'carpass'}, "Digest::MD5");
    $serverhmac->add($clienttoken);
    if ($serverhmac->digest() ne $dclientdigest)
      {
      &io_terminate($fn,$hdl,undef,"#$fn $vehicleid error - Incorrect client authentication - aborting connection");
      return;
      }

    # Calculate a server token    
    my $servertoken;
    foreach (0 .. 21)
      { $servertoken .= substr($b64tab,rand(64),1); }
    $serverhmac = Digest::HMAC->new($vrec->{'carpass'}, "Digest::MD5");
    $serverhmac->add($servertoken);
    my $serverdigest = encode_base64($serverhmac->digest(),'');

    # Calculate the shared session key
    $serverhmac = Digest::HMAC->new($vrec->{'carpass'}, "Digest::MD5");
    my $sessionkey = $servertoken . $clienttoken;
    $serverhmac->add($sessionkey);
    my $serverkey = $serverhmac->digest;
    AE::log info => "#$fn $clienttype $vehicleid crypt session key $sessionkey (".unpack("H*",$serverkey).")";
    my $txcipher = Crypt::RC4::XS->new($serverkey);
    $txcipher->RC4(chr(0) x 1024);  # Prime with 1KB of zeros
    my $rxcipher = Crypt::RC4::XS->new($serverkey);
    $rxcipher->RC4(chr(0) x 1024);  # Prime with 1KB of zeros

    # Store these for later use...
    $conns{$fn}{'serverkey'} = $serverkey;
    $conns{$fn}{'serverdigest'} = $serverdigest;
    $conns{$fn}{'servertoken'} = $servertoken;
    $conns{$fn}{'clientdigest'} = $clientdigest;
    $conns{$fn}{'clienttoken'} = $clienttoken;
    $conns{$fn}{'vehicleid'} = $vehicleid;
    $conns{$fn}{'txcipher'} = $txcipher;
    $conns{$fn}{'rxcipher'} = $rxcipher;
    $conns{$fn}{'clienttype'} = $clienttype;

    # Send out server welcome message
    AE::log info => "#$fn $clienttype $vehicleid tx MP-S 0 $servertoken $serverdigest";
    $hdl->push_write("MP-S 0 $servertoken $serverdigest\r\n");

    # Login...
    &io_login($fn,$hdl,$vehicleid,$clienttype);
    }
  elsif (defined $conns{$fn}{'vehicleid'})
    {
    # Let's process this as an encrypted message line...
    my $message = $conns{$fn}{'rxcipher'}->RC4(decode_base64($line));
    if ($message =~ /^MP-0\s(\S)(.*)/)
      {
      my ($code,$data) = ($1,$2);
      AE::log info => "#$fn $clienttype $vid rx $code $data";
      &io_message($fn, $hdl, $conns{$fn}{'vehicleid'}, $code, $data);
      }
    else
      {
      &io_terminate($fn,$hdl,$conns{$fn}{'vehicleid'},"#$fn $vid error - Unable to decode message - aborting connection");
      return;
      }
    }
  else
    {
    AE::log info => "#$fn $clienttype $vid error - unrecognised message from vehicle";
    }
  }

sub io_login
  {
  my ($fn,$hdl,$vehicleid,$clienttype) = @_;

  if ($clienttype eq 'A')
    {
    # An APP login
    $app_conns{$vehicleid}{$fn} = $fn;
    # Notify any listening cars
    my $cfn = $car_conns{$vehicleid};
    if (defined $cfn)
      {
      &io_tx($cfn, $conns{$cfn}{'handle'}, 'Z', scalar keys %{$app_conns{$vehicleid}});
      }
    # And notify the app itself
    &io_tx($fn, $hdl, 'Z', (defined $car_conns{$vehicleid})?"1":"0");
    # Update the app with current stored messages
    my $vrec = &db_get_vehicle($vehicleid);
    if ($vrec->{'v_ptoken'} ne '')
      {
      &io_tx($fn, $hdl, 'E', 'T'.$vrec->{'v_ptoken'});
      }
    my $sth = $db->prepare('SELECT * FROM ovms_carmessages WHERE vehicleid=? and m_valid=1');
    $sth->execute($vehicleid);
    while (my $row = $sth->fetchrow_hashref())
      {
      if ($row->{'m_paranoid'})
        {
        &io_tx($fn, $hdl, 'E', 'M'.$row->{'m_code'}.$row->{'m_msg'});
        }
      else
        {
        &io_tx($fn, $hdl, $row->{'m_code'},$row->{'m_msg'});
        }
      }
    &io_tx($fn, $hdl, 'T', $vrec->{'v_lastupdatesecs'});
    }
  elsif ($clienttype eq 'C')
    {
    if (defined $car_conns{$vehicleid})
      {
      # Car is already logged in - terminate it
      &io_terminate($car_conns{$vehicleid},$conns{$car_conns{$vehicleid}}{'handle'},$vehicleid, "#$car_conns{$vehicleid} $vehicleid error - duplicate car login - clearing first connection");
      }
    $car_conns{$vehicleid} = $fn;
    # Notify any listening apps
    &io_tx_apps($vehicleid, 'Z', '1');
    # And notify the car itself
    my $appcount = (defined $app_conns{$vehicleid})?(scalar keys %{$app_conns{$vehicleid}}):0;
    &io_tx($fn, $hdl, 'Z', $appcount);
    }
  }

sub io_terminate
  {
  my ($fn, $handle, $vehicleid, $msg) = @_;

  AE::log error => $msg if (defined $msg);

  if (defined $vehicleid)
    {
    if ($conns{$fn}{'clienttype'} eq 'C')
      {
      delete $car_conns{$vehicleid};
      # Notify any listening apps
      foreach (keys %{$app_conns{$vehicleid}})
        {
        my $afn = $_;
        &io_tx($afn, $conns{$afn}{'handle'}, 'Z', '0');
        }
      }
    elsif ($conns{$fn}{'clienttype'} eq 'A')
      {
      delete $app_conns{$vehicleid}{$fn};
      # Notify any listening cars
      my $cfn = $car_conns{$vehicleid};
      if (defined $cfn)
        {
        &io_tx($cfn, $conns{$cfn}{'handle'}, 'Z', scalar keys %{$app_conns{$vehicleid}});
        }
      }
    }

  $handle->destroy if (defined $handle);
  delete $conns{$fn} if (defined $fn);;

  return;
  }

sub io_tx
  {
  my ($fn, $handle, $code, $data) = @_;

  my $vid = $conns{$fn}{'vehicleid'};
  my $clienttype = $conns{$fn}{'clienttype'}; $clienttype='-' if (!defined $clienttype);
  AE::log info => "#$fn $clienttype $vid tx $code $data";
  $handle->push_write(encode_base64($conns{$fn}{'txcipher'}->RC4("MP-0 $code$data"),'')."\r\n");
  }

sub io_tx_car
  {
  my ($vehicleid, $code, $data) = @_;

  my $cfn = $car_conns{$vehicleid};
  if (defined $cfn)
    {
    &io_tx($cfn, $conns{$cfn}{'handle'}, $code, $data);
    }
  }

sub io_tx_apps
  {
  my ($vehicleid, $code, $data) = @_;

  # Notify any listening apps
  foreach (keys %{$app_conns{$vehicleid}})
    {
    my $afn = $_;
    &io_tx($afn, $conns{$afn}{'handle'}, $code, $data);
    }
  }

# A TCP listener
tcp_server undef, 6867, sub
  {
  my ($fh, $host, $port) = @_;
  my $key = "$host:$port";
  $fh->blocking(0);
  my $fn = $fh->fileno();
  AE::log info => "#$fn - new connection from $host:$port";
  my $handle; $handle = new AnyEvent::Handle(fh => $fh, on_error => \&io_error, on_timeout => \&io_timeout, keepalive => 1, no_delay => 1, rtimeout => 1200.0);
  $handle->push_read (line => \&io_line);

  $conns{$fn}{'fh'} = $fh;
  $conns{$fn}{'handle'} = $handle;
  };

# Main event loop...
EV::loop();

sub db_tim
  {
  #print "DB: Tick...\n";
  if (!defined $db)
    {
    $db = DBI->connect($config->val('db','path'),$config->val('db','user'),$config->val('db','pass'));
    return;
    }
  if (! $db->ping())
    {
    AE::log error => "Lost database connection - reconnecting...";
    $db = DBI->connect($config->val('db','path'),$config->val('db','user'),$config->val('db','pass'));
    }
  }

sub db_get_vehicle
  {
  my ($vehicleid) = @_;

  my $sth = $db->prepare('SELECT *,TIME_TO_SEC(TIMEDIFF(UTC_TIMESTAMP(),v_lastupdate)) as v_lastupdatesecs FROM ovms_cars WHERE vehicleid=?');
  $sth->execute($vehicleid);
  my $row = $sth->fetchrow_hashref();

  return $row;
  }

# Message handlers
sub io_message
  {
  my ($fn,$handle,$vehicleid,$code,$data) = @_;

  my $clienttype = $conns{$fn}{'clienttype'}; $clienttype='-' if (!defined $clienttype);

  # Handle system-level messages first
  if ($code eq 'A') ## PING
    {
    AE::log info => "#$fn $clienttype $vehicleid msg ping from $vehicleid";
    &io_tx($fn, $handle, "a", "");
    return;
    }
  elsif ($code eq 'a') ## PING ACK
    {
    AE::log info => "#$fn $clienttype $vehicleid msg pingack from $vehicleid";
    return;
    }
  elsif ($code eq 'P') ## PUSH NOTIFICATION
    {
    AE::log info => "#$fn $clienttype $vehicleid msg push notification '$data' => $vehicleid";
    # Send it to any listening apps
    &io_tx_apps($vehicleid, $code, $data);
    # And also send via the mobile networks
    # TODO
    return;
    }

  # The remaining messages are standard

  # Handle paranoid messages
  my $m_paranoid=0;
  my $m_code=$code;
  my $m_data=$data;
  if ($code eq 'E')
    {
    my ($paranoidmsg,$paranoidcode,$paranoiddata,$paranoidtoken)=($1,$3,$4,$2) if ($data =~ /^(.)((.)(.+))$/);
    if ($paranoidmsg eq 'T')
      {
      # The paranoid token is being set
      $conns{$fn}{'ptoken'} = $paranoidtoken;
      &io_tx_apps($vehicleid, $code, $data); # Send it on to connected apps
      # Invalidate any stored paranoid messages for this vehicle
      $db->do("UPDATE ovms_carmessages SET m_valid=0 WHERE vehicleid=? AND m_paranoid=1",undef,$vehicleid);
      AE::log error => "#$fn $clienttype $vehicleid paranoid token set '$paranoidtoken'";
      return;
      }
    elsif ($paranoidmsg eq 'M')
      {
      # A paranoid message is being sent
      $m_paranoid=1;
      $m_code=$paranoidcode;
      $m_data=$paranoiddata;
      }
    else
      {
      # Unknown paranoid msg type
      AE::log error => "#$fn $clienttype $vehicleid unknown paranoid message type '$paranoidmsg'";
      return;
      }
    }

  if ($clienttype eq 'C')
    {
    # Let's store the data in the database...
    my $ptoken = $conns{$fn}{'ptoken'}; $ptoken="" if (!defined $ptoken);
    $db->do("INSERT INTO ovms_carmessages (vehicleid,m_code,m_valid,m_msgtime,m_paranoid,m_ptoken,m_msg) "
          . "VALUES (?,?,1,UTC_TIMESTAMP(),?,?,?) ON DUPLICATE KEY UPDATE "
          . "m_valid=1, m_msgtime=UTC_TIMESTAMP(), m_paranoid=?, m_ptoken=?, m_msg=?",
            undef,
            $vehicleid, $m_code, $m_paranoid, $ptoken, $m_data,
            $m_paranoid, $ptoken, $m_data);
    $db->do("UPDATE ovms_cars SET v_lastupdate=UTC_TIMESTAMP() WHERE vehicleid=?",undef,$vehicleid);
    # And send it on to the apps...
    AE::log info => "#$fn $clienttype $vehicleid msg handle $m_code $m_data";
    &io_tx_apps($vehicleid, $code, $data);
    &io_tx_apps($vehicleid, "T", 0);
    }
  elsif ($clienttype eq 'A')
    {
    # Send it on to the car...
    &io_tx_car($vehicleid, $code, $data);
    }
  }
