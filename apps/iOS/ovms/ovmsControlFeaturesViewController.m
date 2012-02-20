//
//  ovmsControlFeaturesViewController.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsControlFeaturesViewController.h"
#import "JHNotificationManager.h"

@implementation ovmsControlFeaturesViewController
@synthesize m_table;
@synthesize spinner;

- (id)initWithStyle:(UITableViewStyle)style
{
    self = [super initWithStyle:style];
    if (self) {
        // Custom initialization
      features_ready = NO;
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

- (void)viewDidLoad
{
    [super viewDidLoad];

    // Uncomment the following line to preserve selection between presentations.
    // self.clearsSelectionOnViewWillAppear = NO;
 
    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    // self.navigationItem.rightBarButtonItem = self.editButtonItem;
}

- (void)viewDidUnload
{
  [self setM_table:nil];
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated
  {
  [super viewWillAppear:animated];
  features_ready = NO;
  m_table.userInteractionEnabled = NO;
  
  // Request the list of features from the car...
  [[ovmsAppDelegate myRef] commandRegister:@"1" callback:self];
  [self startSpinner];
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

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

- (void)startSpinner
  {
  spinner = nil;
  if (spinner==nil)
    {
    spinner = [[UIActivityIndicatorView alloc]initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhiteLarge];
    spinner.frame = CGRectMake(0, 0, 50, 50);
    spinner.center = self.view.center;
    [spinner setBackgroundColor:[UIColor blackColor]];
    [self.view addSubview:spinner];
    }
  m_table.userInteractionEnabled = NO;
  [spinner startAnimating];
  spinner.hidden = NO;
  }

- (void)stopSpinner
  {
  [spinner stopAnimating];
  m_table.userInteractionEnabled = YES;
  }

- (void)commandResult:(NSArray*)result
  {  
  if ([result count]>1)
    {
    int command = [[result objectAtIndex:0] intValue];
    int rcode = [[result objectAtIndex:1] intValue];
    if (command == 2)
      {
      switch (rcode)
        {
        case 0:
          [self stopSpinner];
          break;
        case 1: // failed
          [JHNotificationManager
           notificationWithMessage:
           [NSString stringWithFormat:@"Failed: %@",[[result objectAtIndex:2] stringValue]]];
          [[ovmsAppDelegate myRef] commandCancel];
          break;
        case 2: // unsupported
          [JHNotificationManager notificationWithMessage:@"Unsupported operation"];
          [[ovmsAppDelegate myRef] commandCancel];
          break;
        case 3: // unimplemented
          [JHNotificationManager notificationWithMessage:@"Unimplemented operation"];
          [[ovmsAppDelegate myRef] commandCancel];
          break;
        }
      return;
      }
    if (command != 1) return; // Not for us
    switch (rcode)
      {
      case 0:
        {
        int fn = 0;
        int fm = 0;
        int fv = 0;
        if ([result count]>4)
          {
          fn = [[result objectAtIndex:2] intValue];
          fm = [[result objectAtIndex:3] intValue];
          fv = [[result objectAtIndex:4] intValue];
          if (fn < FEATURES_MAX)
            {
            feature[fn] = fv;
            }
          if (fn == fm-1)
            {
            [[ovmsAppDelegate myRef] commandCancel];
            features_ready = YES;
            m_table.userInteractionEnabled = YES;
            [m_table reloadData];
            [spinner stopAnimating];
            }
          }
        }
        break;
      case 1: // failed
        [JHNotificationManager
         notificationWithMessage:
         [NSString stringWithFormat:@"Failed: %@",[[result objectAtIndex:2] stringValue]]];
        [[ovmsAppDelegate myRef] commandCancel];
        break;
      case 2: // unsupported
        [JHNotificationManager notificationWithMessage:@"Unsupported operation"];
        [[ovmsAppDelegate myRef] commandCancel];
        break;
      case 3: // unimplemented
        [JHNotificationManager notificationWithMessage:@"Unimplemented operation"];
        [[ovmsAppDelegate myRef] commandCancel];
        break;
      }
    }
  else
    {
    [[ovmsAppDelegate myRef] commandCancel];
    }
  }

- (void)textFieldDidEndEditing:(UITextField *)textField
  {
  UIView *cell = [[textField superview] superview];
  int fn = cell.tag-200;
  int val = [textField.text intValue];

  textField.text = [NSString stringWithFormat:@"%d",val];
  if (val != feature[fn])
    {
    feature[fn] = val;
    [[ovmsAppDelegate myRef] commandRegister:[NSString stringWithFormat:@"2,%d,%d",fn,val] callback:self];
    [self startSpinner];
    }
  }

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
  {
  // Return the number of sections.
  return (features_ready)?1:0;
  }

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
  {
  // Return the number of rows in the section.
  return (features_ready)?FEATURES_MAX:0;
  }

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
  {
  static NSString *CellIdentifier = @"FeatureCell";
  
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
  if (cell == nil) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier];
  }
  [cell setTag:200+indexPath.row];
  
  // Configure the cell...
  
  UILabel *cellLabel = (UILabel *)[cell viewWithTag:1000];
  switch (indexPath.row)
    {
    case FEATURE_SPEEDO:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Digital Speedo",indexPath.row]];
      break;
    case FEATURE_STREAM:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: GPS Stream",indexPath.row]];
      break;
    case FEATURE_MINSOC:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Minimum SOC",indexPath.row]];
      break;
    case FEATURE_CARBITS:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Car Bits",indexPath.row]];
      break;
    case FEATURE_CANWRITE:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: CAN Write",indexPath.row]];
      break;
    default:
      [cellLabel setText:[NSString stringWithFormat:@"#%d:",indexPath.row]];
      break;
    }

  UITextField *cellText = (UITextField *)[cell viewWithTag:1001];
  [cellText setText:[NSString stringWithFormat:@"%d", feature[indexPath.row]]];

  return cell;
  }

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
  {
  [self.view endEditing:TRUE];
  }

@end
