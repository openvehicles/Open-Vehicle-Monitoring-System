-- MySQL dump 10.11
--
-- Host: localhost    Database: openvehicles
-- ------------------------------------------------------
-- Server version	5.0.51a

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `ovms_autoprovision`
--

DROP TABLE IF EXISTS `ovms_autoprovision`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `ovms_autoprovision` (
  `ap_key` varchar(64) NOT NULL default '' COMMENT 'Unique Auto-Provisioning Key',
  `ap_stoken` varchar(32) NOT NULL default '',
  `ap_sdigest` varchar(32) NOT NULL default '',
  `ap_msg` varchar(4096) NOT NULL default '',
  `deleted` tinyint(1) NOT NULL default '0',
  `changed` datetime NOT NULL default '0000-00-00 00:00:00',
  `owner` int(10) unsigned NOT NULL default '0',
  `v_lastused` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`ap_key`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Auto-Provisioning Records';
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `ovms_carmessages`
--

DROP TABLE IF EXISTS `ovms_carmessages`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `ovms_carmessages` (
  `vehicleid` varchar(32) NOT NULL default '' COMMENT 'Unique vehicle ID',
  `m_code` char(1) NOT NULL default '',
  `m_valid` tinyint(1) NOT NULL default '1',
  `m_msgtime` datetime NOT NULL default '0000-00-00 00:00:00',
  `m_paranoid` tinyint(1) NOT NULL default '0',
  `m_ptoken` varchar(32) NOT NULL default '',
  `m_msg` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`vehicleid`,`m_code`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Stores vehicle messages';
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `ovms_cars`
--

DROP TABLE IF EXISTS `ovms_cars`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `ovms_cars` (
  `vehicleid` varchar(32) NOT NULL default '' COMMENT 'Unique vehicle ID',
  `vehiclename` varchar(64) NOT NULL default '',
  `owner` int(10) unsigned NOT NULL default '0' COMMENT 'Owner user ID.',
  `telephone` varchar(48) NOT NULL default '',
  `carpass` varchar(255) NOT NULL default '' COMMENT 'Car password',
  `userpass` varchar(255) NOT NULL default '' COMMENT 'User password (optional)',
  `cryptscheme` varchar(1) NOT NULL default '0',
  `v_ptoken` varchar(32) NOT NULL default '',
  `v_server` varchar(32) NOT NULL default '*',
  `v_type` varchar(10) NOT NULL default 'CAR',
  `deleted` tinyint(1) NOT NULL default '0',
  `changed` datetime NOT NULL default '0000-00-00 00:00:00',
  `v_lastupdate` datetime NOT NULL default '0000-00-00 00:00:00' COMMENT 'Car last update received',
  PRIMARY KEY  (`vehicleid`),
  KEY `owner` (`owner`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Stores vehicle current data';
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `ovms_historicalmessages`
--

DROP TABLE IF EXISTS `ovms_historicalmessages`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `ovms_historicalmessages` (
  `vehicleid` varchar(32) NOT NULL default '' COMMENT 'Unique vehicle ID',
  `h_timestamp` datetime NOT NULL default '0000-00-00 00:00:00',
  `h_recordtype` varchar(32) NOT NULL default '',
  `h_recordnumber` int(5) NOT NULL default '0',
  `h_data` text NOT NULL,
  `h_expires` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`vehicleid`,`h_recordtype`,`h_recordnumber`,`h_timestamp`),
  KEY `h_expires` (`h_expires`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Stores historical data records';
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `ovms_notifies`
--

DROP TABLE IF EXISTS `ovms_notifies`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `ovms_notifies` (
  `vehicleid` varchar(32) NOT NULL default '' COMMENT 'Unique vehicle ID',
  `appid` varchar(128) NOT NULL default '' COMMENT 'Unique App ID',
  `pushtype` varchar(16) NOT NULL default '',
  `pushkeytype` varchar(16) NOT NULL default '',
  `pushkeyvalue` varchar(256) NOT NULL default '',
  `lastupdated` datetime NOT NULL default '0000-00-00 00:00:00',
  `active` tinyint(1) NOT NULL default '1',
  PRIMARY KEY  (`vehicleid`,`appid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Stores app notification configs';
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `ovms_owners`
--

DROP TABLE IF EXISTS `ovms_owners`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `ovms_owners` (
  `owner` int(10) unsigned NOT NULL,
  `name` varchar(60) NOT NULL default '',
  `mail` varchar(254) NOT NULL default '',
  `pass` varchar(128) NOT NULL default '',
  `status` tinyint(4) NOT NULL default '0',
  `deleted` tinyint(1) NOT NULL default '0',
  `changed` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`owner`),
  KEY `name` (`name`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Stores vehicle owners';
SET character_set_client = @saved_cs_client;

-- Dump completed on 2013-02-18  1:17:19
