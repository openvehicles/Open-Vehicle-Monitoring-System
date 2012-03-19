//
//  ovmsControlCellularUsageViewController.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsControlCellularUsageViewController.h"
#import "JHNotificationManager.h"

@implementation ovmsControlCellularUsageViewController
@synthesize m_webview;

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (void)didReceiveMemoryWarning
{
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc that aren't in use.
}

#pragma mark - View lifecycle

/*
// Implement loadView to create a view hierarchy programmatically, without using a nib.
- (void)loadView
{
}
*/


// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad
{
    [super viewDidLoad];

}

- (void)viewDidUnload
{
    [self setM_webview:nil];
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
  {
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
    return YES;
  else
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
  }

- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation duration:(NSTimeInterval)duration
  {
  m_webview.hidden = YES;
  }

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
  {
  [self displayChart];
  }

- (void)viewWillAppear:(BOOL)animated
  {
  [super viewWillAppear:animated];

  // Request the list of features from the car...
  t_rxt = 0;
  t_txt = 0;
  t_days = 0;
  [[ovmsAppDelegate myRef] commandRegister:@"30" callback:self];
  [self startSpinner:@"Loading Usage"];
  m_webview.hidden = YES;
  }

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
}

- (void)viewDidDisappear:(BOOL)animated
{
  [super viewDidDisappear:animated];
}

- (void)startSpinner:(NSString *)label
{
  MBProgressHUD *hud = [MBProgressHUD showHUDAddedTo:self.navigationController.view animated:YES];
  hud.labelText = label; 
}

- (void)stopSpinner
{
  [MBProgressHUD hideHUDForView:self.navigationController.view animated:YES];
}

- (void)displayChart
  {
  NSString *filePath = [[NSBundle mainBundle] pathForResource:@"ovmsControlCellularUsageView" ofType:@"html"];
  NSString *page = [NSString stringWithContentsOfFile:filePath encoding:NSUTF8StringEncoding error:NULL];
  
  // Now, let's hack the page to put in the data...
  NSString *r01 = [NSString stringWithFormat:@"Past %d Days", t_days];
  page = [page stringByReplacingOccurrencesOfString:@"+++REPLACEME01+++" withString:r01];
  
  NSString *r02 = [NSString stringWithFormat:@"%d Day Tx:%0.2fMB Rx:%0.2fMB",
                   t_days,
                   (float)t_txt/(1024*1024),
                   (float)t_rxt/(1024*1024)];
  page = [page stringByReplacingOccurrencesOfString:@"+++REPLACEME02+++" withString:r02];
  
  NSString *r03 = NULL;
  NSString *r04 = NULL;
  NSString *r05 = NULL;
  for (int k=0;k<t_days;k++)
    {
    //'T-6', 'T-5', 'T-4', 'T-3', 'T-2', 'T-1'
    if (r03 == NULL)
      r03 = [NSString stringWithFormat:@"'%@'",t_day[k]];
    else
      r03 = [NSString stringWithFormat:@"%@, '%@'",r03,t_day[k]];
    //1.336060, 1.538156, 1.576579, 1.600652, 1.968113, 1.901067
    if (r04 == NULL)
      r04 = [NSString stringWithFormat:@"%0.1f",(float)t_tx[k]/1024];
    else
      r04 = [NSString stringWithFormat:@"%@, %0.1f",r04, (float)t_tx[k]/1024];   
    //15.727003, 17.356071, 16.716049, 18.542843, 19.564053, 19.830493
    if (r05 == NULL)
      r05 = [NSString stringWithFormat:@"%0.1f",(float)t_rx[k]/1024];
    else
      r05 = [NSString stringWithFormat:@"%@, %0.1f",r05, (float)t_rx[k]/1024];   
    }
  page = [page stringByReplacingOccurrencesOfString:@"+++REPLACEME03+++" withString:r03];
  page = [page stringByReplacingOccurrencesOfString:@"+++REPLACEME04+++" withString:r04];
  page = [page stringByReplacingOccurrencesOfString:@"+++REPLACEME05+++" withString:r05];

  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
    {
    // The device is an iPad running iPhone 3.2 or later.
    UIInterfaceOrientation orientation = self.interfaceOrientation;
    if ((orientation == UIInterfaceOrientationPortrait)||
        (orientation == UIInterfaceOrientationPortraitUpsideDown))
      {
      page = [page stringByReplacingOccurrencesOfString:@"+++REPLACEME10+++" withString:@"768"];
      page = [page stringByReplacingOccurrencesOfString:@"+++REPLACEME11+++" withString:@"960"];
      }
    else
      {
      page = [page stringByReplacingOccurrencesOfString:@"+++REPLACEME10+++" withString:@"960"];
      page = [page stringByReplacingOccurrencesOfString:@"+++REPLACEME11+++" withString:@"768"];
      }
    }
  else
    {
    // The device is an iPhone or iPod touch.
    page = [page stringByReplacingOccurrencesOfString:@"+++REPLACEME10+++" withString:@"320"];
    page = [page stringByReplacingOccurrencesOfString:@"+++REPLACEME11+++" withString:@"416"];
    }
  
  [m_webview loadHTMLString:page baseURL:nil];
  }

- (void)commandResult:(NSArray*)result
{  
  if ([result count]>1)
    {
    int command = [[result objectAtIndex:0] intValue];
    int rcode = [[result objectAtIndex:1] intValue];
    if (command != 30) return; // Not for us
    switch (rcode)
      {
      case 0:
        {
        if ([result count]>5)
          {
          int fn = [[result objectAtIndex:2] intValue];
          int fm = [[result objectAtIndex:3] intValue];
          NSString *day = [result objectAtIndex:4];
          int rx = [[result objectAtIndex:5] intValue];
          int tx = [[result objectAtIndex:6] intValue];
          if (fn < MAX_DAYS)
            {
            t_day[fm-fn] = day;
            t_rx[fm-fn] = rx;
            t_tx[fm-fn] = tx;
            t_rxt += rx;
            t_txt += tx;
            t_days = fm;
            }
          if (fn == fm)
            {
            [[ovmsAppDelegate myRef] commandCancel];
            [self stopSpinner];
            [self displayChart];
            }
          }
        }
        break;
      case 1: // failed
        [JHNotificationManager
         notificationWithMessage:
         [NSString stringWithFormat:@"Failed: %@",[result objectAtIndex:2]]];
        [[ovmsAppDelegate myRef] commandCancel];
        [self stopSpinner];
        break;
      case 2: // unsupported
        [JHNotificationManager notificationWithMessage:@"Unsupported operation"];
        [[ovmsAppDelegate myRef] commandCancel];
        [self stopSpinner];
        break;
      case 3: // unimplemented
        [JHNotificationManager notificationWithMessage:@"Unimplemented operation"];
        [[ovmsAppDelegate myRef] commandCancel];
        [self stopSpinner];
        break;
      }
    }
  else
    {
    [[ovmsAppDelegate myRef] commandCancel];
    [self stopSpinner];
    }
}

- (BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType
{
  return YES;
}

- (void)webViewDidFinishLoad:(UIWebView *)webView
  {
  m_webview.hidden = NO;
  }

@end
