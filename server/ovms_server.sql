-- MySQL dump 10.13  Distrib 5.1.66, for redhat-linux-gnu (x86_64)
--
-- Host: localhost    Database: openvehicles
-- ------------------------------------------------------
-- Server version	5.1.66

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
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `ovms_autoprovision` (
  `ap_key` varchar(64) NOT NULL DEFAULT '' COMMENT 'Unique Auto-Provisioning Key',
  `ap_stoken` varchar(32) NOT NULL DEFAULT '',
  `ap_sdigest` varchar(32) NOT NULL DEFAULT '',
  `ap_msg` varchar(4096) NOT NULL DEFAULT '',
  `deleted` tinyint(1) NOT NULL DEFAULT '0',
  `changed` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  PRIMARY KEY (`ap_key`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Auto-Provisioning Records';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `ovms_carmessages`
--

DROP TABLE IF EXISTS `ovms_carmessages`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `ovms_carmessages` (
  `vehicleid` varchar(32) NOT NULL DEFAULT '' COMMENT 'Unique vehicle ID',
  `m_code` char(1) NOT NULL DEFAULT '',
  `m_valid` tinyint(1) NOT NULL DEFAULT '1',
  `m_msgtime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `m_paranoid` tinyint(1) NOT NULL DEFAULT '0',
  `m_ptoken` varchar(32) NOT NULL DEFAULT '',
  `m_msg` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`vehicleid`,`m_code`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Stores vehicle messages';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `ovms_cars`
--

DROP TABLE IF EXISTS `ovms_cars`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `ovms_cars` (
  `vehicleid` varchar(32) NOT NULL DEFAULT '' COMMENT 'Unique vehicle ID',
  `owner` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'Owner user ID.',
  `carpass` varchar(255) NOT NULL DEFAULT '' COMMENT 'Car password',
  `userpass` varchar(255) NOT NULL DEFAULT '' COMMENT 'User password (optional)',
  `cryptscheme` varchar(1) NOT NULL DEFAULT '0',
  `v_ptoken` varchar(32) NOT NULL DEFAULT '',
  `v_server` varchar(32) NOT NULL DEFAULT '*',
  `v_type` varchar(10) NOT NULL DEFAULT 'CAR',
  `deleted` tinyint(1) NOT NULL DEFAULT '0',
  `changed` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `v_lastupdate` datetime NOT NULL DEFAULT '0000-00-00 00:00:00' COMMENT 'Car last update received',
  PRIMARY KEY (`vehicleid`),
  KEY `owner` (`owner`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Stores vehicle current data';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `ovms_historicalmessages`
--

DROP TABLE IF EXISTS `ovms_historicalmessages`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `ovms_historicalmessages` (
  `vehicleid` varchar(32) NOT NULL DEFAULT '' COMMENT 'Unique vehicle ID',
  `h_timestamp` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `h_recordtype` varchar(32) NOT NULL DEFAULT '',
  `h_recordnumber` int(5) NOT NULL DEFAULT '0',
  `h_data` text NOT NULL,
  `h_expires` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  PRIMARY KEY (`vehicleid`,`h_recordtype`,`h_recordnumber`,`h_timestamp`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Stores historical data records';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `ovms_notifies`
--

DROP TABLE IF EXISTS `ovms_notifies`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `ovms_notifies` (
  `vehicleid` varchar(32) NOT NULL DEFAULT '' COMMENT 'Unique vehicle ID',
  `appid` varchar(128) NOT NULL DEFAULT '' COMMENT 'Unique App ID',
  `pushtype` varchar(16) NOT NULL DEFAULT '',
  `pushkeytype` varchar(16) NOT NULL DEFAULT '',
  `pushkeyvalue` varchar(256) NOT NULL DEFAULT '',
  `lastupdated` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `active` tinyint(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`vehicleid`,`appid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Stores app notification configs';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `ovms_utilisation`
--

DROP TABLE IF EXISTS `ovms_utilisation`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `ovms_utilisation` (
  `vehicleid` varchar(32) NOT NULL DEFAULT '' COMMENT 'Unique vehicle ID',
  `u_date` date NOT NULL DEFAULT '0000-00-00',
  `u_c_rx` int(10) NOT NULL DEFAULT '0',
  `u_c_tx` int(10) NOT NULL DEFAULT '0',
  `u_a_rx` int(10) NOT NULL DEFAULT '0',
  `u_a_tx` int(10) NOT NULL DEFAULT '0',
  PRIMARY KEY (`vehicleid`,`u_date`),
  KEY `vehicle` (`vehicleid`,`u_date`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Stores network utilisation records';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2012-12-17  0:33:25
