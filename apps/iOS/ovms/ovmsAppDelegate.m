//
//  ovmsAppDelegate.m
//  ovms
//
//  Created by Mark Webb-Johnson on 16/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import "ovmsAppDelegate.h"
#import "GCDAsyncSocket.h"
#import "JHNotificationManager.h"
#import "Cars.h"

@implementation ovmsAppDelegate

@synthesize window = _window;

@synthesize sel_car;
@synthesize sel_label;
@synthesize sel_netpass;
@synthesize sel_userpass;
@synthesize sel_imagepath;

@synthesize managedObjectContext = __managedObjectContext;
@synthesize managedObjectModel = __managedObjectModel;
@synthesize persistentStoreCoordinator = __persistentStoreCoordinator;

@synthesize car_lastupdated;
@synthesize car_connected;
@synthesize car_paranoid;

@synthesize location_delegate;
@synthesize car_location;

@synthesize status_delegate;
@synthesize car_soc;
@synthesize car_units;
@synthesize car_linevoltage;
@synthesize car_chargecurrent;
@synthesize car_chargestate;
@synthesize car_chargemode;
@synthesize car_idealrange;
@synthesize car_estimatedrange;

@synthesize car_delegate;
@synthesize car_doors1;
@synthesize car_doors2;
@synthesize car_lockstate;
@synthesize car_vin;
@synthesize car_firmware;
@synthesize server_firmware;
@synthesize car_gsmlevel;
@synthesize car_tpem;
@synthesize car_tmotor;
@synthesize car_tbattery;
@synthesize car_trip;
@synthesize car_odometer;
@synthesize car_speed;
@synthesize car_tpms_fr_pressure;
@synthesize car_tpms_fr_temp;
@synthesize car_tpms_rr_pressure;
@synthesize car_tpms_rr_temp;
@synthesize car_tpms_fl_pressure;
@synthesize car_tpms_fl_temp;
@synthesize car_tpms_rl_pressure;
@synthesize car_tpms_rl_temp;

+ (ovmsAppDelegate *) myRef
{
  //return self;
  return (ovmsAppDelegate *)[UIApplication sharedApplication].delegate;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    // Override point for customization after application launch.
 
  // Set the application defaults
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  NSDictionary *appDefaults = [NSDictionary
                               dictionaryWithObjectsAndKeys:@"www.openvehicles.com", @"ovmsServer",
                                                            @"6867", @"ovmsPort",
                                                            @"DEMO", @"selCar",
                                                            @"Demonstration Car", @"selLabel",
                                                            @"DEMO", @"selNetPass",
                                                            @"DEMO", @"selUserPass",
                                                            @"car_models_signaturered.png", @"selImagePath",
                                                            @"", @"apnsDeviceid",
                                                            nil];
  [defaults registerDefaults:appDefaults];
  [defaults synchronize];

  apns_deviceid = [defaults stringForKey:@"apnsDeviceid"];
  apns_devicetoken = @"";
  #ifdef DEBUG
  apns_pushkeytype = @"sandbox";
  #else
  apns_pushkeytype = @"production";
  #endif
  
  if ([apns_deviceid length] == 0)
    {
    CFUUIDRef     myUUID;
    CFStringRef   myUUIDString;
  
    myUUID = CFUUIDCreate(kCFAllocatorDefault);
    myUUIDString = CFUUIDCreateString(kCFAllocatorDefault, myUUID);
  
    apns_deviceid = (__bridge_transfer NSString*)myUUIDString;
    }

  sel_car = [defaults stringForKey:@"selCar"];
  sel_label = [defaults stringForKey:@"selLabel"];
  sel_netpass = [defaults stringForKey:@"selNetPass"];
  sel_userpass = [defaults stringForKey:@"selUserPass"];
  sel_imagepath = [defaults stringForKey:@"selImagePath"];
  
  NSManagedObjectContext *context = [self managedObjectContext];
  NSFetchRequest *request = [[NSFetchRequest alloc] init];
  [request setEntity: [NSEntityDescription entityForName:@"Cars" inManagedObjectContext: context]];
  NSError *error = nil;
  NSUInteger count = [context countForFetchRequest: request error: &error];
  if (count == 0)
    {
    Cars *car = [NSEntityDescription
                  insertNewObjectForEntityForName:@"Cars" 
                  inManagedObjectContext:context];
    car.vehicleid = @"DEMO";
    car.label = @"Demonstration Car";
    car.netpass = @"DEMO";
    car.userpass = @"DEMO";
    car.imagepath = @"car_models_signaturered.png";
    if (![context save:&error])
      {
      NSLog(@"Whoops, couldn't save: %@", [error localizedDescription]);
      }
    }
  
  // Let the device know we want to receive push notifications
	[[UIApplication sharedApplication] registerForRemoteNotificationTypes:
   (UIRemoteNotificationTypeSound | UIRemoteNotificationTypeAlert)];
  
  return YES;
}
	
- (void)application:(UIApplication*)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData*)deviceToken
{
  NSString *tempString = [[deviceToken description] stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"<>"]];
  apns_devicetoken = [tempString stringByReplacingOccurrencesOfString:@" " withString:@""];
	NSLog(@"My token is: %@", apns_devicetoken);
}

- (void)application:(UIApplication*)application didFailToRegisterForRemoteNotificationsWithError:(NSError*)error
{
	NSLog(@"Failed to get token, error: %@", error);
}

- (void)application:(UIApplication *)application didReceiveLocalNotification:(UILocalNotification *)notification
{
  
}

- (void)application:(UIApplication *)application didReceiveRemoteNotification:(NSDictionary *)userInfo
{
  NSString *message = nil;
  NSDictionary* aps = [userInfo objectForKey:@"aps"];
  if (aps == nil) return;
  
  id alert = [aps objectForKey:@"alert"];
  if ([alert isKindOfClass:[NSString class]])
    {
    message = alert;
    }
  else if ([alert isKindOfClass:[NSDictionary class]])
    {
    message = [alert objectForKey:@"body"];
    }
  
  [JHNotificationManager notificationWithMessage:message];
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    /*
     Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
     Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
     */
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:apns_deviceid forKey:@"apnsDeviceid"];
  [defaults setObject:sel_car forKey:@"selCar"];
  [defaults setObject:sel_label forKey:@"selLabel"];
  [defaults setObject:sel_netpass forKey:@"selNetPass"];
  [defaults setObject:sel_userpass forKey:@"selUserPass"];
  [defaults setObject:sel_imagepath forKey:@"selImagePath"];
  [defaults synchronize];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    /*
     Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
     If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
     */
  [self serverDisconnect];
  [self saveContext];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    /*
     Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
     */
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    /*
     Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
     */
  [self serverConnect];
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    /*
     Called when the application is about to terminate.
     Save data if appropriate.
     See also applicationDidEnterBackground:.
    */
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:apns_deviceid forKey:@"apnsDeviceid"];
  [defaults setObject:sel_car forKey:@"selCar"];
  [defaults setObject:sel_label forKey:@"selLabel"];
  [defaults setObject:sel_netpass forKey:@"selNetPass"];
  [defaults setObject:sel_userpass forKey:@"selUserPass"];
  [defaults setObject:sel_imagepath forKey:@"selImagePath"];
  [defaults synchronize];

  [self serverDisconnect];
  [self saveContext];
}

/**
 Returns the URL to the application's Documents directory.
 */
- (NSURL *)applicationDocumentsDirectory
{
  return [[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] lastObject];
}

- (void)saveContext
{
  NSError *error = nil;
  NSManagedObjectContext *managedObjectContext = self.managedObjectContext;
  if (managedObjectContext != nil)
    {
    if ([managedObjectContext hasChanges] && ![managedObjectContext save:&error])
      {
      /*
       Replace this implementation with code to handle the error appropriately.
       
       abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development. 
       */
      NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
      abort();
      } 
    }
}

- (void)switchCar:(NSString*)car
{
  // We need to get the car record
  NSString* oldcar = sel_car;
  NSString* oldnewpass = sel_netpass;
  
  NSManagedObjectContext *context = [self managedObjectContext];
  NSFetchRequest *request = [[NSFetchRequest alloc] init];
  [request setEntity: [NSEntityDescription entityForName:@"Cars" inManagedObjectContext: context]];
  NSPredicate *predicate =
  [NSPredicate predicateWithFormat:@"vehicleid == %@", car];
  [request setPredicate:predicate];
  NSError *error = nil;
  NSArray *array = [context executeFetchRequest:request error:&error];
  if (array != nil)
    {
    if ([array count]>0)
      {
      Cars* car = [array objectAtIndex:0];
      sel_car = car.vehicleid;
      sel_label = car.label;
      sel_userpass = car.userpass;
      sel_netpass = car.netpass;
      sel_imagepath = car.imagepath;
      if ((![oldcar isEqualToString:sel_car])||(![oldnewpass isEqualToString:sel_netpass]))
        {
        [self serverDisconnect];
        [self serverConnect];
        }
      }
    }
}

- (void)didStartNetworking
{
  networkingCount = 1;
  [UIApplication sharedApplication].networkActivityIndicatorVisible = YES;
}

- (void)didStopNetworking
{
  networkingCount = 0;
  [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
}


- (void)serverConnect
{
  unsigned char digest[MD5_SIZE];
  unsigned char edigest[MD5_SIZE*2];
  
  if (asyncSocket == NULL)
    {
    self.car_lastupdated = 0;
    self.car_connected = 0;
    self.car_paranoid = FALSE;
    
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString* ovmsServer = [defaults stringForKey:@"ovmsServer"];
    int ovmsPort = [defaults integerForKey:@"ovmsPort"];
    const char *password = [sel_netpass UTF8String];
    const char *vehicleid = [sel_car UTF8String];
    
    dispatch_queue_t mainQueue = dispatch_get_main_queue();
    asyncSocket = [[GCDAsyncSocket alloc] initWithDelegate:self delegateQueue:mainQueue];
    NSError *error = nil;
    if (![asyncSocket connectToHost:ovmsServer onPort:ovmsPort error:&error])
      {
      // Croak on the error
      return;
      
      }
    [asyncSocket readDataToData:[GCDAsyncSocket CRLFData] withTimeout:-1 tag:0];
    
    // Make a (semi-)random client token
    sranddev();
    for (int k=0;k<TOKEN_SIZE;k++)
      {
      token[k] = cb64[rand()%64];
      }
    token[TOKEN_SIZE] = 0;
    
    hmac_md5(token, TOKEN_SIZE, (const uint8_t*)password, strlen(password), digest);
    base64encode(digest, MD5_SIZE, edigest);
    NSString *welcomeStr = [NSString stringWithFormat:@"MP-A 0 %s %s %s\r\n",token,edigest,vehicleid];
    NSData *welcomeData = [welcomeStr dataUsingEncoding:NSUTF8StringEncoding];
    [asyncSocket writeData:welcomeData withTimeout:-1 tag:0];    
    [self didStartNetworking];
    }
  }

- (void)serverDisconnect
{
  if (asyncSocket != NULL)
    {
    self.car_lastupdated = 0;
    self.car_connected = 0;
    [self serverClearState];
    [asyncSocket setDelegate:nil delegateQueue:NULL];
    [asyncSocket disconnect];
    [self didStopNetworking];
    asyncSocket = NULL;
    }
}

- (void)serverClearState
{
  //TODO
  car_location.latitude = 0;
  car_location.longitude = 0;
  car_soc = 0;
  car_units = @"-";
  car_linevoltage = 0;
  car_chargecurrent = 0;
  car_chargestate = @"";
  car_chargemode = @"";
  car_idealrange = 0;
  car_estimatedrange = 0;
  car_doors1 = 0;
  car_doors2 = 0;
  car_lockstate = 0;
  car_vin = @"";
  car_firmware = @"";
  server_firmware = @"";
  car_gsmlevel = 0;
  car_tpem = 0;
  car_tmotor = 0;
  car_tbattery = 0;
  car_trip = 0;
  car_odometer = 0;
  car_speed = 0;
  car_tpms_fr_pressure = 0;
  car_tpms_fr_temp = 0;
  car_tpms_rr_pressure = 0;
  car_tpms_rr_temp = 0;
  car_tpms_fl_pressure = 0;
  car_tpms_fl_temp = 0;
  car_tpms_rl_pressure = 0;
  car_tpms_rl_temp = 0;
  
  if ([self.location_delegate conformsToProtocol:@protocol(ovmsLocationDelegate)])
    {
    [self.location_delegate updateLocation];
    }
  if ([self.status_delegate conformsToProtocol:@protocol(ovmsStatusDelegate)])
    {
    [self.status_delegate updateStatus];
    }
  if ([self.car_delegate conformsToProtocol:@protocol(ovmsCarDelegate)])
    {
    [self.car_delegate updateCar];
    }
}

- (void)handleCommand:(char)code command:(NSString*)cmd
{
  if (code == 'E')
    {
    // We have a paranoid mode message
    if (cmd == nil) return;
    const char *pm = [cmd UTF8String];
    if (pm == nil) return;
    self.car_paranoid = TRUE;
    if (*pm == 'T')
      {
      // Set the paranoid token
      pmToken =  [[NSString alloc] initWithCString:pm+1 encoding:NSUTF8StringEncoding];
      const char *password = [sel_userpass UTF8String];
      hmac_md5((uint8_t*)pm+1, strlen(pm+1), (const uint8_t*)password, strlen(password), (uint8_t*)&pmDigest);
      return;
      }
    else if (*pm == 'M')
      {
      // Decrypt the paranoid message
      char buf[1024];
      int len;

      code = pm[1];
      len = base64decode((uint8_t*)pm+2, (uint8_t*)buf);
      RC4_setup(&pmCrypto, (uint8_t*)&pmDigest, MD5_SIZE);
      for (int k=0;k<1024;k++)
        {
        uint8_t x = 0;
        RC4_crypt(&pmCrypto, &x, &x, 1);
        }
      RC4_crypt(&pmCrypto, (uint8_t*)buf, (uint8_t*)buf, len);
      cmd = [[NSString alloc] initWithCString:buf encoding:NSUTF8StringEncoding];
      }
    }

  switch(code)
  {
    case 'Z': // Number of connected cars
      {
      self.car_connected = [cmd intValue];
      if ([self.status_delegate conformsToProtocol:@protocol(ovmsStatusDelegate)])
        {
        [self.status_delegate updateStatus];
        }
      }
      break;
    case 'P': // PUSH notification
      {
      NSString* message = [cmd substringFromIndex:1];
      [JHNotificationManager notificationWithMessage:message];
      }
      break;
    case 'S': // STATUS
      {
      NSArray *lparts = [cmd componentsSeparatedByString:@","];
      if ([lparts count]>=8)
        {
        car_soc = [[lparts objectAtIndex:0] intValue];
        car_units = [lparts objectAtIndex:1];
        car_linevoltage = [[lparts objectAtIndex:2] intValue];
        car_chargecurrent = [[lparts objectAtIndex:3] intValue];
        car_chargestate = [lparts objectAtIndex:4];
        car_chargemode = [lparts objectAtIndex:5];
        car_idealrange = [[lparts objectAtIndex:6] intValue];
        car_estimatedrange = [[lparts objectAtIndex:7] intValue];
        }
      if ([self.status_delegate conformsToProtocol:@protocol(ovmsStatusDelegate)])
        {
        [self.status_delegate updateStatus];
        }
      }
      break;
    case 'T': // TIME
      {
      self.car_lastupdated = time(0) - [cmd intValue];
      if ([self.status_delegate conformsToProtocol:@protocol(ovmsStatusDelegate)])
        {
        [self.status_delegate updateStatus];
        }
      }
      break;
    case 'L': // LOCATION
      {
      if ([self.status_delegate conformsToProtocol:@protocol(ovmsStatusDelegate)])
        {
        [self.status_delegate updateStatus];
        }
      NSArray *lparts = [cmd componentsSeparatedByString:@","];
      if ([lparts count]>=2)
        {
        car_location.latitude = [[lparts objectAtIndex:0] doubleValue];
        car_location.longitude = [[lparts objectAtIndex:1] doubleValue];
        // Update the visible location
        if ([self.location_delegate conformsToProtocol:@protocol(ovmsLocationDelegate)])
          {
          [self.location_delegate updateLocation];
          }
        }
      }
      break;
    case 'F': // CAR FIRMWARE
      {
      if ([self.status_delegate conformsToProtocol:@protocol(ovmsStatusDelegate)])
        {
        [self.status_delegate updateStatus];
        }
      NSArray *lparts = [cmd componentsSeparatedByString:@","];
      if ([lparts count]>=3)
        {
        car_firmware = [lparts objectAtIndex:0];
        car_vin = [lparts objectAtIndex:1];
        car_gsmlevel = [[lparts objectAtIndex:2] intValue];
        // Update the visible status
        if ([self.car_delegate conformsToProtocol:@protocol(ovmsCarDelegate)])
          {
          [self.car_delegate updateCar];
          }
        }
      }
      break;
    case 'f': // SERVER FIRMWARE
      {
      if ([self.status_delegate conformsToProtocol:@protocol(ovmsStatusDelegate)])
        {
        [self.status_delegate updateStatus];
        }
      NSArray *lparts = [cmd componentsSeparatedByString:@","];
      if ([lparts count]>=1)
        {
        server_firmware = [lparts objectAtIndex:0];
        // Update the visible status
        if ([self.car_delegate conformsToProtocol:@protocol(ovmsCarDelegate)])
          {
          [self.car_delegate updateCar];
          }
        }
      }
      break;
    case 'D': // CAR ENVIRONMENT
      {
      if ([self.status_delegate conformsToProtocol:@protocol(ovmsStatusDelegate)])
        {
        [self.status_delegate updateStatus];
        }
      NSArray *lparts = [cmd componentsSeparatedByString:@","];
      if ([lparts count]>=9)
        {
        car_doors1 = [[lparts objectAtIndex:0] intValue];
        car_doors2 = [[lparts objectAtIndex:1] intValue];
        car_lockstate = [[lparts objectAtIndex:2] intValue];
        car_tpem = [[lparts objectAtIndex:3] intValue];
        car_tmotor = [[lparts objectAtIndex:4] intValue];
        car_tbattery = [[lparts objectAtIndex:5] intValue];
        car_trip = [[lparts objectAtIndex:6] intValue];
        car_odometer = [[lparts objectAtIndex:7] intValue];
        car_speed = [[lparts objectAtIndex:8] intValue];
        // Update the visible status
        if ([self.car_delegate conformsToProtocol:@protocol(ovmsCarDelegate)])
          {
          [self.car_delegate updateCar];
          }
        }
      }
      break;
    case 'W': // CAR TPMS
      {
      if ([self.status_delegate conformsToProtocol:@protocol(ovmsStatusDelegate)])
        {
        [self.status_delegate updateStatus];
        }
      NSArray *lparts = [cmd componentsSeparatedByString:@","];
      if ([lparts count]>=8)
        {
        car_tpms_fr_pressure = [[lparts objectAtIndex:0] floatValue];
        car_tpms_fr_temp = [[lparts objectAtIndex:1] intValue];
        car_tpms_rr_pressure = [[lparts objectAtIndex:2] floatValue];
        car_tpms_rr_temp = [[lparts objectAtIndex:3] intValue];
        car_tpms_fl_pressure = [[lparts objectAtIndex:4] floatValue];
        car_tpms_fl_temp = [[lparts objectAtIndex:5] intValue];
        car_tpms_rl_pressure = [[lparts objectAtIndex:6] floatValue];
        car_tpms_rl_temp = [[lparts objectAtIndex:7] intValue];
        // Update the visible status
        if ([self.car_delegate conformsToProtocol:@protocol(ovmsCarDelegate)])
          {
          [self.car_delegate updateCar];
          }
        }
      }
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark Socket Delegate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)socket:(GCDAsyncSocket *)sock didConnectToHost:(NSString *)host port:(UInt16)port
{
  // Here we do the stuff after the connection to the host
}


- (void)socket:(GCDAsyncSocket *)sock didWriteDataWithTag:(long)tag
{
}

- (void)socket:(GCDAsyncSocket *)sock didReadData:(NSData *)data withTag:(long)tag
{
  const char *password = [sel_netpass UTF8String];
  unsigned char digest[MD5_SIZE];
  unsigned char edigest[(MD5_SIZE*2)+2];

  [self didStopNetworking];
  
  NSString *response = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
  if (tag==0)
    { // Welcome message
    NSArray *rparts = [response componentsSeparatedByString:@" "];
    NSString *stoken = [rparts objectAtIndex:2];
    NSString *etoken = [rparts objectAtIndex:3];
    const char *cstoken = [stoken UTF8String];
      if ((stoken==NULL)||(etoken==NULL))
      {
      [self serverDisconnect];
      return;
      }
    // Check for token-replay attack
    if (strcmp((char*)token,cstoken)==0)
      {
      [self serverDisconnect];
      return; // Server is using our token!
      }
      
    // Validate server token
    hmac_md5((const uint8_t*)cstoken, strlen(cstoken), (const uint8_t*)password, strlen(password), digest);
    base64encode(digest, MD5_SIZE, edigest);
    if (strncmp([etoken UTF8String],(const char*)edigest,strlen((const char*)edigest))!=0)
      {
      [self serverDisconnect];
      return; // Invalid server digest
      }
      
    // Ok, at this point, our token is ok
    int keylen = strlen((const char*)token)+strlen(cstoken)+1;
    char key[keylen];
    strcpy(key,cstoken);
    strcat(key,(const char*)token);
    hmac_md5((const uint8_t*)key, strlen(key), (const uint8_t*)password, strlen(password), digest);
      
    // Setup, and prime the rx and tx cryptos
    RC4_setup(&rxCrypto, digest, MD5_SIZE);
    for (int k=0;k<1024;k++)
      {
      uint8_t x = 0;
      RC4_crypt(&rxCrypto, &x, &x, 1);
      }
    RC4_setup(&txCrypto, digest, MD5_SIZE);
    for (int k=0;k<1024;k++)
      {
      uint8_t x = 0;
      RC4_crypt(&txCrypto, &x, &x, 1);
      }
    [asyncSocket readDataToData:[GCDAsyncSocket CRLFData] withTimeout:-1 tag:1];
      
      
    if ([apns_devicetoken length] > 0)
    {
      // Subscribe to push notifications
      char buf[1024];
      char output[1024];
      NSString* pns = [NSString stringWithFormat:@"MP-0 p%@,apns,%@,%@,%@,%@",
                       apns_deviceid, apns_pushkeytype, sel_car, sel_netpass, apns_devicetoken];
      strcpy(buf, [pns UTF8String]);
      int len = strlen(buf);
      RC4_crypt(&txCrypto, (uint8_t*)buf, (uint8_t*)buf, len);
      base64encode((uint8_t*)buf, len, (uint8_t*)output);
      NSString *pushStr = [NSString stringWithFormat:@"%s\r\n",output];
      NSData *pushData = [pushStr dataUsingEncoding:NSUTF8StringEncoding];
      [asyncSocket writeData:pushData withTimeout:-1 tag:0];    
      }
    }
  else if (tag==1)
    { // Normal encrypted data packet
    //char buf[[response length]+1];
    char buf[1024];
    int len = base64decode((uint8_t*)[response UTF8String], (uint8_t*)buf);
    RC4_crypt(&rxCrypto, (uint8_t*)buf, (uint8_t*)buf, len);
      if ((buf[0]=='M')&&(buf[1]=='P')&&(buf[2]=='-')&&(buf[3]=='0'))
        {
        NSString *cmd = [[NSString alloc] initWithCString:buf+6 encoding:NSUTF8StringEncoding];
        [self handleCommand:buf[5] command:cmd];
        }
    [asyncSocket readDataToData:[GCDAsyncSocket CRLFData] withTimeout:-1 tag:1];
    }
}

- (void)socketDidDisconnect:(GCDAsyncSocket *)sock withError:(NSError *)err
{
  [[ovmsAppDelegate myRef] serverClearState];
}

/**
 Returns the managed object context for the application.
 If the context doesn't already exist, it is created and bound to the persistent store coordinator for the application.
 */
- (NSManagedObjectContext *)managedObjectContext
{
  if (__managedObjectContext != nil)
    {
    return __managedObjectContext;
    }
  
  NSPersistentStoreCoordinator *coordinator = [self persistentStoreCoordinator];
  if (coordinator != nil)
    {
    __managedObjectContext = [[NSManagedObjectContext alloc] init];
    [__managedObjectContext setPersistentStoreCoordinator:coordinator];
    }
  return __managedObjectContext;
}

/**
 Returns the managed object model for the application.
 If the model doesn't already exist, it is created from the application's model.
 */
- (NSManagedObjectModel *)managedObjectModel
{
  if (__managedObjectModel != nil)
    {
    return __managedObjectModel;
    }
  NSURL *modelURL = [[NSBundle mainBundle] URLForResource:@"Model" withExtension:@"momd"];
  __managedObjectModel = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
  return __managedObjectModel;
}

/**
 Returns the persistent store coordinator for the application.
 If the coordinator doesn't already exist, it is created and the application's store added to it.
 */
- (NSPersistentStoreCoordinator *)persistentStoreCoordinator
{
  if (__persistentStoreCoordinator != nil)
    {
    return __persistentStoreCoordinator;
    }
  
  NSURL *storeURL = [[self applicationDocumentsDirectory] URLByAppendingPathComponent:@"ovmsmodel.sqlite"];
  
  NSError *error = nil;
  __persistentStoreCoordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:[self managedObjectModel]];
  if (![__persistentStoreCoordinator addPersistentStoreWithType:NSSQLiteStoreType configuration:nil URL:storeURL options:nil error:&error])
    {
    /*
     Replace this implementation with code to handle the error appropriately.
     
     abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development. 
     
     Typical reasons for an error here include:
     * The persistent store is not accessible;
     * The schema for the persistent store is incompatible with current managed object model.
     Check the error message to determine what the actual problem was.
     
     
     If the persistent store is not accessible, there is typically something wrong with the file path. Often, a file URL is pointing into the application's resources directory instead of a writeable directory.
     
     If you encounter schema incompatibility errors during development, you can reduce their frequency by:
     * Simply deleting the existing store:
     [[NSFileManager defaultManager] removeItemAtURL:storeURL error:nil]
     
     * Performing automatic lightweight migration by passing the following dictionary as the options parameter: 
     [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:YES], NSMigratePersistentStoresAutomaticallyOption, [NSNumber numberWithBool:YES], NSInferMappingModelAutomaticallyOption, nil];
     
     Lightweight migration will only work for a limited set of schema changes; consult "Core Data Model Versioning and Data Migration Programming Guide" for details.
     
     */
    NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
    abort();
    }    
  
  return __persistentStoreCoordinator;
}

@end
