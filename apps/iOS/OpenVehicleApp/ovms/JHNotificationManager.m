//
//  NotificationManager.m
//  Notification
//
//  Created by Jeff Hodnett on 13/09/2011.
//  Copyright 2011 Applausible. All rights reserved.
//

#import "JHNotificationManager.h"

#define kSecondsVisibleDelay 2.0f

@implementation JHNotificationManager

+(JHNotificationManager *)sharedManager
{
    static JHNotificationManager *instance = nil;
    if(instance == nil) {
        instance = [[JHNotificationManager alloc] init];
    }
    return instance;
}

-(id)init
{
    if( (self = [super init]) ) {
        
        // Setup the array
        notificationQueue = [[NSMutableArray alloc] init];
        
        // Set not showing by default
        showingNotification = NO;
    }
    return self;
}

-(void)dealloc
{
}

#pragma messages
+(void)notificationWithMessage:(NSString *)message
{
    // Show the notification
    [[JHNotificationManager sharedManager] addNotificationViewWithMessage:message];
}

-(void)addNotificationViewWithMessage:(NSString *)message
{
    // Grab the main window
    UIWindow *window = [[[UIApplication sharedApplication] delegate] window];
    
    // Grab the background image for calculations
    UIImage *bgImage = [UIImage imageNamed:@"notification_background"];
    
    // Create the notification view (here you could just call another UIVirew subclass)
    UIView *notificationView = [[UIView alloc] initWithFrame:CGRectMake(0, -bgImage.size.height, bgImage.size.width, bgImage.size.height)];
    [notificationView setBackgroundColor:[UIColor clearColor]];
    
    // Add an image background
    UIImageView *bgImageView = [[UIImageView alloc] initWithFrame:CGRectMake(0, 0, bgImage.size.width, bgImage.size.height)];
    [bgImageView setImage:bgImage];
    [notificationView addSubview:bgImageView];
    
    // Add some text label
    UILabel *label = [[UILabel alloc] initWithFrame:CGRectMake(10, 5, 300, notificationView.frame.size.height)];
    [label setText:message];
    [label setFont:[UIFont systemFontOfSize:14.0f]];
    [label setTextAlignment:UITextAlignmentLeft];
    [label setTextColor:[UIColor whiteColor]];
    [label setBackgroundColor:[UIColor clearColor]];
    label.lineBreakMode=UILineBreakModeWordWrap;
    label.numberOfLines=0;
    [notificationView addSubview:label];
    
    // Add to the window
    [window addSubview:notificationView];
    [notificationQueue addObject:notificationView];
    
    // Should we show this notification view
    if(!showingNotification) {
        [self showNotificationView:notificationView];
    }
}

-(void)showNotificationView:(UIView *)notificationView
{
    // Set showing the notification
    showingNotification = YES;
    
    // Animate the view downwards
    [UIView beginAnimations:@"" context:nil];
    
    // Setup a callback for the animation ended
    [UIView setAnimationDelegate:self];
    [UIView setAnimationDidStopSelector:@selector(showNotificationAnimationComplete:finished:context:)];
    
    [UIView setAnimationDuration:0.5f];
    
    [notificationView setFrame:CGRectMake(notificationView.frame.origin.x, notificationView.frame.origin.y+notificationView.frame.size.height, notificationView.frame.size.width, notificationView.frame.size.height)];
    
    [UIView commitAnimations];
}

-(void)showNotificationAnimationComplete:(NSString*)animationID finished:(NSNumber*)finished context:(void*)context
{
    // Hide the notification after a set second delay
    [self performSelector:@selector(hideCurrentNotification) withObject:nil afterDelay:kSecondsVisibleDelay];
}

-(void)hideCurrentNotification
{
    // Get the current view
    UIView *notificationView = [notificationQueue objectAtIndex:0];
    
    // Animate the view downwards
    [UIView beginAnimations:@"" context:nil];
    
    // Setup a callback for the animation ended
    [UIView setAnimationDelegate:self];
    [UIView setAnimationDidStopSelector:@selector(hideNotificationAnimationComplete:finished:context:)];
    
    [UIView setAnimationDuration:0.5f];
    
    [notificationView setFrame:CGRectMake(notificationView.frame.origin.x, notificationView.frame.origin.y-notificationView.frame.size.height, notificationView.frame.size.width, notificationView.frame.size.height)];
    
    [UIView commitAnimations];
}

-(void)hideNotificationAnimationComplete:(NSString*)animationID finished:(NSNumber*)finished context:(void*)context
{
    // Remove the old one
    UIView *notificationView = [notificationQueue objectAtIndex:0];
    [notificationView removeFromSuperview];
    [notificationQueue removeObject:notificationView];
    
    // Set not showing
    showingNotification = NO;
    
    // Do we have to add anymore items - if so show them
    if([notificationQueue count] > 0) {
        UIView *v = [notificationQueue objectAtIndex:0];
        [self showNotificationView:v];
    }
}

@end
