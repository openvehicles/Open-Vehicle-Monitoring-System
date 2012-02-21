//
//  ovmsControlCellularUsageViewController.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "MBProgressHUD.h"
#import "ovmsAppDelegate.h"

#define MAX_DAYS 100

@interface ovmsControlCellularUsageViewController : UIViewController <UIWebViewDelegate, ovmsCommandDelegate, MBProgressHUDDelegate>
  {
  NSString *t_day[MAX_DAYS];
  int t_rx[MAX_DAYS];
  int t_tx[MAX_DAYS];
  int t_days;
  int t_rxt;
  int t_txt;
  }

@property (strong, nonatomic) IBOutlet UIWebView *m_webview;

- (void)startSpinner:(NSString *)label;
- (void)stopSpinner;
- (void)displayChart;

@end
