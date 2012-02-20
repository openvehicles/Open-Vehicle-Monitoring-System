//
//  ovmsControlParametersViewController.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsControlParametersViewController.h"
#import "JHNotificationManager.h"

@implementation ovmsControlParametersViewController
@synthesize m_table;

- (id)initWithStyle:(UITableViewStyle)style
{
    self = [super initWithStyle:style];
    if (self) {
        // Custom initialization
      params_ready = NO;
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

  params_ready = NO;
  m_table.userInteractionEnabled = NO;
  
  // Request the list of features from the car...
  [[ovmsAppDelegate myRef] commandRegister:@"3" callback:self];
  [self startSpinner:@"Loading Parameters"];
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

- (void)startSpinner:(NSString *)label
  {
  MBProgressHUD *hud = [MBProgressHUD showHUDAddedTo:self.navigationController.view animated:YES];
  hud.labelText = label; 
  m_table.userInteractionEnabled = NO;
  }

- (void)stopSpinner
  {
  [MBProgressHUD hideHUDForView:self.navigationController.view animated:YES];
  m_table.userInteractionEnabled = YES;
  }

- (void)commandResult:(NSArray*)result
  {  
  if ([result count]>1)
    {
    int command = [[result objectAtIndex:0] intValue];
    int rcode = [[result objectAtIndex:1] intValue];
    if (command == 4)
      {
      [[ovmsAppDelegate myRef] commandCancel];
      [self stopSpinner];
      switch (rcode)
        {
        case 0:
          break;
        case 1: // failed
          [JHNotificationManager
           notificationWithMessage:
           [NSString stringWithFormat:@"Failed: %@",[[result objectAtIndex:2] stringValue]]];
          break;
        case 2: // unsupported
          [JHNotificationManager notificationWithMessage:@"Unsupported operation"];
          break;
        case 3: // unimplemented
          [JHNotificationManager notificationWithMessage:@"Unimplemented operation"];
          break;
        }
      return;
      }
    if (command != 3) return; // Not for us
    switch (rcode)
      {
      case 0:
        {
        int fn = 0;
        int fm = 0;
        NSString* fv = @"";
        if ([result count]>4)
          {
          fn = [[result objectAtIndex:2] intValue];
          fm = [[result objectAtIndex:3] intValue];
          fv = [result objectAtIndex:4];
          if (fn < PARAM_FEATURE_S)
            {
            param[fn] = fv;
            }
          if (fn == fm-1)
            {
            [[ovmsAppDelegate myRef] commandCancel];
            params_ready = YES;
            [self stopSpinner];
            [m_table reloadData];
            }
          }
        }
        break;
      case 1: // failed
        [JHNotificationManager
         notificationWithMessage:
         [NSString stringWithFormat:@"Failed: %@",[[result objectAtIndex:2] stringValue]]];
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

- (void)textFieldDidEndEditing:(UITextField *)textField
  {
  UIView *cell = [[textField superview] superview];
  int fn = cell.tag-200;
  NSString* val = textField.text;
  
  if ([val compare:param[fn]])
    {
    param[fn] = val;
    [[ovmsAppDelegate myRef] commandRegister:[NSString stringWithFormat:@"4,%d,%@",fn,val] callback:self];
    [self startSpinner:@"Saving"];
    }
  }


#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
  {
  // Return the number of sections.
  return (params_ready)?1:0;
  }

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
  {
  // Return the number of rows in the section.
  return (params_ready)?PARAM_FEATURE_S:0;
  }

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
  static NSString *CellIdentifier = @"ParamCell";
  
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
  if (cell == nil) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier];
  }
  [cell setTag:200+indexPath.row];
  
  // Configure the cell...
  
  UILabel *cellLabel = (UILabel *)[cell viewWithTag:1000];
  switch (indexPath.row)
    {
    case PARAM_REGPHONE:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Registered Telephone",indexPath.row]];
      break;
    case PARAM_REGPASS:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Registration Password",indexPath.row]];
      break;
    case PARAM_MILESKM:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Miles/Km",indexPath.row]];
      break;
    case PARAM_NOTIFIES:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Notifications",indexPath.row]];
      break;
    case PARAM_SERVERIP:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Server IP",indexPath.row]];
      break;
    case PARAM_GPRSAPN:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: GPRS APN",indexPath.row]];
      break;
    case PARAM_GPRSUSER:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: GPRS User",indexPath.row]];
      break;
    case PARAM_GPRSPASS:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: GPRS Password",indexPath.row]];
      break;
    case PARAM_MYID:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Vehicle ID",indexPath.row]];
      break;
    case PARAM_NETPASS1:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Network Password",indexPath.row]];
      break;
    case PARAM_PARANOID:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Paranoid Mode",indexPath.row]];
      break;
    case PARAM_S_GROUP:
      [cellLabel setText:[NSString stringWithFormat:@"#%d: Social Group",indexPath.row]];
      break;
    default:
      [cellLabel setText:[NSString stringWithFormat:@"#%d:",indexPath.row]];
      break;
    }
  
  UITextField *cellText = (UITextField *)[cell viewWithTag:1001];
  if ((indexPath.row == PARAM_REGPASS)||(indexPath.row == PARAM_NETPASS1))
    {
    [cellText setText:@"**********"];
    cellText.enabled = NO;    
    }
  else
    {
    [cellText setText:param[indexPath.row]];
    cellText.enabled = YES;
    }
  
  return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
  {
  [self.view endEditing:TRUE];
  }


@end
