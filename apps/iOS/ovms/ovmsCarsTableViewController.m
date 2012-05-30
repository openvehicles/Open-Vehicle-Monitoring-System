//
//  ovmsCarsTableViewController.m
//  ovms
//
//  Created by Mark Webb-Johnson on 23/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import "ovmsCarsTableViewController.h"
#import "ovmsCarsFormViewController.h"
#import "Cars.h"

@implementation ovmsCarsTableViewController

@synthesize cars = _cars;
@synthesize context = _context;

- (id)initWithStyle:(UITableViewStyle)style
{
    self = [super initWithStyle:style];
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

- (void)viewDidLoad
{
    [super viewDidLoad];

    // Uncomment the following line to preserve selection between presentations.
    self.clearsSelectionOnViewWillAppear = NO;
 
    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    self.navigationItem.rightBarButtonItem = self.editButtonItem;
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];

  int originalcount = [_cars count];
  
  _context = [ovmsAppDelegate myRef].managedObjectContext;
  NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
  NSEntityDescription *entity = [NSEntityDescription 
                                 entityForName:@"Cars" inManagedObjectContext:_context];
  [fetchRequest setEntity:entity];
  NSError *error;
  self.cars = [_context executeFetchRequest:fetchRequest error:&error];

  if (originalcount>0)
    {
    [self.tableView reloadData];
    }

  for (int k=0;k<[_cars count]; k++)
    {
    Cars *car = [_cars objectAtIndex:k];
    NSIndexPath *indexPath = [NSIndexPath indexPathForRow: k inSection: 0];
    UITableViewCell *cell = [self.tableView cellForRowAtIndexPath:indexPath];
    
    UIButton *disclosure = (UIButton*)[cell viewWithTag:3];
    if ([car.vehicleid isEqualToString:[ovmsAppDelegate myRef].sel_car])
      {
      [self.tableView selectRowAtIndexPath:indexPath animated:NO scrollPosition:UITableViewScrollPositionMiddle];
      disclosure.enabled = YES;
      disclosure.hidden = NO;
      }
    else
      {
      disclosure.enabled = NO;
      disclosure.hidden = YES;
      }
    }
  
  [[ovmsAppDelegate myRef] registerForUpdate:self];
  [self update];
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
  [[ovmsAppDelegate myRef] deregisterFromUpdate:self];
}

- (void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:animated];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
  {
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
    return YES;
  else
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
  }

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
  if ([[segue identifier] isEqualToString:@"editCar"])
    {
    NSIndexPath *indexPath = [self.tableView indexPathForSelectedRow];
    Cars *car = [_cars objectAtIndex:indexPath.row];
    [[segue destinationViewController] setCarEditing:car.vehicleid];
    }
  else if ([[segue identifier] isEqualToString:@"newCar"])
    {
    [[segue destinationViewController] setCarEditing:nil];
    }
}

- (void)update
  {
  BOOL enabled = [ovmsAppDelegate myRef].car_online;

  for (int k=0;k<[_cars count]; k++)
    {
    Cars *car = [_cars objectAtIndex:k];
    NSIndexPath *indexPath = [NSIndexPath indexPathForRow: k inSection: 0];
    UITableViewCell *cell = [self.tableView cellForRowAtIndexPath:indexPath];
    UIButton *disclosure = (UIButton*)[cell viewWithTag:3];
    UIImageView *iview = (UIImageView*)[cell viewWithTag:8];
    UIButton *info = (UIButton*)[cell viewWithTag:5];
    if ([car.vehicleid isEqualToString:[ovmsAppDelegate myRef].sel_car])
      {
      iview.hidden = !enabled;
      info.hidden = !enabled;
      info.enabled = enabled;
      info.highlighted = NO;
      
      int car_gsmlevel = [ovmsAppDelegate myRef].car_gsmlevel;
      int car_gsmdbm = 0;
      if (car_gsmlevel <= 31)
        car_gsmdbm = -113 + (car_gsmlevel*2);
      
      int car_signalbars = 0;
      if ((car_gsmdbm < -121)||(car_gsmdbm >= 0))
        car_signalbars = 0;
      else if (car_gsmdbm < -107)
        car_signalbars = 1;
      else if (car_gsmdbm < -98)
        car_signalbars = 2;
      else if (car_gsmdbm < -87)
        car_signalbars = 3;
      else if (car_gsmdbm < -76)
        car_signalbars = 4;
      else
        car_signalbars = 5;
      
      iview.image = [UIImage imageNamed:[NSString stringWithFormat:@"signalbars-%d.png",car_signalbars]];      

      disclosure.enabled = YES;
      disclosure.hidden = NO;
      }
    else
      {
      iview.hidden = YES;
      info.hidden = YES;
      info.enabled = NO;
      info.highlighted = NO;
      disclosure.enabled = NO;
      disclosure.hidden = YES;
      }
    }
  }

- (void)setEditing:(BOOL)editing animated:(BOOL)animated
  {
  if (editing)
    {
    // Disable disclosure
    for (int k=0;k<[_cars count]; k++)
      {
      Cars *car = [_cars objectAtIndex:k];
      NSIndexPath *indexPath = [NSIndexPath indexPathForRow: k inSection: 0];
      UITableViewCell *cell = [self.tableView cellForRowAtIndexPath:indexPath];
      UIButton *disclosure = (UIButton*)[cell viewWithTag:3];
      UIButton *info = (UIButton*)[cell viewWithTag:5];
      UIImageView *iview = (UIImageView*)[cell viewWithTag:8];
      if ([car.vehicleid isEqualToString:[ovmsAppDelegate myRef].sel_car])
        {
        disclosure.enabled = NO;
        disclosure.hidden = YES;
        info.enabled = NO;
        info.hidden = YES;
        iview.hidden = YES;
        }
      }
    [super setEditing:editing animated:animated];
    }
  else
    {
    // Re-select the row
    [super setEditing:editing animated:animated];
    [self update];
    }
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
   return [_cars count];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *CellIdentifier = @"CellIdentifier";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier];
    }

  // Retrieve the relevant car record
  Cars *car = [_cars objectAtIndex:indexPath.row];

  // Get the cell label using its tag and set it
  UILabel *cellLabel = (UILabel *)[cell viewWithTag:1];
  [cellLabel setText:car.label];
  
  // get the cell imageview using its tag and set it
  UIImageView *cellImage = (UIImageView *)[cell viewWithTag:2];
  [cellImage setImage:[UIImage imageNamed:[NSString stringWithFormat:car.imagepath, indexPath.row]]];

  // Init the other parts
  UIButton *disclosure = (UIButton*)[cell viewWithTag:3];
  disclosure.enabled = NO;
  disclosure.hidden = YES;
  UIImageView *iview = (UIImageView*)[cell viewWithTag:8];
  iview.hidden = YES;
  UIButton *info = (UIButton*)[cell viewWithTag:5];
  info.hidden = YES;
  info.enabled = NO;
  info.highlighted = NO;

  return cell;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
  {
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
    return 200;
  else
    return 144;
  }

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (editingStyle == UITableViewCellEditingStyleDelete)
      {
      // Delete the row from the data source
      if ([_cars count]==1) return; // Can't delete the last car
      Cars *car = [_cars objectAtIndex:indexPath.row];
      [_context deleteObject:car];
      NSError *error;
      if (![_context save:&error])
        {
        NSLog(@"Whoops, couldn't save: %@", [error localizedDescription]);
        return;
        }

      // Reload the cars array...
      NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
      NSEntityDescription *entity = [NSEntityDescription 
                                     entityForName:@"Cars" inManagedObjectContext:_context];
      [fetchRequest setEntity:entity];
      self.cars = [_context executeFetchRequest:fetchRequest error:&error];

      [tableView deleteRowsAtIndexPaths:[NSArray arrayWithObject:indexPath] withRowAnimation:UITableViewRowAnimationFade];
      }   
    else if (editingStyle == UITableViewCellEditingStyleInsert)
      {
        // Create a new instance of the appropriate class, insert it into the array, and add a new row to the table view
      }   
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  Cars* car = [_cars objectAtIndex:indexPath.row];
  if (! [[ovmsAppDelegate myRef].sel_car isEqualToString:car.vehicleid])
    {
    // Switch the car
    [[ovmsAppDelegate myRef] switchCar:car.vehicleid];
    }
  for (int k=0;k<[_cars count]; k++)
    {
    Cars *car = [_cars objectAtIndex:k];
    NSIndexPath *indexPath = [NSIndexPath indexPathForRow: k inSection: 0];
    UITableViewCell *cell = [self.tableView cellForRowAtIndexPath:indexPath];
    UIButton *disclosure = (UIButton*)[cell viewWithTag:3];
    UIButton *info = (UIButton*)[cell viewWithTag:5];
    if ([car.vehicleid isEqualToString:[ovmsAppDelegate myRef].sel_car])
      {
      [self.tableView selectRowAtIndexPath:indexPath animated:NO scrollPosition:UITableViewScrollPositionMiddle];
      disclosure.enabled = YES;
      disclosure.hidden = NO;
      info.highlighted = NO;
      }
    else
      {
      disclosure.enabled = NO;
      disclosure.hidden = YES;
      }
    }
}

@end
