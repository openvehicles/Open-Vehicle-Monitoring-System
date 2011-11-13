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
-- Table structure for table `ovms_cars`
--

DROP TABLE IF EXISTS `ovms_cars`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `ovms_cars` (
  `vehicleid` varchar(32) NOT NULL default '' COMMENT 'Unique vehicle ID',
  `owner` int(10) unsigned NOT NULL default '0' COMMENT 'Owner user ID.',
  `carpass` varchar(255) NOT NULL default '' COMMENT 'Car password',
  `userpass` varchar(255) NOT NULL default '' COMMENT 'User password (optional)',
  `cryptscheme` varchar(1) NOT NULL default '0',
  `v_latitude` double(10,6) NOT NULL default '0.000000',
  `v_longitude` double(10,6) NOT NULL default '0.000000',
  `v_soc` int(3) NOT NULL default '0' COMMENT 'Car SOC %',
  `v_units` char(1) NOT NULL default 'K' COMMENT 'Car units M/K',
  `v_linevoltage` int(3) NOT NULL default '0' COMMENT 'Car line voltage',
  `v_chargecurrent` int(3) NOT NULL default '0' COMMENT 'Car charge current',
  `v_chargestate` varchar(20) NOT NULL default '' COMMENT 'Car charge state',
  `v_chargemode` varchar(20) NOT NULL default '' COMMENT 'Car charge mode',
  `v_idealrange` int(3) NOT NULL default '0' COMMENT 'Car ideal range',
  `v_estimatedrange` int(3) NOT NULL default '0' COMMENT 'Car estimated range',
  `v_lastupdate` datetime NOT NULL default '0000-00-00 00:00:00' COMMENT 'Car last update received',
  PRIMARY KEY  (`vehicleid`),
  KEY `owner` (`owner`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='OVMS: Stores vehicle current data';
SET character_set_client = @saved_cs_client;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2011-11-13 12:53:26
