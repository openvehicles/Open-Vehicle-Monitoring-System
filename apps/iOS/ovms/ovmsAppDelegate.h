//
//  ovmsAppDelegate.h
//  ovms
//
//  Created by Mark Webb-Johnson on 16/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <CFNetwork/CFSocketStream.h>
#import "crypto_base64.h"
#import "crypto_md5.h"
#import "crypto_hmac.h"
#import "crypto_rc4.h"

#define TOKEN_SIZE 22

@class GCDAsyncSocket;

@interface ovmsAppDelegate : UIResponder <UIApplicationDelegate>
{
  GCDAsyncSocket *asyncSocket;
  NSInteger networkingCount;
  unsigned char token[TOKEN_SIZE+1];
  RC4_CTX rxCrypto;
  RC4_CTX txCrypto;
}

@property (strong, nonatomic) UIWindow *window;

- (void)didStartNetworking;
- (void)didStopNetworking;
- (void)serverConnect;
- (void)serverDisconnect;
- (void)handleCommand:(NSString*)cmd;

@end
