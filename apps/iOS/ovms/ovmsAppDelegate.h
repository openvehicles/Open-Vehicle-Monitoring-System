//
//  ovmsAppDelegate.h
//  ovms
//
//  Created by Mark Webb-Johnson on 16/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <CFNetwork/CFSocketStream.h>
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

@interface ovmsAppDelegate : UIResponder <UIApplicationDelegate>
{
  GCDAsyncSocket *asyncSocket;
  NSInteger networkingCount;
  unsigned char token[TOKEN_SIZE+1];
  RC4_CTX rxCrypto;
  RC4_CTX txCrypto;
  id location_delegate;
  
  CLLocationCoordinate2D car_location;
}

@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic) id location_delegate;
@property (assign) CLLocationCoordinate2D car_location;

+ (ovmsAppDelegate *) myRef;
- (void)didStartNetworking;
- (void)didStopNetworking;
- (void)serverConnect;
- (void)serverDisconnect;
- (void)handleCommand:(char)code command:(NSString*)cmd;

@end
