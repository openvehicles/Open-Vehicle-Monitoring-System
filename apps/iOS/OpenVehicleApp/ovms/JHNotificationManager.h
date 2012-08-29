//
//  JHNotificationManager.h
//  Notification
//
//  Created by Jeff Hodnett on 13/09/2011.
//  Copyright 2011 Applausible. All rights reserved.
//

#import <Foundation/Foundation.h>


@interface JHNotificationManager : NSObject {
 
@private
    // The notificatin views array
    NSMutableArray *notificationQueue;
    
    // Are we showing a notification
    BOOL showingNotification;
}

+(JHNotificationManager *)sharedManager;

+(void)notificationWithMessage:(NSString *)message;

-(void)addNotificationViewWithMessage:(NSString *)message;
-(void)showNotificationView:(UIView *)notificationView;

@end
