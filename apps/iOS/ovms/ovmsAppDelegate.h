//
//  ovmsAppDelegate.h
//  ovms
//
//  Created by Mark Webb-Johnson on 16/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <CFNetwork/CFSocketStream.h>
#import "time.h"
#import "CoreLocation/CoreLocation.h"
#import "crypto_base64.h"
#import "crypto_md5.h"
#import "crypto_hmac.h"
#import "crypto_rc4.h"

#define TOKEN_SIZE 22

@class GCDAsyncSocket;

@protocol ovmsLocationDelegate
@required
-(void) updateLocation;
@end

@protocol ovmsStatusDelegate
@required
-(void) updateStatus;
@end

@interface ovmsAppDelegate : UIResponder <UIApplicationDelegate>
{
  GCDAsyncSocket *asyncSocket;
  NSInteger networkingCount;
  unsigned char token[TOKEN_SIZE+1];
  RC4_CTX rxCrypto;
  RC4_CTX txCrypto;

  NSString* pmToken;
  RC4_CTX pmCrypto;
  MD5_CTX pmDigest;
  
  id location_delegate;
  id status_delegate;
  
  NSString* sel_car;
  NSString* sel_label;
  NSString* sel_netpass;
  NSString* sel_userpass;
  NSString* sel_imagepath;

  time_t car_lastupdated;
  int car_connected;
  
  CLLocationCoordinate2D car_location;
  int car_soc;
  NSString* car_units;
  int car_linevoltage;
  int car_chargecurrent;
  NSString* car_chargestate;
  NSString* car_chargemode;
  int car_idealrange;
  int car_estimatedrange;
}

@property (strong, nonatomic) UIWindow *window;

@property (strong, nonatomic) NSString* sel_car;
@property (strong, nonatomic) NSString* sel_label;
@property (strong, nonatomic) NSString* sel_netpass;
@property (strong, nonatomic) NSString* sel_userpass;
@property (strong, nonatomic) NSString* sel_imagepath;

@property (readonly, strong, nonatomic) NSManagedObjectContext *managedObjectContext;
@property (readonly, strong, nonatomic) NSManagedObjectModel *managedObjectModel;
@property (readonly, strong, nonatomic) NSPersistentStoreCoordinator *persistentStoreCoordinator;

@property (assign) time_t car_lastupdated;
@property (assign) int car_connected;

@property (strong, nonatomic) id location_delegate;
@property (assign) CLLocationCoordinate2D car_location;

@property (strong, nonatomic) id status_delegate;
@property (assign) int car_soc;
@property (strong, nonatomic) NSString* car_units;
@property (assign) int car_linevoltage;
@property (assign) int car_chargecurrent;
@property (strong, nonatomic) NSString* car_chargestate;
@property (strong, nonatomic) NSString* car_chargemode;
@property (assign) int car_idealrange;
@property (assign) int car_estimatedrange;

+ (ovmsAppDelegate *) myRef;
- (void)saveContext;
- (NSURL *)applicationDocumentsDirectory;
- (void)didStartNetworking;
- (void)didStopNetworking;
- (void)serverConnect;
- (void)serverDisconnect;
- (void)handleCommand:(char)code command:(NSString*)cmd;
- (void)switchCar:(NSString*)car;

@end
