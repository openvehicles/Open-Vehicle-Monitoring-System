//
//  ovmsGroupsTableViewController.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 16/4/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsGroupsTableViewController.h"

@interface ovmsGroupsTableViewController ()

@end

@implementation ovmsGroupsTableViewController

@synthesize locationGroups;
@synthesize editing_insertrow;

- (id)initWithStyle:(UITableViewStyle)style
  {
  self = [super initWithStyle:style];
  if (self)
    {
    // Custom initialization
    }
  return self;
  }

- (void)viewDidLoad
  {
  [super viewDidLoad];

  // Load the current values...
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  NSString *defaultsLG = [defaults stringForKey:@"locationGroups"];
  locationGroups = [NSMutableArray array];
  if ([defaultsLG length]>0)
    {
    NSEnumerator *enumerator = [[defaultsLG componentsSeparatedByString:@" "] objectEnumerator];
    id target;    
    while ((target = [enumerator nextObject]))
      {
      [locationGroups addObject:target];
      }
    }

  [TestFlight passCheckpoint:@"GROUPS_VISITED"];
  
  // Uncomment the following line to preserve selection between presentations.
  // self.clearsSelectionOnViewWillAppear = NO;
 
  self.navigationItem.rightBarButtonItem = self.editButtonItem;
  }

- (void)viewDidUnload
  {
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

- (void)setEditing:(BOOL)editing animated:(BOOL)animated
  {
  [super setEditing:editing animated:animated];

  if (editing) editing_insertrow = [locationGroups count];

  NSIndexPath *path = [NSIndexPath indexPathForRow:editing_insertrow inSection:0];
  NSArray *paths = [NSArray arrayWithObject:path];

  if (editing)
    {
    // Add an insert row
    [[self tableView] insertRowsAtIndexPaths:paths 
                            withRowAnimation:UITableViewRowAnimationTop];
    }
  else if ([locationGroups count] == editing_insertrow)
    {
    // Check potentially inserted last row
    UITableViewCell *cell = [[self tableView] cellForRowAtIndexPath:path];
    UITextView *cellLabel = (UITextView *)[cell viewWithTag:1];
    UISwitch *cellSwitch = (UISwitch *)[cell viewWithTag:2];
    NSString *labelv = cellLabel.text;
    BOOL switchv = cellSwitch.on;
    if ([labelv length]>0)
      {
      // We need to add a new entry
      NSString *newlabel = [NSString stringWithFormat:@"%@,%d",labelv,switchv];
      [locationGroups addObject:newlabel];
      [self savegroups];
      [[self tableView] reloadData];
      }
    else
      {
      // Clean-up unused insert row
      [[self tableView] deleteRowsAtIndexPaths:paths 
                              withRowAnimation:UITableViewRowAnimationTop];
      }
    }
  }

- (void)savegroups
  {
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  
  NSArray *array = [NSArray arrayWithArray:locationGroups];
  NSString *lg = [array componentsJoinedByString:@" "];
  [defaults setObject:lg forKey:@"locationGroups"];
  [defaults synchronize];
  }

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
  {
  // Return the number of sections.
  return 1;
  }

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
  {
  // Return the number of rows in the section.
  if ([self isEditing])
    return [locationGroups count] + 1;
  else
    return [locationGroups count];
  }

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
  {
  static NSString *CellIdentifier = @"GroupCell";
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
  
  // Configure the cell...
  UITextView *cellLabel = (UITextView *)[cell viewWithTag:1];
  UISwitch *cellSwitch = (UISwitch *)[cell viewWithTag:2];
    
  if (indexPath.row < [locationGroups count])
    {
    NSArray *gp = [[locationGroups objectAtIndex:indexPath.row] componentsSeparatedByString:@","];
    [cellLabel setText:[gp objectAtIndex:0]];
    if ([gp count]>=2) [cellSwitch setOn:[[gp objectAtIndex:1] intValue]];
    }
  else
    {
    [cellLabel setText:@""];
    [cellSwitch setOn:NO];
    }

  return cell;
  }

- (UITableViewCellEditingStyle)tableView:(UITableView *)tableView editingStyleForRowAtIndexPath:(NSIndexPath *)indexPath
  {
  if (indexPath.row < [locationGroups count])
    {
    return UITableViewCellEditingStyleDelete;
    }
  else
    {
    return UITableViewCellEditingStyleInsert;
    }
  }

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath
  {
  // Return NO if you do not want the specified item to be editable.
  return YES;
  }

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
  {
  if (editingStyle == UITableViewCellEditingStyleDelete)
    {
    // Delete the row from the data source
    int row = indexPath.row;
    [locationGroups removeObjectAtIndex:row];
    editing_insertrow--;
    [self savegroups];
    [tableView deleteRowsAtIndexPaths:[NSArray arrayWithObject:indexPath] withRowAnimation:UITableViewRowAnimationFade];
    }   
  else if (editingStyle == UITableViewCellEditingStyleInsert)
    {
    // Create a new instance of the appropriate class, insert it into the array, and add a new row to the table view
    }   
  }

// Override to support rearranging the table view.
- (void)tableView:(UITableView *)tableView moveRowAtIndexPath:(NSIndexPath *)fromIndexPath toIndexPath:(NSIndexPath *)toIndexPath
  {
  int fromrow = fromIndexPath.row;
  int torow = toIndexPath.row;
  
  if (fromrow != torow)
    {
    id obj = [locationGroups objectAtIndex:fromrow];
    [locationGroups removeObjectAtIndex:fromrow];
    if (torow >= [locationGroups count])
      {
      [locationGroups addObject:obj];
      }
    else
      {
      [locationGroups insertObject:obj atIndex:torow];
      }
    }
  [self savegroups];
  }

/*
// Override to support conditional rearranging of the table view.
- (BOOL)tableView:(UITableView *)tableView canMoveRowAtIndexPath:(NSIndexPath *)indexPath
{
    // Return NO if you do not want the item to be re-orderable.
    return YES;
}
*/

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
  {
  // Navigation logic may go here. Create and push another view controller.
  /*
    <#DetailViewController#> *detailViewController = [[<#DetailViewController#> alloc] initWithNibName:@"<#Nib name#>" bundle:nil];
    // ...
    // Pass the selected object to the new view controller.
    [self.navigationController pushViewController:detailViewController animated:YES];
  */
  }

- (IBAction)done:(id)sender
  {
  [[ovmsAppDelegate myRef] subscribeGroups];
  [self dismissModalViewControllerAnimated:YES];
  }

- (IBAction)endedit:(id)sender
  {
  UITextView *cellLabel = (UITextView *)sender;
  UITableViewCell *cell = (UITableViewCell*)cellLabel.superview.superview;
  NSIndexPath *indexpath = [[self tableView] indexPathForCell:cell];
  if (indexpath)
    {
    int row = indexpath.row;

    NSString *label = [cellLabel text];
    if (row < [locationGroups count])
      {
      // Editing an existing label
      NSArray *gp = [[locationGroups objectAtIndex:row] componentsSeparatedByString:@","];
      NSString *newlabel = [NSString stringWithFormat:@"%@,%d",label,[[gp objectAtIndex:1] intValue]];
      [locationGroups replaceObjectAtIndex:row withObject:newlabel];
      [self savegroups];
      }
    }
  }

- (IBAction)endswitchedit:(id)sender
  {
  UISwitch *cellSwitch = (UISwitch *)sender;
  UITableViewCell *cell = (UITableViewCell*)cellSwitch.superview.superview;
  NSIndexPath *indexpath = [[self tableView] indexPathForCell:cell];
  if (indexpath)
    {
    int row = indexpath.row;
    
    BOOL switchv = cellSwitch.on;
    if (row < [locationGroups count])
      {
      // Editing an existing label
      NSArray *gp = [[locationGroups objectAtIndex:row] componentsSeparatedByString:@","];
      NSString *newlabel = [NSString stringWithFormat:@"%@,%d",[gp objectAtIndex:0],switchv];
      [locationGroups replaceObjectAtIndex:row withObject:newlabel];
      [self savegroups];
      }
    }  
  }

#pragma mark -
#pragma mark UITextField Delegate

-(BOOL) textFieldShouldReturn:(UITextField*) textField
{
  [textField resignFirstResponder]; 
  return YES;
}

@end
