//
//  ovmsAppDelegate.m
//  ovms
//
//  Created by Mark Webb-Johnson on 16/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import "ovmsAppDelegate.h"
#import "GCDAsyncSocket.h"

@implementation ovmsAppDelegate

@synthesize window = _window;

@synthesize location_delegate;
@synthesize status_delegate;

@synthesize car_location;
@synthesize car_soc;
@synthesize car_units;
@synthesize car_linevoltage;
@synthesize car_chargecurrent;
@synthesize car_chargestate;
@synthesize car_chargemode;
@synthesize car_idealrange;
@synthesize car_estimatedrange;


+ (ovmsAppDelegate *) myRef
{
  //return self;
  return (ovmsAppDelegate *)[UIApplication sharedApplication].delegate;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    // Override point for customization after application launch.
    return YES;
}
							
- (void)applicationWillResignActive:(UIApplication *)application
{
    /*
     Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
     Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
     */
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    /*
     Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
     If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
     */
  [self serverDisconnect];
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
  [self serverDisconnect];
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
  char password[] = "NETPASS";
  
  if (asyncSocket == NULL)
    {
    dispatch_queue_t mainQueue = dispatch_get_main_queue();
    asyncSocket = [[GCDAsyncSocket alloc] initWithDelegate:self delegateQueue:mainQueue];
    NSError *error = nil;
    if (![asyncSocket connectToHost:@"www.openvehicles.com" onPort:6867 error:&error])
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
    NSString *welcomeStr = [NSString stringWithFormat:@"MP-A 0 %s %s %s\r\n",token,edigest,"TESTCAR"];
    NSData *welcomeData = [welcomeStr dataUsingEncoding:NSUTF8StringEncoding];
    [asyncSocket writeData:welcomeData withTimeout:-1 tag:0];    
    [self didStartNetworking];
    }
}

- (void)serverDisconnect
{
  if (asyncSocket != NULL)
    {
    [asyncSocket setDelegate:nil delegateQueue:NULL];
    [asyncSocket disconnect];
    [self didStopNetworking];
    asyncSocket = NULL;
    }
}

- (void)handleCommand:(char)code command:(NSString*)cmd
{
  switch(code)
  {
    case 'Z': // PING
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
    case 'L': // LOCATION
      {
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
  char password[] = "NETPASS";
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
}

@end
