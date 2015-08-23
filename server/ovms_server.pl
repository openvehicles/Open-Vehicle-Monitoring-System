#!/usr/bin/perl

use EV;
use AnyEvent;
use AnyEvent::Handle;
use AnyEvent::Socket;
use AnyEvent::HTTP;
use AnyEvent::HTTPD;
use IO::Handle;
use AnyEvent::Log;
use Config::IniFiles;
use DBI;
use Digest::MD5;
use Digest::HMAC;
use Digest::SHA qw(sha256 sha512);
use Crypt::RC4::XS;
use MIME::Base64;
use JSON::XS;
use URI;
use URI::QueryParam;
use URI::Escape;
use Data::UUID;
use HTTP::Parser::XS qw(parse_http_request);
use Socket qw(SOL_SOCKET SO_KEEPALIVE);
use Email::MIME;
use Email::Sender::Simple qw(sendmail);

use constant SOL_TCP => 6;
use constant TCP_KEEPIDLE => 4;
use constant TCP_KEEPINTVL => 5;
use constant TCP_KEEPCNT => 6;

# Global Variables

my $VERSION = "2.1.1-20121216";
my $b64tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
my $itoa64 = './0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz';
my %conns;
my $utilisations;
my %car_conns;
my %app_conns;
my $svr_conns;
my %api_conns;
my %group_msgs;
my %group_subs;
my $db;
my $config;
my %http_request_api_noauth;
my %http_request_api_auth;
my %authfail_notified;

# PUSH notifications
my @apns_queue_sandbox;
my @apns_queue_production;
my @apns_queue;
my $apns_handle;
my $apns_running=0;
my @c2dm_queue;
my $c2dm_handle;
my $c2dm_auth;
my $c2dm_running=0;
my @mail_queue;

# Auto-flush
select STDERR; $|=1;
select STDOUT; $|=1;
$AnyEvent::Log::FILTER->level ("info");

# Configuration
$config = Config::IniFiles->new(-file => 'ovms_server.conf');

# Vehicle Error Code Expansion configurations...
my $vecem = Config::IniFiles->new();

foreach (sort glob 'ovms_server*.vece')
  {
  my $vecef = $_;
  AE::log info => "- - - VECE loading $vecef";
  my $vece = Config::IniFiles->new(-file => $vecef);
  foreach ($vece->Sections())
    {
    my $s = $_;
    $vecem->AddSection($s);
    foreach ($vece->Parameters($s))
      {
      my $p = $_;
      $vecem->newval($s,$p,$vece->val($s,$p));
      }
    }
  }

# Globals
my $timeout_app      = $config->val('server','timeout_app',60*20);
my $timeout_car      = $config->val('server','timeout_car',60*16);
my $timeout_svr      = $config->val('server','timeout_svr',60*60);
my $loghistory_tim   = $config->val('log','history',0);

# A database ticker
$db = DBI->connect($config->val('db','path'),$config->val('db','user'),$config->val('db','pass'));
if (!defined $db)
  {
  AE::log error => "fatal: cannot connect to database ($!)";
  exit(1);
  }
$db->{mysql_auto_reconnect} = 1;
my $dbtim = AnyEvent->timer (after => 60, interval => 60, cb => \&db_tim);

# An APNS ticker
my $apnstim = AnyEvent->timer (after => 1, interval => 1, cb => \&apns_tim);

# A C2DM ticker
my $c2dmtim = AnyEvent->timer (after => 1, interval => 1, cb => \&c2dm_tim);

# A MAIL ticker
my $mail_enabled = $config->val('mail','enabled',0);
my $mail_sender = $config->val('mail','sender','notifications@openvehicles.com');
my $mail_interval = $config->val('mail','interval',10);
if ($mail_enabled eq 1)
  {
  my $mailtim = AnyEvent->timer (after => $mail_interval, interval => $mail_interval, cb => \&mail_tim);
  }

# A utilisation ticker
my $utiltim = AnyEvent->timer (after => 60, interval => 60, cb => \&util_tim);

# Server PUSH tickers
my $svrtim = AnyEvent->timer (after => 30, interval => 30, cb => \&svr_tim);
my $svrtim2 = AnyEvent->timer (after => 300, interval => 300, cb => \&svr_tim2);

# Session cleanup tickers
my $apitim = AnyEvent->timer (after => 10, interval => 10, cb => \&api_tim);

# A server client
my $svr_handle;
my $svr_client_token;
my $svr_client_digest;
my $svr_txcipher;
my $svr_rxcipher;
my $svr_server   = $config->val('master','server');
my $svr_port     = $config->val('master','port',6867);
my $svr_vehicle  = $config->val('master','vehicle');
my $svr_pass     = $config->val('master','password');
if (defined $svr_server)
  {
  &svr_client();
  }

sub io_error
  {
  my ($hdl, $fatal, $msg) = @_;

  my $fn = $hdl->fh->fileno();
  my $vid = $conns{$fn}{'vehicleid'}; $vid='-' if (!defined $vid);
  my $clienttype = $conns{$fn}{'clienttype'}; $clienttype='-' if (!defined $clienttype);
  &log($fn, $clienttype, $vid, "got error $msg");
  &io_terminate($fn,$hdl,$conns{$fn}{'vehicleid'},undef);
  }

sub io_timeout
  {
  my ($hdl) = @_;

  my $fn = $hdl->fh->fileno();
  my $vid = $conns{$fn}{'vehicleid'}; $vid='-' if (!defined $vid);
  my $clienttype = $conns{$fn}{'clienttype'}; $clienttype='-' if (!defined $clienttype);

  # We've got an N second receive data timeout

  # Let's see if this is the initial welcome message negotiation...
  if ($clienttype eq '-')
    {
    # OK, it has been 60 seconds since the client connected, but still no identification
    # Time to shut it down...
    &log($fn, $clienttype, $vid, "timeout due to no initial welcome exchange");
    &io_terminate($fn,$hdl,$vid,undef);
    return;
    }

  # At this point, it is either a car or an app - let's handle the timeout
  my $now = AnyEvent->now;
  my $lastrx = $conns{$fn}{'lastrx'};
  my $lastping = $conns{$fn}{'lastping'};
  if ($clienttype eq 'A')
    {
    if (($lastrx+$timeout_app)<$now)
      {
      # The APP has been unresponsive for timeout_app seconds - time to disconnect it
      &log($fn, $clienttype, $vid, "timeout due app due to inactivity");
      &io_terminate($fn,$hdl,$vid,undef);
      return;
      }
    }
  elsif ($clienttype eq 'S')
    {
    if (($lastrx+$timeout_svr)<$now)
      {
      # The SVR has been unresponsive for timeout_svr seconds - time to disconnect it
      AE::log error => "#$fn $clienttype $vid timeout svr due to inactivity";
      &io_terminate($fn,$hdl,$vid,undef);
      return;
      }
    }
  elsif ($clienttype eq 'C')
    {
    if (($lastrx+$timeout_car)<$now)
      {
      # The CAR has been unresponsive for timeout_car seconds - time to disconnect it
      &log($fn, $clienttype, $vid, "timeout car due to inactivity");
      &io_terminate($fn,$hdl,$vid,undef);
      return;
      }
    if ( (($lastrx+$timeout_car-60)<$now) && (($lastping+300)<$now) )
      {
      # The CAR has been unresponsive for timeout_car-60 seconds - time to ping it
      AE::log info => "#$fn $clienttype $vid ping car (due to lack of response)"
      &io_tx($fn, $conns{$fn}{'handle'}, 'A', 'FA');
      $conns{$fn}{'lastping'} = $now;
      }
    }
  }

sub io_line
  {
  my ($hdl, $line) = @_;

  my $fn = $hdl->fh->fileno();
  my $vid = $conns{$fn}{'vehicleid'}; $vid='-' if (!defined $vid);
  my $clienttype = $conns{$fn}{'clienttype'}; $clienttype='-' if (!defined $clienttype);
  $utilisations{$vid.'-'.$clienttype}{'rx'} += length($line)+2;
  $utilisations{$vid.'-'.$clienttype}{'vid'} = $vid;
  $utilisations{$vid.'-'.$clienttype}{'clienttype'} = $clienttype;
  AE::log info => "#$fn $clienttype $vid rx $line";
  $hdl->push_read(line => \&io_line);
  $conns{$fn}{'lastrx'} = time;

  if ($line =~ /^MP-(\S)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)(\s+(.+))?/)
    {
    my ($clienttype,$protscheme,$clienttoken,$clientdigest,$vehicleid,$rest) = ($1,$2,$3,$4,uc($5),$7);
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
      if (($clienttype eq 'C')&&(!defined $authfail_notified{$vehicleid}))
        {
        $authfail_notified{$vehicleid}=1;
        my $host = $conns{$fn}{'host'};
        &push_queuenotify($vehicleid, 'A', "Vehicle authentication failed ($host)");
        }
      &io_terminate($fn,$hdl,undef,"#$fn $vehicleid error - Incorrect client authentication - aborting connection");
      return;
      }
    else
      {
      if (($clienttype eq 'C')&&(defined $authfail_notified{$vehicleid}))
        {
        delete $authfail_notified{$vehicleid};
        my $host = $conns{$fn}{'host'};
        &push_queuenotify($vehicleid, 'A', "Vehicle authentication successful ($host)");
        }
      }

    # Check server permissions
    if (($clienttype eq 'S')&&($vrec->{'v_type'} ne 'SERVER'))
      {
      &io_terminate($fn,$hdl,undef,"#$fn $vehicleid error - Can't authenticate a car as a server - aborting connection");
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
    $conns{$fn}{'lastping'} = time;

    # Send out server welcome message
    AE::log info => "#$fn $clienttype $vehicleid tx MP-S 0 $servertoken $serverdigest";
    my $towrite = "MP-S 0 $servertoken $serverdigest\r\n";
    $conns{$fn}{'tx'} += length($towrite);
    $hdl->push_write($towrite);

    # Account for it...
    $utilisations{$vehicleid.'-'.$clienttype}{'rx'} += length($line)+2;
    $utilisations{$vehicleid.'-'.$clienttype}{'tx'} += $towrite;
    $utilisations{$vehicleid.'-'.$clienttype}{'vid'} = $vid;
    $utilisations{$vehicleid.'-'.$clienttype}{'clienttype'} = $clienttype;

    # Login...
    &io_login($fn,$hdl,$vehicleid,$clienttype,$rest);
    }
  elsif ($line =~ /^AP-C\s+(\S)\s+(\S+)/)
    {
    my ($protscheme,$apkey) = ($1,$2);
    my $vehicleid = $conns{$fn}{'vehicleid'}; $vehicleid='-' if (!defined $vehicleid);
    if ($protscheme ne '0')
      {
      &io_terminate($fn,$hdl,undef,"#$fn $vehicleid error - Unsupported protection scheme - aborting connection");
      return;
      }
    if (defined $conns{$fn}{'ap_already'})
      {
      AE::log info => "#$fn $vehicleid error - Already auto-provisioned on this connection";
      AE::log info => "#$fn C $vehicleid tx AP-X";
      my $towrite = "AP-X\r\n";
      $conns{$fn}{'tx'} += length($towrite);
      $hdl->push_write($towrite);
      return;
      }
    $conns{$fn}{'ap_already'} = 1;
    my $sth = $db->prepare('SELECT * FROM ovms_autoprovision WHERE ap_key=? and deleted=0');
    $sth->execute($apkey);
    my $row = $sth->fetchrow_hashref();
    if (!defined $row)
      {
      AE::log info => "#$fn $vehicleid info - No auto-provision profile found for $apkey";
      AE::log info => "#$fn C $vehicleid tx AP-X";
      my $towrite = "AP-X\r\n";
      $conns{$fn}{'tx'} += length($towrite);
      $hdl->push_write($towrite);
      return;
      }
    # All ok, let's send the data...
    my $towrite = "AP-S 0 ".join(' ',$row->{'ap_stoken'},$row->{'ap_sdigest'},$row->{'ap_msg'})."\r\n";
    AE::log info => "#$fn C $vehicleid tx AP-S 0 ".join(' ',$row->{'ap_stoken'},$row->{'ap_sdigest'},$row->{'ap_msg'});
    $conns{$fn}{'tx'} += length($towrite);
    $hdl->push_write($towrite);
    }
  elsif (defined $conns{$fn}{'vehicleid'})
    {
    # Let's process this as an encrypted message line...
    my $message = $conns{$fn}{'rxcipher'}->RC4(decode_base64($line));
    if ($message =~ /^MP-0\s(\S)(.*)/)
      {
      my ($code,$data) = ($1,$2);
      &log($fn, $clienttype, $vid, "rx msg $code $data");
      &io_message($fn, $hdl, $conns{$fn}{'vehicleid'}, $vrec, $code, $data);
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
  my ($fn,$hdl,$vehicleid,$clienttype,$rest) = @_;

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
    my $v_ptoken = $vrec->{'v_ptoken'};
    my $sth = $db->prepare('SELECT * FROM ovms_carmessages WHERE vehicleid=? and m_valid=1 order by field(m_code,"S","F") DESC,m_code ASC');
    $sth->execute($vehicleid);
    while (my $row = $sth->fetchrow_hashref())
      {
      if ($row->{'m_paranoid'})
        {
        if ($v_ptoken ne '')
          {
          &io_tx($fn, $hdl, 'E', 'T'.$v_ptoken);
          $v_ptoken = ''; # Make sure it only gets sent once
          }
        &io_tx($fn, $hdl, 'E', 'M'.$row->{'m_code'}.$row->{'m_msg'});
        }
      else
        {
        &io_tx($fn, $hdl, $row->{'m_code'},$row->{'m_msg'});
        }
      }
    &io_tx($fn, $hdl, 'T', $vrec->{'v_lastupdatesecs'});
    }
  elsif ($clienttype eq 'S')
    {
    # An SVR login
    if (defined $svr_conns{$vehicleid})
      {
      # Server is already logged in - terminate it
      &io_terminate($svr_conns{$vehicleid},$conns{$svr_conns{$vehicleid}}{'handle'},$vehicleid, "#$svr_conns{$vehicleid} $vehicleid error - duplicate server login - clearing first connection");
      }
    $svr_conns{$vehicleid} = $fn;
    my ($svrupdate_v,$svrupdate_o) = ($1,$2) if ($rest =~ /^(\S+ \S+) (\S+ \S+)/);
    $conns{$fn}{'svrupdate_v'} = $svrupdate_v;
    $conns{$fn}{'svrupdate_o'} = $svrupdate_o;
    &svr_push($fn,$vehicleid);
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
      # Cleanup group messages
      if (defined $conns{$fn}{'cargroups'})
        {
        foreach (keys %{$conns{$fn}{'cargroups'}})
          {
          delete $group_msgs{$_}{$vehicleid};
          }
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
      # Cleanup group messages
      if (defined $conns{$fn}{'appgroups'})
        {   
        foreach (keys %{$conns{$fn}{'appgroups'}})
          {
          delete $group_subs{$_}{$fn};
          }
        }
      }
    elsif ($conns{$fn}{'clienttype'} eq 'S')
      {
      delete $svr_conns{$vehicleid};
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
  my $encoded = encode_base64($conns{$fn}{'txcipher'}->RC4("MP-0 $code$data"),'');
  AE::log info => "#$fn $clienttype $vid tx $encoded ($code $data)";
  $utilisations{$vid.'-'.$clienttype}{'tx'} += length($encoded)+2 if ($vid ne '-');
  $utilisations{$vid.'-'.$clienttype}{'vid'} = $vid;
  $utilisations{$vid.'-'.$clienttype}{'clienttype'} = $clienttype;
  $handle->push_write($encoded."\r\n");
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
  AE::log info => "#$fn - new ovms connection from $host:$port";
  my $handle; $handle = new AnyEvent::Handle(fh => $fh, on_error => \&io_error, on_rtimeout => \&io_timeout, keepalive => 1, no_delay => 1, rtimeout => 30);
  $handle->push_read (line => \&io_line);

  setsockopt($fh, SOL_SOCKET, SO_KEEPALIVE, 1);
  setsockopt($fh, SOL_TCP, TCP_KEEPCNT, 9);
  setsockopt($fh, SOL_TCP, TCP_KEEPIDLE, 240);
  setsockopt($fh, SOL_TCP, TCP_KEEPINTVL, 240);

  $conns{$fn}{'fh'} = $fh;
  $conns{$fn}{'handle'} = $handle;
  $conns{$fn}{'host'} = $host;
  $conns{$fn}{'port'} = $port;
  };

# An HTTP server
my $http_server = AnyEvent::HTTPD->new (port => 6868, request_timeout => 30, allowed_methods => [GET,PUT,POST,DELETE]);
$http_server->reg_cb (
                '/group' => \&http_request_in_group,
                '/api' => \&http_request_in_api,
                '/file' => \&http_request_in_file,
                '/electricracekml' => \&http_request_in_electricracekml,
                '/electricracekmlfull' => \&http_request_in_electricracekmlfull,
                '/electricrace' => \&http_request_in_electricrace,
                '' => \&http_request_in_root
                );
$http_server->reg_cb (
                client_connected => sub {
                  my ($httpd, $host, $port) = @_;
                    AE::log info => join(' ','http','-','-',$host.':'.$port,'connect');
                  }
                );
$http_server->reg_cb (
                client_disconnected => sub {
                  my ($httpd, $host, $port) = @_;
                    AE::log info => join(' ','http','-','-',$host.':'.$port,'disconnect');
                  }
                );

my $https_server;
if (-e 'ovms_server.pem')
  {
  $https_server = AnyEvent::HTTPD->new (port => 6869, request_timeout => 30, ssl  => { cert_file => "ovms_server.pem" }, allowed_methods => [GET,PUT,POST,DELETE]);
  $https_server->reg_cb (
                   '/group' => \&http_request_in_group,
                   '/api' => \&http_request_in_api,
                   '/file' => \&http_request_in_file,
                   '' => \&http_request_in_root
                   );
  $https_server->reg_cb (
                   client_connected => sub {
                     my ($httpd, $host, $port) = @_;
                       AE::log info => join(' ','http','-','-',$host.':'.$port,'connect(ssl)');
                      }
                   );
  $https_server->reg_cb (
                   client_disconnected => sub {
                     my ($httpd, $host, $port) = @_;
                       AE::log info => join(' ','http','-','-',$host.':'.$port,'disconnect(ssl)');
                     }
                  );
  }

# Main event loop...
EV::loop();

sub util_tim
  {
  CONN: foreach (keys %utilisations)
    {
    my $key = $_;
    my $vid = $utilisations{$key}{'vid'};
    my $clienttype = $utilisations{$key}{'clienttype'};
    next CONN if ((!defined $clienttype)||($clienttype eq '-'));
    next CONN if (!defined $vid);
    my $rx = $utilisations{$key}{'rx'}; $rx=0 if (!defined $rx);
    my $tx = $utilisations{$key}{'tx'}; $tx=0 if (!defined $tx);
    next CONN if (($rx+$tx)==0);
    my ($u_c_rx, $u_c_tx, $u_a_rx, $u_a_tx) = (0,0,0,0);
    if ($clienttype eq 'C')
      {
      $u_c_rx += $tx;
      $u_c_tx += $rx;
      }
    elsif ($clienttype eq 'A')
      {
      $u_a_rx += $tx;
      $u_a_tx += $rx;
      }
    $db->do('INSERT INTO ovms_historicalmessages (vehicleid,h_timestamp,h_recordtype,h_recordnumber,h_data,h_expires) '
          . 'VALUES (?,CONCAT(UTC_DATE()," 00:00:00"),"*-OVM-Utilisation",0,?,UTC_TIMESTAMP()+INTERVAL 1 YEAR) '
          . 'ON DUPLICATE KEY UPDATE h_data=h_data+?',
            undef,
            $vid,$u_c_rx,$u_c_rx);
    $db->do('INSERT INTO ovms_historicalmessages (vehicleid,h_timestamp,h_recordtype,h_recordnumber,h_data,h_expires) '
          . 'VALUES (?,CONCAT(UTC_DATE()," 00:00:00"),"*-OVM-Utilisation",1,?,UTC_TIMESTAMP()+INTERVAL 1 YEAR) '
          . 'ON DUPLICATE KEY UPDATE h_data=h_data+?',
            undef,
            $vid,$u_c_tx,$u_c_tx);
    $db->do('INSERT INTO ovms_historicalmessages (vehicleid,h_timestamp,h_recordtype,h_recordnumber,h_data,h_expires) '
          . 'VALUES (?,CONCAT(UTC_DATE()," 00:00:00"),"*-OVM-Utilisation",2,?,UTC_TIMESTAMP()+INTERVAL 1 YEAR) '
          . 'ON DUPLICATE KEY UPDATE h_data=h_data+?',
            undef,
            $vid,$u_a_rx,$u_a_rx);
    $db->do('INSERT INTO ovms_historicalmessages (vehicleid,h_timestamp,h_recordtype,h_recordnumber,h_data,h_expires) '
          . 'VALUES (?,CONCAT(UTC_DATE()," 00:00:00"),"*-OVM-Utilisation",3,?,UTC_TIMESTAMP()+INTERVAL 1 YEAR) '
          . 'ON DUPLICATE KEY UPDATE h_data=h_data+?',
            undef,
            $vid,$u_a_tx,$u_c_tx);
    }
  %utilisations = ();
  }

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
  if (defined $db)
    {
    $db->do('DELETE FROM ovms_historicalmessages WHERE h_expires<UTC_TIMESTAMP();');
    }
  }

sub db_get_vehicle
  {
  my ($vehicleid) = @_;

  my $sth = $db->prepare('SELECT *,TIME_TO_SEC(TIMEDIFF(UTC_TIMESTAMP(),v_lastupdate)) as v_lastupdatesecs FROM ovms_cars WHERE vehicleid=? AND deleted="0"');
  $sth->execute($vehicleid);
  my $row = $sth->fetchrow_hashref();

  return $row;
  }

# Message handlers
sub io_message
  {
  my ($fn,$handle,$vehicleid,$vrec,$code,$data) = @_;

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
    if ($data =~ /^(.)(.+)/)
      {
      my ($alerttype,$alertmsg) = ($1,$2);
      &push_queuenotify($vehicleid, $alerttype, $alertmsg);
      }
    return;
    }
  elsif ($code eq 'p') ## PUSH SUBSCRIPTION
    {
    my ($appid,$pushtype,$pushkeytype,@vkeys) = split /,/,$data;
    $conns{$fn}{'appid'} = $appid;
    while (scalar @vkeys > 0)
      {
      my $vk_vehicleid = shift @vkeys;
      my $vk_netpass = shift @vkeys;
      my $vk_pushkeyvalue = shift @vkeys;

      my $vk_rec = &db_get_vehicle($vk_vehicleid);
      if ((defined $vk_rec)&&($vk_rec->{'carpass'} eq $vk_netpass))
        {
        AE::log info => "#$fn $clienttype $vehicleid msg push subscription $vk_vehicleid:$pushtype/$pushkeytype => $vk_pushkeyvalue";
        $db->do("INSERT INTO ovms_notifies (vehicleid,appid,pushtype,pushkeytype,pushkeyvalue,lastupdated) "
              . "VALUES (?,?,?,?,?,UTC_TIMESTAMP()) ON DUPLICATE KEY UPDATE "
              . "lastupdated=UTC_TIMESTAMP(), pushkeytype=?, pushkeyvalue=?",
                undef,
                $vk_vehicleid, $appid, $pushtype, $pushkeytype, $vk_pushkeyvalue,
                $pushkeytype,$vk_pushkeyvalue);
        }
      }
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
      if ($vrec->{'v_ptoken'} ne $paranoidtoken)
        {
        # Invalidate any stored paranoid messages for this vehicle
        $db->do("UPDATE ovms_carmessages SET m_valid=0 WHERE vehicleid=? AND m_paranoid=1 AND m_ptoken != ?",undef,$vehicleid,$paranoidtoken);
        $db->do("UPDATE ovms_cars SET v_ptoken=? WHERE vehicleid=?",undef,$paranoidtoken,$vehicleid);
        }
      AE::log info => "#$fn $clienttype $vehicleid paranoid token set '$paranoidtoken'";
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

  # Check for App<->Server<->Car command and response messages...
  if ($m_code eq 'C')
    {
    if ($clienttype ne 'A')
      {
      AE::log info => "#$fn $clienttype $vehicleid msg invalid 'C' message from non-App";
      return;
      }
    if (($m_code eq $code)&&($data =~ /^(\d+)(,(.+))?$/)&&($1 == 30))
      {
      # Special case of an app requesting (non-paranoid) the GPRS data
      my $sth = $db->prepare('SELECT vehicleid,left(h_timestamp,10) AS u_date,group_concat(h_data ORDER BY h_recordnumber) AS data '
                           . 'FROM ovms_historicalmessages WHERE vehicleid=? AND h_recordtype="*-OVM-Utilisation" '
                           . 'GROUP BY vehicleid,u_date,h_recordtype ORDER BY h_timestamp desc LIMIT 90');
      $sth->execute($vehicleid);
      my $rows = $sth->rows;
      my $k = 0;
      while (my $row = $sth->fetchrow_hashref())
        {
        $k++;
        &io_tx($fn, $handle, 'c', sprintf('30,0,%d,%d,%s,%s',$k,$rows,
               $row->{'u_date'},$row->{'data'}));
        }
      if ($rows == 0)
        {
        &io_tx($fn, $handle, 'c', '30,1,No GPRS utilisation data available');
        }
      return;
      }
    elsif (($m_code eq $code)&&($data =~ /^(\d+)(,(.+))?$/)&&($1 == 31))
      {
      # Special case of an app requesting (non-paranoid) the historical data summary
      my ($h_since) = $3;
      $h_since='0000-00-00' if (!defined $h_since);
      my $sth = $db->prepare('SELECT h_recordtype,COUNT(DISTINCT h_recordnumber) AS distinctrecs, COUNT(*) AS recs,SUM(LENGTH(h_recordtype)+LENGTH(h_data)+LENGTH(vehicleid)+20) AS tsize, MIN(h_timestamp) AS first, MAX(h_timestamp) AS last '
                           . 'FROM ovms_historicalmessages WHERE vehicleid=? AND h_timestamp>? GROUP BY h_recordtype ORDER BY h_recordtype;');
      $sth->execute($vehicleid,$h_since);
      my $rows = $sth->rows;
      my $k = 0;
      while (my $row = $sth->fetchrow_hashref())
        {
        $k++;
        &io_tx($fn, $handle, 'c', sprintf('31,0,%d,%d,%s,%d,%d,%d,%s,%s',$k,$rows,
               $row->{'h_recordtype'},
               $row->{'distinctrecs'},
               $row->{'recs'},
               $row->{'tsize'},
               $row->{'first'},
               $row->{'last'}));
        }
      if ($rows == 0)
        {
        &io_tx($fn, $handle, 'c', '31,1,No historical data available');
        }
      return;
      }
    elsif (($m_code eq $code)&&($data =~ /^(\d+)(,(.+))?$/)&&($1 == 32))
      {
      # Special case of an app requesting (non-paranoid) the GPRS data
      my ($h_recordtype,$h_since) = split /,/,$3,2;
      $h_since='0000-00-00' if (!defined $h_since);
      my $sth = $db->prepare('SELECT * FROM ovms_historicalmessages WHERE vehicleid=? AND h_recordtype=? AND h_timestamp>? ORDER BY h_timestamp,h_recordnumber');
      $sth->execute($vehicleid,$h_recordtype,$h_since);
      my $rows = $sth->rows;
      my $k = 0;
      while (my $row = $sth->fetchrow_hashref())
        {
        $k++;
        &io_tx($fn, $handle, 'c', sprintf('32,0,%d,%d,%s,%s,%d,%s',$k,$rows,
               $row->{'h_recordtype'}, $row->{'h_timestamp'}, $row->{'h_recordnumber'}, $row->{'h_data'}));
        }
      if ($rows == 0)
        {
        &io_tx($fn, $handle, 'c', '32,1,No historical data available');
        }
      return;
      }
    &io_tx_car($vehicleid, $code, $data); # Send it on to the car
    return;
    }
  elsif ($m_code eq 'c')
    {
    if ($clienttype ne 'C')
      {
      AE::log info => "#$fn $clienttype $vehicleid msg invalid 'c' message from non-Car";
      return;
      }
    &io_tx_apps($vehicleid, $code, $data); # Send it on to the apps
    return;
    }
  elsif ($m_code eq 'g')
    {
    # A group update message
    my ($groupid,$groupmsg) = split /,/,$data,2;
    $groupid = uc($groupid);
    if ($clienttype eq 'C')
      {
      AE::log info => "#$fn $clienttype $vehicleid msg group update $groupid $groupmsg";
      # Store the update
      $conns{$fn}{'cargroups'}{$groupid} = 1;
      $group_msgs{$groupid}{$vehicleid} = $groupmsg;
      # Notify all the apps
      foreach(keys %{$group_subs{$groupid}})
        {
        my $afn = $_;
        &io_tx($afn, $conns{$afn}{'handle'}, $m_code, join(',',$vehicleid,$groupid,$groupmsg));
        }
      }
    return;
    }
  elsif ($m_code eq 'G')
    {
    # A group subscription message
    my ($groupid,$groupmsg) = split /,/,$data,2;
    $groupid = uc($groupid);
    if ($clienttype eq 'A')
      {
      AE::log info => "#$fn $clienttype $vehicleid msg group subscribe $groupid";
      $conns{$fn}{'appgroups'}{$groupid} = 1;
      $group_subs{$groupid}{$fn} = $fn;
      }
    # Send all outstanding group messages...
    foreach (keys %{$group_msgs{$groupid}})
      {
      my $vehicleid = $_;
      my $msg = $group_msgs{$groupid}{$vehicleid};
      &io_tx($fn, $conns{$fn}{'handle'}, 'g', join(',',$vehicleid,$groupid,$msg));
      }
    return;
    }
  elsif ($m_code eq 'H')
    {
    if ($clienttype ne 'C')
      {
      AE::log info => "#$fn $clienttype $vehicleid msg invalid 'H' message from non-Car";
      return;
      }
    my ($h_recordtype,$h_recordnumber,$h_lifetime,$h_data) = split /,/,$data,4;
    $db->do("INSERT IGNORE INTO ovms_historicalmessages (vehicleid,h_timestamp,h_recordtype,h_recordnumber,h_data,h_expires) "
          . "VALUES (?,UTC_TIMESTAMP(),?,?,?,UTC_TIMESTAMP()+INTERVAL ? SECOND)",
            undef,
            $vehicleid, $h_recordtype, $h_recordnumber, $h_data, $h_lifetime);
    return;
    }
  elsif ($m_code eq 'h')
    {
    if ($clienttype ne 'C')
      {
      AE::log info => "#$fn $clienttype $vehicleid msg invalid 'h' message from non-Car";
      return;
      }
    my ($h_ackcode,$h_timediff,$h_recordtype,$h_recordnumber,$h_lifetime,$h_data) = split /,/,$data,6;
    $db->do("INSERT IGNORE INTO ovms_historicalmessages (vehicleid,h_timestamp,h_recordtype,h_recordnumber,h_data,h_expires) "
          . "VALUES (?,UTC_TIMESTAMP()+INTERVAL ? SECOND,?,?,?,UTC_TIMESTAMP()+INTERVAL ? SECOND)",
            undef,
            $vehicleid, $h_timediff, $h_recordtype, $h_recordnumber, $h_data, $h_lifetime-$h_timediff);
    &io_tx($fn, $conns{$fn}{'handle'}, 'h', $h_ackcode);
    return;
    }

  if ($clienttype eq 'C')
    {
    # Kludge: fix to 1.2.0 bug with S messages in performance mode
    if (($code eq 'S')&&($m_paranoid == 0)&&($data =~ /,performance,,/))
      {
      $data =~ s/,performance,,/,performance,/;
      $m_data =~ s/,performance,,/,performance,/;
      }
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
    if ($m_code eq "F")
      {
      # Let's follow up with server version...
      &io_tx_apps($vehicleid, "f", $VERSION);
      }
    &io_tx_apps($vehicleid, "T", 0);
    }
  elsif ($clienttype eq 'A')
    {
    # Send it on to the car...
    &io_tx_car($vehicleid, $code, $data);
    }
  }

sub svr_tim
  {
  return if (scalar keys %svr_conns == 0);

  # Drupal -> ovms_owners maintenance
  $db->do('INSERT INTO ovms_owners SELECT uid,name,mail,pass,status,0,utc_timestamp() FROM users WHERE users.uid NOT IN (SELECT owner FROM ovms_owners)');
  $db->do('UPDATE ovms_owners LEFT JOIN users ON users.uid=ovms_owners.owner '
        . 'SET ovms_owners.pass=users.pass, ovms_owners.status=users.status, ovms_owners.name=users.name, ovms_owners.mail=users.mail, deleted=0, changed=UTC_TIMESTAMP() '
        . 'WHERE users.pass<>ovms_owners.pass OR users.status<>ovms_owners.status OR users.name<>ovms_owners.name OR users.mail<>ovms_owners.mail');
  $db->do('UPDATE ovms_owners SET deleted=1,changed=UTC_TIMESTAMP() WHERE deleted=0 AND owner NOT IN (SELECT uid FROM users)');

  my %last;
  my $sth = $db->prepare('SELECT v_server,MAX(changed) AS lu FROM ovms_cars WHERE v_type="CAR" GROUP BY v_server');
  $sth->execute();
  while (my $row = $sth->fetchrow_hashref())
    {
    $last{$row->{'v_server'}} = $row->{'lu'};
    }

  my $last_o;
  $sth = $db->prepare('SELECT MAX(changed) as lu FROM ovms_owners');
  $sth->execute();
  while (my $row = $sth->fetchrow_hashref())
    {
    $last_o = $row->{'lu'};
    }

  foreach (keys %svr_conns)
    {
    my $vehicleid = $_;
    my $fn = $svr_conns{$vehicleid};
    my $svrupdate_v = $conns{$fn}{'svrupdate_v'};
    my $svrupdate_o = $conns{$fn}{'svrupdate_o'};
    my $lw = $last{'*'}; $lw='0000-00-00 00:00:00' if (!defined $lw);
    my $ls = $last{$vehicleid}; $ls='0000-00-00 00:00:00' if (!defined $ls);
    if (($lw gt $svrupdate_v)||($ls gt $svrupdate_v)||($last_o gt $svrupdate_o))
      {
      &svr_push($fn,$vehicleid);
      }
    }
  }

sub svr_tim2
  {
  if ((!defined $svr_handle)&&(defined $svr_server))
    {
    &svr_client();
    }
  }

sub svr_push
  {
  my ($fn,$vehicleid) = @_;

  # Push updated cars to the specified server
  return if (!defined $svr_conns{$vehicleid}); # Make sure it is a server

  my $sth = $db->prepare('SELECT * FROM ovms_cars WHERE v_type="CAR" AND v_server IN ("*",?) AND changed>? ORDER BY changed');
  $sth->execute($vehicleid,$conns{$fn}{'svrupdate_v'});
  while (my $row = $sth->fetchrow_hashref())
    {
    &io_tx($fn, $conns{$fn}{'handle'}, 'RV', 
            join(',',$row->{'vehicleid'},$row->{'owner'},$row->{'carpass'},
                     $row->{'v_server'},$row->{'deleted'},$row->{'changed'}));
    $conns{$fn}{'svrupdate_v'} = $row->{'changed'};
    }

  $sth = $db->prepare('SELECT * FROM ovms_owners WHERE changed>? ORDER BY changed');
  $sth->execute($conns{$fn}{'svrupdate_o'});
  while (my $row = $sth->fetchrow_hashref())
    {
    &io_tx($fn, $conns{$fn}{'handle'}, 'RO',
            join(',',$row->{'owner'},$row->{'name'},$row->{'mail'},
                     $row->{'pass'},$row->{'status'},$row->{'deleted'},$row->{'changed'}));
    $conns{$fn}{'svrupdate_o'} = $row->{'changed'};
    }
  }

sub svr_client
  {
  tcp_connect $svr_server, $svr_port, sub
    {
    my ($fh) = @_;

    $svr_handle = new AnyEvent::Handle(fh => $fh, on_error => \&svr_error, on_rtimeout => \&svr_timeout, keepalive => 1, no_delay => 1, rtimeout => 60*60);
    $svr_handle->push_read (line => \&svr_welcome);

    my $sth = $db->prepare('SELECT MAX(changed) AS mc FROM ovms_cars WHERE v_type="CAR"');
    $sth->execute();
    my $row = $sth->fetchrow_hashref();
    my $last_v = $row->{'mc'}; $last_v = '0000-00-00 00:00:00' if (!defined $last_v);

    $sth = $db->prepare('SELECT MAX(changed) AS mc FROM ovms_owners');
    $sth->execute();
    $row = $sth->fetchrow_hashref();
    my $last_o = $row->{'mc'}; $last_o = '0000-00-00 00:00:00' if (!defined $last_o);

    $svr_client_token = '';
    foreach (0 .. 21)
      { $svr_client_token .= substr($b64tab,rand(64),1); }
    my $client_hmac = Digest::HMAC->new($svr_pass, "Digest::MD5");
    $client_hmac->add($svr_client_token);
    $svr_client_digest = $client_hmac->b64digest();
    $svr_handle->push_write("MP-S 0 $svr_client_token $svr_client_digest $svr_vehicle $last_v $last_o\r\n");
    }
  }

sub svr_welcome
  {
  my ($hdl, $line) = @_;

  AE::log info => "#$fn - - svr welcome $line";

  my $fn = $hdl->fh->fileno();

  my ($welcome,$crypt,$server_token,$server_digest) = split /\s+/,$line;

  my $d_server_digest = decode_base64($server_digest);
  my $client_hmac = Digest::HMAC->new($svr_pass, "Digest::MD5");
  $client_hmac->add($server_token);
  if ($client_hmac->digest() ne $d_server_digest)
    {
    AE::log error => "#$fn - - svr server digest is invalid - aborting";
    undef $svr_handle;
    return;
    }

  $client_hmac = Digest::HMAC->new($svr_pass, "Digest::MD5");
  $client_hmac->add($server_token);
  $client_hmac->add($svr_client_token);
  my $client_key = $client_hmac->digest;

  $svr_txcipher = Crypt::RC4::XS->new($client_key);
  $svr_txcipher->RC4(chr(0) x 1024); # Prime the cipher
  $svr_rxcipher = Crypt::RC4::XS->new($client_key);
  $svr_rxcipher->RC4(chr(0) x 1024); # Prime the cipher

  $svr_handle->push_read (line => \&svr_line);
  }

sub svr_line
  {
  my ($hdl, $line) = @_;
  my $fn = $hdl->fh->fileno();

  $svr_handle->push_read (line => \&svr_line);

  my $dline = $svr_rxcipher->RC4(decode_base64($line));
  AE::log info => "#$fn - - svr got $dline";

  if ($dline =~ /^MP-0 A/)
    {
    $svr_handle->push_write(encode_base64($svr_txcipher->RC4("MP-0 a"),''));
    }
  elsif ($dline =~ /MP-0 RV(.+)/)
    {
    my ($vehicleid,$owner,$carpass,$v_server,$deleted,$changed) = split(/,/,$1);
    AE::log info => "#$fn - - svr got vehicle record update $vehicleid ($changed)";

    $db->do('INSERT INTO ovms_cars (vehicleid,owner,carpass,v_server,deleted,changed,v_lastupdate) '
          . 'VALUES (?,?,?,?,?,?,NOW()) '
          . 'ON DUPLICATE KEY UPDATE owner=?, carpass=?, v_server=?, deleted=?, changed=?',
            undef,
            $vehicleid,$owner,$carpass,$v_server,$deleted,$changed,$owner,$carpass,$v_server,$deleted,$changed);
    }
  elsif ($dline =~ /MP-0 RO(.+)/)
    {
    my ($owner,$name,$mail,$pass,$status,$deleted,$changed) = split(/,/,$1);
    AE::log info => "#$fn - - svr got owner record update $owner ($changed)";
  
    $db->do('INSERT INTO ovms_owners (owner,name,mail,pass,status,deleted,changed) '
          . 'VALUES (?,?,?,?,?,?,?) '
          . 'ON DUPLICATE KEY UPDATE name=?, mail=?, pass=?, status=?, deleted=?, changed=?',
            undef,
            $owner,$name,$mail,$pass,$status,$deleted,$changed,
            $name,$mail,$pass,$status,$deleted,$changed);
    }
  }

sub svr_error
  {
  my ($hdl, $fatal, $msg) = @_;
  my $fn = $hdl->fh->fileno();

  AE::log info => "#$fn - - svr got disconnect from remote";

  undef $svr_handle;  
  }

sub svr_timeout
  {
  my ($hdl) = @_;
  my $fn = $hdl->fh->fileno();

  AE::log info => "#$fn - - svr got timeout from remote";

  undef $svr_handle;
  }

sub push_queuenotify
  {
  my ($vehicleid, $alerttype, $alertmsg) = @_;

  if ($alerttype eq 'E')
    {
    # VECE expansion...
    my ($vehicletype,$errorcode,$errordata) = split(/,/,$alertmsg);
    $alertmsg = &vece_expansion($vehicletype,$errorcode,$errordata);
    }

  my $sth = $db->prepare('SELECT * FROM ovms_notifies WHERE vehicleid=? and active=1');
  $sth->execute($vehicleid);
  CANDIDATE: while (my $row = $sth->fetchrow_hashref())
    {
    my %rec;
    $rec{'vehicleid'} = $vehicleid;
    $rec{'alerttype'} = $alerttype;
    $rec{'alertmsg'} = $alertmsg;
    $rec{'pushkeytype'} = $row->{'pushkeytype'};
    $rec{'pushkeyvalue'} = $row->{'pushkeyvalue'};
    $rec{'appid'} = $row->{'appid'};
    foreach (%{$app_conns{$vehicleid}})
      {
      my $fn = $_;
      next CANDIDATE if ($conns{$fn}{'appid'} eq $row->{'appid'}); # Car connected?
      }
    if ($row->{'pushtype'} eq 'apns')
      {
      if ($row->{'pushkeytype'} eq 'sandbox')
        { push @apns_queue_sandbox,\%rec; }
      else
        { push @apns_queue_production,\%rec; }
      AE::log info => "- - $vehicleid msg queued apns notification for $rec{'pushkeytype'}:$rec{'appid'}";
      }
    if ($row->{'pushtype'} eq 'c2dm')
      {
      push @c2dm_queue,\%rec;
      AE::log info => "- - $vehicleid msg queued c2dm notification for $rec{'pushkeytype'}:$rec{'appid'}";
      }
    if ($row->{'pushtype'} eq 'mail' && $mail_enabled eq 1)
      {
      push @mail_queue,\%rec;
      AE::log info => "- - $vehicleid msg queued mail notification for $rec{'pushkeyvalue'}";
      }
    }
  }

sub vece_expansion
  {
  my ($vehicletype,$errorcode,$errordata) = @_;

  my $car = $vehicletype;
  while ($car ne '')
    {
    my $t = $vecem->val($car,$errorcode);
    if (defined $t)
      {
      my $text = sprintf $t,$errordata;
      return "Vehicle Alert #$errorcode: ".$text;
      }
    $car = substr($car,0,-1);
    }

  return sprintf "Vehicle Alert Code: %s/%d (%08x)",$vehicletype,$errorcode,$errordata;
  }

sub apns_send
  {
  my ($token, $payload) = @_;

  my $json = JSON::XS->new->utf8->encode ($payload);

  my $btoken = pack "H*",$token;

  $apns_handle->push_write( pack('C', 0) ); # command

  $apns_handle->push_write( pack('n', bytes::length($btoken)) ); # token length
  $apns_handle->push_write( $btoken );                           # device token

  # Apple Push Notification Service refuses string values as badge number
  if ($payload->{aps}{badge} && looks_like_number($payload->{aps}{badge}))
    {
    $payload->{aps}{badge} += 0;
    }

  # The maximum size allowed for a notification payload is 256 bytes;
  # Apple Push Notification Service refuses any notification that exceeds this limit.
  if ( (my $exceeded = bytes::length($json) - 256) > 0 )
    {
    if (ref $payload->{aps}{alert} eq 'HASH')
      {
      $payload->{aps}{alert}{body} = &_trim_utf8($payload->{aps}{alert}{body}, $exceeded);
      }
    else
      {
      $payload->{aps}{alert} = &_trim_utf8($payload->{aps}{alert}, $exceeded);
      }

    $json = JSON::XS->new->utf8->encode($payload);
    }

  $apns_handle->push_write( pack('n', bytes::length($json)) ); # payload length
  $apns_handle->push_write( $json );                           # payload
  }

sub _trim_utf8
  {
  my ($string, $trim_length) = @_;

  my $string_bytes = JSON::XS->new->utf8->encode($string);
  my $trimmed = '';

  my $start_length = bytes::length($string_bytes) - $trim_length;
  return $trimmed if $start_length <= 0;

  for my $len ( reverse $start_length - 6 .. $start_length )
    {
    local $@;
    eval
      {
      $trimmed = JSON::XS->new->utf8->decode(substr($string_bytes, 0, $len));
      };
    last if $trimmed;
    }

  return $trimmed;
  }

sub apns_push
  {
  my ($hdl, $success, $error_message) = @_;

  my $fn = $hdl->fh->fileno();
  AE::log info => "#$fn - - connected to apns for push notification";

  foreach my $rec (@apns_queue)
    {
    my $vehicleid = $rec->{'vehicleid'};
    my $alerttype = $rec->{'alerttype'};
    my $alertmsg = $rec->{'alertmsg'};
    my $pushkeyvalue = $rec->{'pushkeyvalue'};
    my $appid = $rec->{'appid'};
    AE::log info => "#$fn - $vehicleid msg apns '$alertmsg' => $pushkeyvalue";
    &apns_send( $pushkeyvalue => { aps => { alert => "$vehicleid\n$alertmsg", sound => 'default' } } );
    }
  $apns_handle->on_drain(sub
                {
                my ($hdl) = @_;
                my $fn = $hdl->fh->fileno();
                AE::log info => "#$fn - - msg apns is drained and done";
                undef $apns_handle;
                $apns_running=0;
                });
  }

sub apns_tim
  {
  return if ($apns_running);
  return if ((scalar @apns_queue_sandbox == 0)&&(scalar @apns_queue_production == 0));

  my ($host,$certfile,$keyfile);
  if (scalar @apns_queue_sandbox > 0)
    {
    # We have notifications to deliver for the sandbox
    @apns_queue = @apns_queue_sandbox;
    @apns_queue_sandbox = ();
    $host = 'gateway.sandbox.push.apple.com';
    $certfile = $keyfile = 'ovms_apns_sandbox.pem';
    }
  elsif (scalar @apns_queue_production > 0)
    {
    @apns_queue = @apns_queue_production;
    @apns_queue_production = ();
    $host = 'gateway.push.apple.com';
    $certfile = $keyfile = 'ovms_apns_production.pem';
    }

  AE::log info => "- - - msg apns processing queue for $host";
  $apns_running=1;

  tcp_connect $host, 2195, sub
    {
    my ($fh) = @_;

    $apns_handle = new AnyEvent::Handle(
          fh       => $fh,
          peername => $host,
          tls      => "connect",
          tls_ctx  => { cert_file => $certfile, key_file => $keyfile, verify => 0, verify_peername => $host },
          on_error => sub
                {
                $apns_handle = undef;
                $apns_running = 0;
                $_[0]->destroy;
                },
          on_starttls => \&apns_push
          );
    }
  }

sub c2dm_tim
  {
  if (($c2dm_running == 0)&&(!defined $c2dm_auth))
    {
    return if (scalar @c2dm_queue == 0);

    # OK. First step is we need to get an AUTH token...
    $c2dm_running = 1;
    $c2dm_auth = undef;
    my $c2dm_email = uri_escape($config->val('c2dm','email'));
    my $c2dm_password = uri_escape($config->val('c2dm','password'));
    my $c2dm_type = uri_escape($config->val('c2dm','accounttype'));
    my $body = 'Email='.$c2dm_email.'&Passwd='.$c2dm_password.'&accountType='.$c2dm_type.'&source=openvehicles-ovms-1&service=ac2dm';
    AE::log info => "- - - msg c2dm obtaining auth token for notifications";
    http_request
      POST => 'https://www.google.com/accounts/ClientLogin',
      body => $body,
      headers=>{ "Content-Type" => "application/x-www-form-urlencoded" },
      sub
        {
        my ($data, $headers) = @_;
        foreach (split /\n/,$data)
          {
          $c2dm_auth = $1 if (/^Auth=(.+)/);
          }
        if (!defined $c2dm_auth)
          {
          AE::log error => "- - - msg c2dm could not authenticate to google ($body)";
          @c2dm_queue = ();
          $c2dm_running = 0;
          return;
          }
        $c2dm_running = 2;
        };
    }
  elsif (($c2dm_running == 2)||(($c2dm_running==0)&&(defined $c2dm_auth)&&(scalar @c2dm_queue > 0)))
    {
    $c2dm_running = 2;
    AE::log info => "- - - msg c2dm auth is '$c2dm_auth'";

    foreach my $rec (@c2dm_queue)
      {
      my $vehicleid = $rec->{'vehicleid'};
      my $alerttype = $rec->{'alerttype'};
      my $alertmsg = $rec->{'alertmsg'};
      my $pushkeyvalue = $rec->{'pushkeyvalue'};
      my $appid = $rec->{'appid'};
      AE::log info => "#$fn - $vehicleid msg c2dm '$alertmsg' => $pushkeyvalue";
      my $body = 'registration_id='.uri_escape($pushkeyvalue)
                .'&data.title='.uri_escape($vehicleid)
                .'&data.message='.uri_escape($alertmsg)
                .'&collapse_key='.time;
      http_request
        POST=>'https://android.apis.google.com/c2dm/send',
        body => $body,
        headers=>{ 'Authorization' => 'GoogleLogin auth='.$c2dm_auth,
                   "Content-Type" => "application/x-www-form-urlencoded" },
        sub
          {
          my ($data, $headers) = @_;
          foreach (split /\n/,$data)
            { AE::log info => "- - - msg c2dm message sent ($_)"; }
          };
      }
    @c2dm_queue = ();
    $c2dm_running = 0;
    }
  }

sub mail_tim
  {
  foreach my $rec (@mail_queue)
    {
    my $vehicleid = $rec->{'vehicleid'};
    my $alerttype = $rec->{'alerttype'};
    my $alertmsg = $rec->{'alertmsg'};
    my $pushkeyvalue = $rec->{'pushkeyvalue'};
    if ($pushkeyvalue =~ /@/)
      {
      AE::log info => "#$fn - $vehicleid msg mail '$alertmsg' => '$pushkeyvalue'";
      my $message = Email::MIME->create(
        header_str => [
          From    => $mail_sender,
          To      => $pushkeyvalue,
          Subject => "OVMS notification from $vehicleid",
        ],
        attributes => {
          encoding => 'quoted-printable',
          charset  => 'ISO-8859-1',
        },
        body_str => $alertmsg,
      );
      sendmail($message);
      }
    }
  @mail_queue = ();
  }

sub http_request_in_root
  {
  my ($httpd, $req) = @_;

  AE::log info => join(' ','http','-','-',$req->client_host.':'.$req->client_port,'-',$req->method,$req->url);

  $req->respond ( [404, 'not found', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "not found\n"] );
  $httpd->stop_request;
  }


# GET     /api/cookie                             Login and return a session cookie
INIT { $http_request_api_noauth{'GET:cookie'} = \&http_request_api_cookie_login; }
sub http_request_api_cookie_login
  {
  my ($httpd,$req,$session,@rest) = @_;

  my $username = $req->url->query_param('username');
  my $password = $req->url->query_param('password');

  if ((defined $username)&&(defined $password))
    {
    my $sth = $db->prepare('SELECT * FROM ovms_owners WHERE `name`=? and `status`=1 AND deleted="0000-00-00 00:00:00"');
    $sth->execute($username);
    my $row = $sth->fetchrow_hashref();
    if (defined $row)
      {
      my $passwordhash = $row->{'pass'};
      if (&drupal_password_check($passwordhash, $password))
        {
        # Password ok
        my $ug = new Data::UUID;
        my $sessionid =  $ug->create_str();

        $api_conns{$sessionid}{'username'} = $username;
        $api_conns{$sessionid}{'owner'} = $row->{'owner'};
        $api_conns{$sessionid}{'mail'} = $row->{'name'};
        $api_conns{$sessionid}{'sessionused'} = AnyEvent->now;

        $sth = $db->prepare('SELECT * FROM ovms_cars WHERE owner=? AND deleted=0');
        $sth->execute($row->{'owner'});
        while (my $row = $sth->fetchrow_hashref())
          {
          $api_conns{$sessionid}{'vehicles'}{$row->{'vehicleid'}} = 0;
          }

        AE::log info => join(' ','http','-',$sessionid,$req->client_host.':'.$req->client_port,'session created');

        $req->respond (  [200, 'Authentication ok', { 'Content-Type' => 'text/plain', 'Set-Cookie' => "ovmsapisession=$sessionid", 'Access-Control-Allow-Origin' => '*' }, "Login ok\n"] );
        $httpd->stop_request;
        return;
        }
      }
    }

  $req->respond ( [404, 'Authentication failed', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Authentication failed\n"] );
  $httpd->stop_request;
  }

# DELETE  /api/cookie                             Delete the session cookie and logout
INIT { $http_request_api_auth{'DELETE:cookie'} = \&http_request_api_cookie_logout; }
sub http_request_api_cookie_logout
  {
  my ($httpd,$req,$session,@rest) = @_;

  delete $api_conns{$session};

  AE::log info => join(' ','http','-',$session,$req->client_host.':'.$req->client_port,'session destroyed (on request)');

  $req->respond ( [200, 'Logout ok', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Logout ok\n"] );
  $httpd->stop_request;
  }

# GET     .api/vehicles                           Return alist of registered vehicles
INIT { $http_request_api_auth{'GET:vehicles'} = \&http_request_api_vehicles; }
sub http_request_api_vehicles
  {
  my ($httpd,$req,$session,@rest) = @_;

  my @result;
  foreach (sort keys %{$api_conns{$session}{'vehicles'}})
    {
    my $vehicleid = $_;
    my $connected = (defined $car_conns{$vehicleid})?1:0;
    my $appcount = scalar keys %{$app_conns{$vehicleid}};

    my %h = ( 'id'=>$vehicleid, 'v_net_connected'=>$connected, 'v_apps_connected'=>$appcount );
    push @result, \%h;
    }

  my $json = JSON::XS->new->utf8->canonical->encode (\@result) . "\n";
  $req->respond ( [200, 'Logout ok', { 'Content-Type' => 'application/json', 'Access-Control-Allow-Origin' => '*' }, $json] );
  $httpd->stop_request;
  }


# GET     /api/protocol/<VEHICLEID>               Return raw protocol records (no vehicle connection)
INIT { $http_request_api_auth{'GET:protocol'} = \&http_request_api_protocol; }
sub http_request_api_protocol
  {
  my ($httpd,$req,$session,@rest) = @_;

  my ($vehicleid) = @rest;

  if ((!defined $vehicleid)||(!defined $api_conns{$session}{'vehicles'}{$vehicleid}))
    {
    AE::log info => join(' ','http','-',$session,$req->client_host.':'.$req->client_port,'Forbidden access',$vehicleid);
    $req->respond ( [404, 'Forbidden', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Forbidden\n"] );
    $httpd->stop_request;
    return;
    }

  my @result;
  my $sth = $db->prepare('SELECT * FROM ovms_carmessages WHERE vehicleid=? and m_valid=1 order by field(m_code,"S","F") DESC,m_code ASC');
  $sth->execute($vehicleid);
  while (my $row = $sth->fetchrow_hashref())
    {
    my %h;
    foreach (qw(m_msgtime m_paranoid m_ptoken m_code m_msg))
      {
      $h{$_} = $row->{$_};
      }
    push @result, \%h;
    }

  my $json = JSON::XS->new->utf8->canonical->encode (\@result) . "\n";
  $req->respond ( [200, 'Logout ok', { 'Content-Type' => 'application/json', 'Access-Control-Allow-Origin' => '*' }, $json] );
  $httpd->stop_request;
  }

# GET     /api/vehicle/<VEHICLEID>                Connect to, and return vehicle information
INIT { $http_request_api_auth{'GET:vehicle'} = \&http_request_api_vehicle_get; }
sub http_request_api_vehicle_get
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# DELETE  /api/vehicle/<VEHICLEID>                Disconnect from vehicle
INIT { $http_request_api_auth{'DELETE:vehicle'} = \&http_request_api_vehicle_delete; }
sub http_request_api_vehicle_delete
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );    
  $httpd->stop_request;
  }

# GET	/api/status/<VEHICLEID>			Return vehicle status
INIT { $http_request_api_auth{'GET:status'} = \&http_request_api_status; }
sub http_request_api_status
  {
  my ($httpd,$req,$session,@rest) = @_;

  my ($vehicleid) = @rest;

  if ((!defined $vehicleid)||(!defined $api_conns{$session}{'vehicles'}{$vehicleid}))
    {
    AE::log info => join(' ','http','-',$session,$req->client_host.':'.$req->client_port,'Forbidden access',$vehicleid);
    $req->respond ( [404, 'Forbidden', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Forbidden\n"] );
    $httpd->stop_request;
    return;
    }

  my $rec = &api_vehiclerecord($vehicleid,'S');
  my %result;
  if (defined $rec)
    {
    if ($rec->{'m_paranoid'})
      {
      $req->respond ( [404, 'Vehicle is paranoid', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Paranoid vehicles not supported by api\n"] );
      $httpd->stop_request;
      return;
      }
    my ($soc,$units,$linevoltage,$chargecurrent,$chargestate,$chargemode,$idealrange,$estimatedrange,
        $chargelimit,$chargeduration,$chargeb4,$chargekwh,$chargesubstate,$chargestateN,$chargemodeN,
        $chargetimer,$chargestarttime,$chargetimerstale) = split /,/,$rec->{'m_msg'};
    $result{'soc'} = $soc;
    $result{'units'} = $units;
    $result{'idealrange'} = $idealrange;
    $result{'estimatedrange'} = $estimatedrange,
    $result{'mode'} = $chargemode;
    }
  $rec= &api_vehiclerecord($vehicleid,'D');
  if (defined $rec)
    {
    if (! $rec->{'m_paranoid'})
      {
      my ($doors1,$doors2,$lockunlock,$tpem,$tmotor,$tbattery,$trip,$odometer,$speed,$parktimer,$ambient,
          $doors3,$staletemps,$staleambient,$vehicle12v,$doors4) = split /,/,$rec->{'m_msg'};
      $result{'fl_dooropen'} =   $doors1 & 0b00000001;
      $result{'fr_dooropen'} =   $doors1 & 0b00000010;
      $result{'cp_dooropen'} =   $doors1 & 0b00000100;
      $result{'pilotpresent'} =  $doors1 & 0b00001000;
      $result{'charging'} =      $doors1 & 0b00010000;
      $result{'handbrake'} =     $doors1 & 0b01000000;
      $result{'caron'} =         $doors1 & 0b10000000;
      $result{'carlocked'} =     $doors2 & 0b00001000;
      $result{'valetmode'} =     $doors2 & 0b00010000;
      $result{'bt_open'} =       $doors2 & 0b01000000;
      $result{'tr_open'} =       $doors2 & 0b10000000;
      $result{'temperature_pem'} = $tpem;
      $result{'temperature_motor'} = $tmotor;
      $result{'temperature_battery'} = $tbattery;
      $result{'tripmeter'} = $trip;
      $result{'odometer'} = $odometer;
      $result{'speed'} = $speed;
      $result{'parkingtimer'} = $parktimer;
      $result{'temperature_ambient'} = $ambient;
      $result{'carawake'} =      $doors3 & 0b00000010;
      $result{'staletemps'} = $staletemps;
      $result{'staleambient'} = $staleambient;
      $result{'vehicle12v'} = $vehicle12v;
      $result{'alarmsounding'} = $doors4 & 0b00000100;
      }
    }

  my $json = JSON::XS->new->utf8->canonical->encode (\%result) . "\n";
  $req->respond ( [200, 'Vehicle Status', { 'Content-Type' => 'application/json', 'Access-Control-Allow-Origin' => '*' }, $json] );
  $httpd->stop_request;
  }

# GET	/api/tpms/<VEHICLEID>			Return tpms status
INIT { $http_request_api_auth{'GET:tpms'} = \&http_request_api_tpms; }
sub http_request_api_tpms
  {
  my ($httpd,$req,$session,@rest) = @_;

  my ($vehicleid) = @rest;

  if ((!defined $vehicleid)||(!defined $api_conns{$session}{'vehicles'}{$vehicleid}))
    {
    AE::log info => join(' ','http','-',$session,$req->client_host.':'.$req->client_port,'Forbidden access',$vehicleid);
    $req->respond ( [404, 'Forbidden', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Forbidden\n"] );
    $httpd->stop_request;
    return;
    }

  my $rec = &api_vehiclerecord($vehicleid,'W');
  my %result;
  if (defined $rec)
    {
    if ($rec->{'m_paranoid'})
      {
      $req->respond ( [404, 'Vehicle is paranoid', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Paranoid vehicles not supported by api\n"] );
      $httpd->stop_request;
      return;
      }
    my ($fr_pressure,$fr_temp,$rr_pressure,$rr_temp,$fl_pressure,$fl_temp,$rl_pressure,$rl_temp,$staletpms) = split /,/,$rec->{'m_msg'};
    $result{'fr_pressure'} = $fr_pressure;
    $result{'fr_temperature'} = $fr_temp;
    $result{'rr_pressure'} = $rr_pressure;
    $result{'rr_temperature'} = $rr_temperature;
    $result{'fl_pressure'} = $fl_pressure;
    $result{'fl_temperature'} = $fl_temperature;
    $result{'rl_pressure'} = $rl_pressure;
    $result{'rl_temperature'} = $rl_temperature;
    $result{'staletpms'} = $staletpms;
    }

  my $json = JSON::XS->new->utf8->canonical->encode (\%result) . "\n";
  $req->respond ( [200, 'TPMS', { 'Content-Type' => 'application/json', 'Access-Control-Allow-Origin' => '*' }, $json] );
  $httpd->stop_request;
  }

# GET	/api/location/<VEHICLEID>		Return vehicle location
INIT { $http_request_api_auth{'GET:location'} = \&http_request_api_location; }
sub http_request_api_location
  {
  my ($httpd,$req,$session,@rest) = @_;

  my ($vehicleid) = @rest;

  if ((!defined $vehicleid)||(!defined $api_conns{$session}{'vehicles'}{$vehicleid}))
    {
    AE::log info => join(' ','http','-',$session,$req->client_host.':'.$req->client_port,'Forbidden access',$vehicleid);
    $req->respond ( [404, 'Forbidden', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Forbidden\n"] );
    $httpd->stop_request;
    return;
    }

  my $rec = &api_vehiclerecord($vehicleid,'L');
  my %result;
  if (defined $rec)
    {
    if ($rec->{'m_paranoid'})
      {
      $req->respond ( [404, 'Vehicle is paranoid', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Paranoid vehicles not supported by api\n"] );
      $httpd->stop_request;
      return;
      }
    my ($latitude,$longitude,$direction,$altitude,$gpslock,$stalegps) = split /,/,$rec->{'m_msg'};
    $result{'latitude'} = $latitude;
    $result{'longitude'} = $longitude;
    $result{'direction'} = $direction;
    $result{'altitude'} = $altitude;
    $result{'gpslock'} = $gpslock;
    $result{'stalegps'} = $stalegps;
    }

  my $json = JSON::XS->new->utf8->canonical->encode (\%result) . "\n";
  $req->respond ( [200, 'Location', { 'Content-Type' => 'application/json', 'Access-Control-Allow-Origin' => '*' }, $json] );
  $httpd->stop_request;
  }

# GET	/api/charge/<VEHICLEID>			Return vehicle charge status
INIT { $http_request_api_auth{'GET:charge'} = \&http_request_api_charge_get; }
sub http_request_api_charge_get
  {
  my ($httpd,$req,$session,@rest) = @_;

  my ($vehicleid) = @rest;

  if ((!defined $vehicleid)||(!defined $api_conns{$session}{'vehicles'}{$vehicleid}))
    {
    AE::log info => join(' ','http','-',$session,$req->client_host.':'.$req->client_port,'Forbidden access',$vehicleid);
    $req->respond ( [404, 'Forbidden', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Forbidden\n"] );
    $httpd->stop_request;
    return;
    }
  
  my $rec = &api_vehiclerecord($vehicleid,'S');
  my %result;
  if (defined $rec)
    {
    if ($rec->{'m_paranoid'})
      {
      $req->respond ( [404, 'Vehicle is paranoid', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Paranoid vehicles not supported by api\n"] );
      $httpd->stop_request;
      return;
      }
    my ($soc,$units,$linevoltage,$chargecurrent,$chargestate,$chargemode,$idealrange,$estimatedrange,
        $chargelimit,$chargeduration,$chargeb4,$chargekwh,$chargesubstate,$chargestateN,$chargemodeN,
        $chargetimer,$chargestarttime,$chargetimerstale) = split /,/,$rec->{'m_msg'};
    $result{'linevoltage'} = $linevoltage;
    $result{'chargecurrent'} = $chargecurrent;
    $result{'chargestate'} = $chargestate;
    $result{'soc'} = $soc;
    $result{'units'} = $units;
    $result{'idealrange'} = $idealrange;
    $result{'estimatedrange'} = $estimatedrange,
    $result{'mode'} = $chargemode;
    $result{'chargelimit'} = $chargelimit;
    $result{'chargeduration'} = $chargeduration;
    $result{'chargekwh'} = $chargekwh;
    $result{'chargetimermode'} = $chargetimer;
    $result{'chargestarttime'} = $chargestarttime;
    $result{'chargetimerstale'} = $chargetimerstale;
    }
  $rec= &api_vehiclerecord($vehicleid,'D');
  if (defined $rec)
    {
    if (! $rec->{'m_paranoid'})
      {
      my ($doors1,$doors2,$lockunlock,$tpem,$tmotor,$tbattery,$trip,$odometer,$speed,$parktimer,$ambient,
          $doors3,$staletemps,$staleambient,$vehicle12v,$doors4) = split /,/,$rec->{'m_msg'};
      $result{'cp_dooropen'} =   $doors1 & 0b00000100;
      $result{'pilotpresent'} =  $doors1 & 0b00001000;
      $result{'charging'} =      $doors1 & 0b00010000;
      $result{'caron'} =         $doors1 & 0b10000000;
      $result{'temperature_pem'} = $tpem;
      $result{'temperature_motor'} = $tmotor;
      $result{'temperature_battery'} = $tbattery;
      $result{'temperature_ambient'} = $ambient;
      $result{'carawake'} =      $doors3 & 0b00000010;
      $result{'staletemps'} = $staletemps;
      $result{'staleambient'} = $staleambient;
      }
    }

  my $json = JSON::XS->new->utf8->canonical->encode (\%result) . "\n";
  $req->respond ( [200, 'Location', { 'Content-Type' => 'application/json', 'Access-Control-Allow-Origin' => '*' }, $json] );
  $httpd->stop_request;
  }

# PUT	/api/charge/<VEHICLEID>			Set vehicle charge status
INIT { $http_request_api_auth{'PUT:charge'} = \&http_request_api_charge_put; }
sub http_request_api_charge_put
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# DELETE	/api/charge/<VEHICLEID>			Abort a vehicle charge
INIT { $http_request_api_auth{'DELETE:charge'} = \&http_request_api_charge_delete; }
sub http_request_api_charge_delete
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# GET	/api/lock/<VEHICLEID>			Return vehicle lock status
INIT { $http_request_api_auth{'GET:lock'} = \&http_request_api_lock_get; }
sub http_request_api_locK_get
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# PUT	/api/lock/<VEHICLEID>			Lock a vehicle
INIT { $http_request_api_auth{'PUT:lock'} = \&http_request_api_lock_put; }
sub http_request_api_lock_put
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# DELETE	/api/lock/<VEHICLEID>			Unlock a vehicle
INIT { $http_request_api_auth{'DELETE:lock'} = \&http_request_api_lock_delete; }
sub http_request_api_lock_delete
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# GET	/api/valet/<VEHICLEID>			Return valet status
INIT { $http_request_api_auth{'GET:valet'} = \&http_request_api_valet_get; }
sub http_request_api_valet_get
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# PUT	/api/valet/<VEHICLEID>			Enable valet mode
INIT { $http_request_api_auth{'PUT:valet'} = \&http_request_api_valet_put; }
sub http_request_api_valet_put
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# DELETE	/api/valet/<VEHICLEID>			Disable valet mode
INIT { $http_request_api_auth{'DELETE:valet'} = \&http_request_api_valet_delete; }
sub http_request_api_valet_delete
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# GET	/api/features/<VEHICLEID>		Return vehicle features
INIT { $http_request_api_auth{'GET:features'} = \&http_request_api_features_get; }
sub http_request_api_features_get
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# PUT	/api/feature/<VEHICLEID>		Set a vehicle feature
INIT { $http_request_api_auth{'PUT:features'} = \&http_request_api_features_put; }
sub http_request_api_features_put
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# GET	/api/parameters/<VEHICLEID>		Return vehicle parameters
INIT { $http_request_api_auth{'GET:parameters'} = \&http_request_api_parameters_get; }
sub http_request_api_parameters_get
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# PUT	/api/parameter/<VEHICLEID>		Set a vehicle parameter
INIT { $http_request_api_auth{'PUT:parameters'} = \&http_request_api_parameters_put; }
sub http_request_api_parameters_put
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }


# PUT	/api/reset/<VEHICLEID>			Reset the module in a particular vehicle
INIT { $http_request_api_auth{'PUT:reset'} = \&http_request_api_reset; }
sub http_request_api_reset
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# PUT	/api/homelink/<VEHICLEID>		Activate home link
INIT { $http_request_api_auth{'PUT:homelink'} = \&http_request_api_homelink; }
sub http_request_api_homelink
  {
  my ($httpd,$req,$session,@rest) = @_;

  $req->respond ( [404, 'Not yet implemented', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Not yet implemented\n"] );
  $httpd->stop_request;
  }

# GET	/api/historical/<VEHICLEID>		Request historical data summary
# GET     /api/historical/<VEHICLEID>/<DATATYPE>  Request historical data records
INIT { $http_request_api_auth{'GET:historical'} = \&http_request_api_historical; }
sub http_request_api_historical
  {
  my ($httpd,$req,$session,@rest) = @_;

  my ($vehicleid,$datatype) = @rest;

  if ((!defined $vehicleid)||(!defined $api_conns{$session}{'vehicles'}{$vehicleid}))
    {
    AE::log info => join(' ','http','-',$session,$req->client_host.':'.$req->client_port,'Forbidden access',$vehicleid);
    $req->respond ( [404, 'Forbidden', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Forbidden\n"] );
    $httpd->stop_request;
    return;
    }

  my @result;
  if (!defined $datatype)
    {
    # A Request for the historical data summary
    my $sth = $db->prepare('SELECT h_recordtype,COUNT(DISTINCT h_recordnumber) AS distinctrecs, COUNT(*) AS totalrecs,SUM(LENGTH(h_recordtype)+LENGTH(h_data)+LENGTH(vehicleid)+20) AS totalsize, MIN(h_timestamp) AS first, MAX(h_timestamp) AS last '
                         . 'FROM ovms_historicalmessages WHERE vehicleid=? GROUP BY h_recordtype ORDER BY h_recordtype;');
    $sth->execute($vehicleid);
    my $rows = $sth->rows;
    while (my $row = $sth->fetchrow_hashref())
      {
      my %h;
      foreach (qw(h_recordtype distinctrecs totalrecs totalsize first last))
        {
        $h{$_} = $row->{$_};
        }
      push @result, \%h;      
      }
    }
  else
    {
    # A request for a specific type of historical data
    my $sth = $db->prepare('SELECT * FROM ovms_historicalmessages WHERE vehicleid=? AND h_recordtype=? ORDER BY h_timestamp,h_recordnumber');
    $sth->execute($vehicleid,$datatype);
    while (my $row = $sth->fetchrow_hashref())
      {
      my %h;
      foreach (qw(h_timestamp h_recordnumber h_data))
        {
        $h{$_} = $row->{$_};
        }
      push @result, \%h;
      }
    }

  my $json = JSON::XS->new->utf8->canonical->encode (\@result) . "\n";
  $req->respond ( [200, 'Historical Data', { 'Content-Type' => 'application/json', 'Access-Control-Allow-Origin' => '*' }, $json] );
  $httpd->stop_request;
  }

sub http_request_in_api
  {
  my ($httpd, $req) = @_;

  my $method = $req->method;
  my $path = $req->url->path;
  my @paths = $req->url->path_segments;
  my $headers = $req->headers;

  my $cookie = $headers->{'cookie'};
  my $session = '-';
  COOKIEJAR: foreach (split /;\s+/,$cookie)
    {
    if (/^ovmsapisession=(.+)/)
      {
      $session = $1;
      last COOKIEJAR;
      }
    }

  if ($paths[0] eq '')
    {
    shift @paths; # Skip '' root
    shift @paths; # Skip 'api'
    my $fn = shift @paths;
    my $fnc = $http_request_api_noauth{uc($method) . ':' . $fn};
    if (defined $fnc)
      {
      AE::log info => join(' ','http','-',$session,$req->client_host.':'.$req->client_port,'ok',$req->method,join('/',$req->url->path_segments));
      &$fnc($httpd, $req, undef, @paths);
      return;
      }
    if ((defined $session)&&($session ne '-')&&(defined $api_conns{$session}))
      {
      $api_conns{$session}{'sessionused'} = AnyEvent->now;
      my $fnc = $http_request_api_auth{uc($method) . ':' . $fn};
      if (defined $fnc)
        {
        AE::log info => join(' ','http','-',$session,$req->client_host.':'.$req->client_port,'ok',$req->method,join('/',$req->url->path_segments));
        &$fnc($httpd, $req, $session, @paths);
        return;
        }
      }
    else
      {
      AE::log info => join(' ','http','-',$session,$req->client_host.':'.$req->client_port,'authfail',$req->method,join('/',$req->url->path_segments));
      $req->respond ( [404, 'Authentication failed', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Authentication failed\n"] );
      $httpd->stop_request;
      return;
      }
    }

  AE::log info => join(' ','http','-',$session,$req->client_host.':'.$req->client_port,'noapi',$req->method,join('/',$req->url->path_segments));
  $req->respond ( [404, 'Unrecongised API call', { 'Content-Type' => 'text/plain', 'Access-Control-Allow-Origin' => '*' }, "Unrecognised API call\n"] );
  $httpd->stop_request;
  }

sub api_tim
  {
  foreach my $session (keys %api_conns)
    {
    my $lastused = $api_conns{$session}{'sessionused'};
    my $expire = AnyEvent->now - 120;

    if ($lastused < $expire)
      {
      delete $api_conns{$session};
      AE::log info => join(' ','http','-',$session,'-','session timeout');
      }
    }
  }

sub api_vehiclerecord
  {
  my ($vehicleid,$code) = @_;

  my $sth = $db->prepare('SELECT * FROM ovms_carmessages WHERE vehicleid=? and m_valid=1 and m_code=?');
  $sth->execute($vehicleid,$code);
  my $row = $sth->fetchrow_hashref();
  return $row;
  }

sub http_request_in_electricracekml
  {
  my ($httpd, $req) = @_;

  AE::log info => join(' ','http','-','-',$req->client_host.':'.$req->client_port,'-',$req->method,$req->url);

  open my $er, 'electricrace/electricrace.kml'
    or $req->respond (
                     [404, 'not found', { 'Content-Type' => 'text/plain' }, 'not found']
                     );

  $req->respond ({ content => ['application/vnd.google-earth.kml+xml', do { local $/; <$er> }] });

  $httpd->stop_request;
  }

sub http_request_in_electricracekmlfull
  {
  my ($httpd, $req) = @_;

  AE::log info => join(' ','http','-','-',$req->client_host.':'.$req->client_port,'-',$req->method,$req->url);

  open my $er, 'electricrace/electricrace.kmlfull'
    or $req->respond (
                     [404, 'not found', { 'Content-Type' => 'text/plain' }, 'not found']
                     );

  $req->respond ({ content => ['application/vnd.google-earth.kml+xml', do { local $/; <$er> }] });

  $httpd->stop_request;
  }

sub http_request_in_electricrace
  {
  my ($httpd, $req) = @_;

  AE::log info => join(' ','http','-','-',$req->client_host.':'.$req->client_port,'-',$req->method,$req->url);

  open my $er, 'electricrace/electricrace.html'
    or $req->respond (
                     [404, 'not found', { 'Content-Type' => 'text/plain' }, 'not found']
                     );

  $req->respond ({ content => ['text/html', do { local $/; <$er> }] });

  $httpd->stop_request;
  }

sub http_request_in_file
  {
  my ($httpd, $req) = @_;

  AE::log info => join(' ','http','-','-',$req->client_host.':'.$req->client_port,'-',$req->method,$req->url);

  my $filepath = $1 if ($req->url =~ /^\/file\/([a-zA-Z0-9\-\_\.]+)$/);

  if ((defined $filepath)&&(-f "httpfiles/$filepath"))
    {
    open my $fp,'<',"httpfiles/$filepath";
    $_ = <$fp>; chop;
    my $contenttype = $_;
    AE::log info => join(' ','http','-','-',$req->client_host.':'.$req->client_port,'-','start file transfer');
    $req->respond ({ content => [$contenttype, sub {
      my ($data_cb) = @_;

      if (!defined $data_cb)
        {
        AE::log info => join(' ','http','-','-',$req->client_host.':'.$req->client_port,'-','finished file transfer');
        close $fp;
        return;
        }
      else
        {
        my $buf;
        read $fp,$buf,16384;
        AE::log info => join(' ','http','-','-',$req->client_host.':'.$req->client_port,'-','file transfer blob: ',length($buf),'bytes');
        &$data_cb($buf);
        }
      } ]});
    }
  else
    {
    $req->respond (
                    [404, 'not found', { 'Content-Type' => 'text/plain' }, "not found\n"]
                 );
    }

  $httpd->stop_request;
  }

sub http_request_in_group
  {
  my ($httpd, $req) = @_;

  AE::log info => join(' ','http','-','-',$req->client_host.':'.$req->client_port,'-',$req->method,$req->url);

  my $id = $req->parm('id');

  my @result;

  push @result,<<"EOT";
<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2">
<Document>
  <name>Open Vehicles KML</name>
  <Style id="icon">
    <IconStyle>
      <Icon>
        <href>http://www.stegen.com/pub/teslapin.png</href>
      </Icon>
    </IconStyle>
  </Style>
EOT

if (defined $group_msgs{$id})
  {
  foreach (sort keys %{$group_msgs{$id}})
    {
    my ($vehicleid,$groupmsg) = ($_,$group_msgs{$id}{$_});
    my ($soc,$speed,$direction,$altitude,$gpslock,$stalegps,$latitude,$longitude) = split(/,/,$groupmsg);

    push @result,<<"EOT";
  <Placemark>
    <name>$vehicleid</name>
    <description>$vehicleid</description>
    <styleUrl>#icon</styleUrl>
    <Point>
      <coordinates>$longitude,$latitude</coordinates>
    </Point>
  </Placemark>
EOT
    }
  }

  push @result,<<"EOT";
</Document>
</kml>
EOT

  $req->respond([
      200, 'OK', {
        'Content-Type'  => 'Content-Type: application/vnd.google-earth.kml+xml'
      },
      join("\n",@result)
   ]);
  $httpd->stop_request;
  }

sub drupal_password_check
  {
  my ($ph,$password) = @_;

  my $iter_log2 = index($itoa64,substr($ph,3,1));
  my $iter_count = 1 << $iter_log2;

  my $phash = substr($ph,0,12);
  my $salt = substr($ph,4,8);

  my $hash = sha512($salt.$password);
  do
    {
    $hash = sha512($hash.$password);
    $iter_count--;
    } while ($iter_count > 0);

  my $encoded = substr($phash . &drupal_password_base64_encode($hash,length($hash)),0,55);

  return ($encoded eq $ph);
  }

sub drupal_password_base64_encode
  {
  my ($input, $count) = @_;
  my $output = '';
  my $i = 0;
  do {
    my $value = ord(substr($input,$i++,1));
    $output .= substr($itoa64,$value & 0x3f,1);
    if ($i < $count) {
      $value |= ord(substr($input,$i,1)) << 8;
    }
    $output .= substr($itoa64,($value >> 6) & 0x3f,1);
    if ($i++ >= $count) {
      break;
    }
    if ($i < $count) {
      $value |= ord(substr($input,$i,1)) << 16;
    }
    $output .= substr($itoa64,($value >> 12) & 0x3f,1);
    if ($i++ >= $count) {
      break;
    }
    $output .= substr($itoa64,($value >> 18) & 0x3f,1);
  } while ($i < $count);

  return $output;
  }

my $loghistory_rec = 0;
sub log
  {
  my ($fh, $clienttype, $vid, $msg) = @_;

  AE::log info => "#$fh $clienttype $vid $msg";

  return if ($loghistory_tim<=0);

  $db->do('INSERT INTO ovms_historicalmessages (vehicleid,h_timestamp,h_recordtype,h_recordnumber,h_data,h_expires) '
          . 'VALUES (?,UTC_TIMESTAMP(),"*-OVM-ServerLogs",?,?,UTC_TIMESTAMP()+INTERVAL ? SECOND) ',
            undef,
            $vid,$loghistory_rec++,"#$fh $clienttype $msg",$loghistory_tim);
  $loghistory_rec=0 if ($loghistory_rec>65535);
  }

